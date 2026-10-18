#include "Contracts.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonParseError>
#include <QtCore/QSet>
#include <QtCore/QUuid>
#include <cmath>

namespace ycc {
namespace {

// QJsonDocument accepts duplicate object keys. Check the token structure before
// conversion so a model cannot make two validators interpret a command differently.
class KeyScanner
{
public:
    explicit KeyScanner(const QByteArray& data) : _data(data) {}

    bool check() { return _value(0) && (_space(), _position == _data.size()); }

private:
    void _space()
    {
        while (_position < _data.size() && QByteArray(" \r\n\t").contains(_data[_position])) {
            ++_position;
        }
    }

    bool _string(QString* decoded = nullptr)
    {
        const auto start = _position++;
        while (_position < _data.size()) {
            const char c = _data[_position++];
            if (c == '\\') {
                ++_position;
            } else if (c == '"') {
                if (decoded) {
                    const auto token = _data.mid(start, _position - start);
                    *decoded = QJsonDocument::fromJson("[" + token + "]").array().first().toString();
                }
                return _position - start <= 65536;
            }
        }
        return false;
    }

    bool _value(int depth)
    {
        _space();
        if (depth > 32 || _position >= _data.size()) {
            return false;
        }
        const char c = _data[_position];
        if (c == '"') {
            return _string();
        }
        if (c == '{' || c == '[') {
            const bool object = c == '{';
            const char end = object ? '}' : ']';
            ++_position;
            _space();
            if (_position < _data.size() && _data[_position] == end) {
                ++_position;
                return true;
            }
            QSet<QString> keys;
            int count = 0;
            while (_position < _data.size()) {
                if (++count > 512) {
                    return false;
                }
                _space();
                if (object) {
                    QString key;
                    if (_data[_position] != '"' || !_string(&key) || keys.contains(key)) {
                        return false;
                    }
                    keys.insert(key);
                    _space();
                    if (_position >= _data.size() || _data[_position++] != ':') {
                        return false;
                    }
                }
                if (!_value(depth + 1)) {
                    return false;
                }
                _space();
                if (_position >= _data.size()) {
                    return false;
                }
                const char separator = _data[_position++];
                if (separator == end) {
                    return true;
                }
                if (separator != ',') {
                    return false;
                }
            }
            return false;
        }
        const auto start = _position;
        while (_position < _data.size() && !QByteArray(",]} \t\r\n").contains(_data[_position])) {
            ++_position;
        }
        return _position > start;
    }

    const QByteArray& _data;
    qsizetype _position = 0;
};

bool text(const QJsonValue& value, int max = 80)
{
    return value.isString() && !value.toString().trimmed().isEmpty() && value.toString().size() <= max;
}
}  // namespace

QJsonObject Error::json() const
{
    return {{"code", code}, {"message", message}};
}

bool isId(const QString& value)
{
    return value.size() == 36 && !QUuid(value).isNull() && QUuid(value).toString(QUuid::WithoutBraces) == value;
}

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QByteArray canonical(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QString digest(const QJsonObject& object)
{
    return QString::fromLatin1(QCryptographicHash::hash(canonical(object), QCryptographicHash::Sha256).toHex());
}

Error exactKeys(const QJsonObject& object, const QStringList& keys)
{
    QSet<QString> expected(keys.begin(), keys.end());
    const auto actualKeys = object.keys();
    if (QSet<QString>(actualKeys.begin(), actualKeys.end()) != expected) {
        return {"INVALID_SCHEMA", "Missing or unexpected fields."};
    }
    return {};
}

Error parseObject(const QByteArray& bytes, QJsonObject& object, qsizetype limit)
{
    if (bytes.size() > limit) {
        return {"INVALID_SCHEMA", "Message exceeds the byte limit."};
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject() || !KeyScanner(bytes).check()) {
        return {"INVALID_SCHEMA", "Expected bounded JSON object with unique keys."};
    }
    object = document.object();
    return {};
}

QJsonObject success(const QJsonObject& value)
{
    return {{"ok", true}, {"value", value}};
}

QJsonObject failure(const Error& error)
{
    return {{"ok", false}, {"error", error.json()}};
}

QJsonObject Profile::json() const
{
    QJsonArray list;
    for (const auto& device : devices) {
        list.append(QJsonObject{{"deviceId", device.id},
                                {"displayName", device.name},
                                {"type", "gripper"},
                                {"adapter", "sim.gripper.v1"},
                                {"binding", device.binding},
                                {"unit", "normalized"},
                                {"minimum", device.minimum},
                                {"maximum", device.maximum},
                                {"openPosition", device.openPosition},
                                {"closePosition", device.closePosition}});
    }
    return {{"schemaVersion", 2},   {"profileId", id},         {"displayName", name},
            {"revision", revision}, {"backend", "simulation"}, {"devices", list}};
}

Error Profile::parse(const QJsonObject& object, Profile& profile)
{
    auto error = exactKeys(object, {"schemaVersion", "profileId", "displayName", "revision", "backend", "devices"});
    if (!error.empty()) {
        return error;
    }
    if (object["schemaVersion"] != QJsonValue(2) || object["backend"] != QJsonValue("simulation")) {
        return {"UNSUPPORTED",
                "This adapter accepts schema 2 simulation profiles only; no physical dispatch is installed."};
    }
    const auto revision = object["revision"];
    if (!isId(object["profileId"].toString()) || !text(object["displayName"]) || !revision.isDouble() ||
        revision.toDouble() < 0 || revision.toDouble() > 1000000000 ||
        std::floor(revision.toDouble()) != revision.toDouble() || !object["devices"].isArray() ||
        object["devices"].toArray().size() > 16) {
        return {"INVALID_SCHEMA", "Invalid profile identity, name, revision or devices."};
    }
    Profile candidate;
    candidate.id = object["profileId"].toString();
    candidate.name = object["displayName"].toString();
    candidate.revision = revision.toInt();
    QSet<QString> ids;
    QSet<QString> bindings;
    for (const auto& value : object["devices"].toArray()) {
        if (!value.isObject()) {
            return {"INVALID_SCHEMA", "Device must be an object."};
        }
        const auto item = value.toObject();
        error = exactKeys(item, {"deviceId", "displayName", "type", "adapter", "binding", "unit", "minimum", "maximum",
                                 "openPosition", "closePosition"});
        if (!error.empty()) {
            return error;
        }
        Device device;
        device.id = item["deviceId"].toString();
        device.name = item["displayName"].toString();
        device.binding = item["binding"].toString();
        if (!isId(device.id) || !text(item["displayName"]) || !isId(device.binding) || ids.contains(device.id) ||
            bindings.contains(device.binding)) {
            return {"BINDING_CONFLICT", "Each device needs a unique UUID and simulated binding UUID."};
        }
        if (item["type"] != QJsonValue("gripper") || item["adapter"] != QJsonValue("sim.gripper.v1") ||
            item["unit"] != QJsonValue("normalized")) {
            return {"UNSUPPORTED", "Device type, adapter or unit is not supported."};
        }
        for (const auto& key : {"minimum", "maximum", "openPosition", "closePosition"}) {
            if (!item[key].isDouble() || !std::isfinite(item[key].toDouble())) {
                return {"INVALID_SCHEMA", "Device positions must be finite numbers."};
            }
        }
        device.minimum = item["minimum"].toDouble();
        device.maximum = item["maximum"].toDouble();
        device.openPosition = item["openPosition"].toDouble();
        device.closePosition = item["closePosition"].toDouble();
        if (device.minimum < 0 || device.maximum > 1 || device.minimum >= device.maximum ||
            device.openPosition < device.minimum || device.openPosition > device.maximum ||
            device.closePosition < device.minimum || device.closePosition > device.maximum ||
            device.openPosition == device.closePosition) {
            return {"INVALID_SCHEMA", "Positions must be distinct and inside the normalized calibrated range."};
        }
        ids.insert(device.id);
        bindings.insert(device.binding);
        candidate.devices.append(device);
    }
    profile = candidate;
    return {};
}
}  // namespace ycc

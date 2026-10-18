#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QVector>

namespace ycc {

struct Error
{
    QString code;
    QString message;

    [[nodiscard]] bool empty() const { return code.isEmpty(); }

    [[nodiscard]] QJsonObject json() const;
};

struct Device
{
    QString id;
    QString name;
    QString binding;
    double openPosition = 1;
    double closePosition = 0;
    double minimum = 0;
    double maximum = 1;
};

struct Profile
{
    QString id;
    QString name;
    int revision = 0;
    QVector<Device> devices;
    [[nodiscard]] QJsonObject json() const;
    [[nodiscard]] static Error parse(const QJsonObject& object, Profile& profile);
};

[[nodiscard]] bool isId(const QString& value);
[[nodiscard]] QString newId();
[[nodiscard]] QByteArray canonical(const QJsonObject& object);
[[nodiscard]] QString digest(const QJsonObject& object);
[[nodiscard]] Error exactKeys(const QJsonObject& object, const QStringList& keys);
[[nodiscard]] Error parseObject(const QByteArray& bytes, QJsonObject& object, qsizetype limit = 262144);
[[nodiscard]] QJsonObject success(const QJsonObject& value = {});
[[nodiscard]] QJsonObject failure(const Error& error);

}  // namespace ycc

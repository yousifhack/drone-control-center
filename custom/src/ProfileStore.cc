#include "ProfileStore.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QtQml/qqml.h>

namespace {

bool hasText(const QJsonValue& value)
{
    return value.isString()
        && !value.toString().trimmed().isEmpty();
}

QString validateExample(const QJsonObject& profile)
{
    const auto version = profile.value("schemaVersion");

    if (!version.isDouble() || version.toDouble() != 1.0) {
        return QStringLiteral("Only schema version 1 is supported.");
    }

    if (!hasText(profile.value("profileId"))
        || !hasText(profile.value("displayName"))) {
        return QStringLiteral("Profile ID and display name are required.");
    }

    if (!profile.value("developmentOnly").isBool()
        || !profile.value("developmentOnly").toBool()
        || !profile.value("vehicleIdentity").isNull()) {
        return QStringLiteral(
            "Expected a development profile with no vehicle identity.");
    }

    if (profile.value("expectedVehicleClass").toString() != "multirotor") {
        return QStringLiteral("This example loader expects a multirotor.");
    }

    if (!profile.value("accessories").isArray()) {
        return QStringLiteral("Accessories must be a list.");
    }

    QSet<QString> accessoryIds;
    QSet<QString> actionIds;

    const auto accessories = profile.value("accessories").toArray();

    for (const auto& entry : accessories) {
        if (!entry.isObject()) {
            return QStringLiteral("Each accessory must be an object.");
        }

        const auto accessory = entry.toObject();

        if (!hasText(accessory.value("accessoryId"))
            || !hasText(accessory.value("displayName"))) {
            return QStringLiteral("Accessory ID and name are required.");
        }

        const QString accessoryId =
            accessory.value("accessoryId").toString();

        if (accessoryIds.contains(accessoryId)) {
            return QStringLiteral("Duplicate accessory ID: ") + accessoryId;
        }

        accessoryIds.insert(accessoryId);

        if (accessory.value("type").toString() != "gripper"
            || accessory.value("verificationState").toString() != "unverified"
            || !accessory.value("hardwareBinding").isNull()
            || !accessory.value("feedbackBinding").isNull()) {
            return QStringLiteral(
                "Expected an unverified gripper with no hardware bindings.");
        }

        if (!accessory.value("actions").isArray()) {
            return QStringLiteral("Accessory actions must be a list.");
        }

        const auto actions = accessory.value("actions").toArray();

        for (const auto& actionEntry : actions) {
            if (!actionEntry.isObject()) {
                return QStringLiteral("Each action must be an object.");
            }

            const auto action = actionEntry.toObject();
            const QString actionId = action.value("actionId").toString();

            if ((actionId != "gripper.open" && actionId != "gripper.close")
                || !hasText(action.value("displayName"))) {
                return QStringLiteral("Unsupported or unnamed example action.");
            }

            if (actionIds.contains(actionId)) {
                return QStringLiteral("Duplicate action ID: ") + actionId;
            }

            actionIds.insert(actionId);
        }
    }

    return {};
}

} // namespace

ProfileStore::ProfileStore(QObject* parent)
    : QObject(parent)
{
}

bool ProfileStore::loadExample(const QString& fileName)
{
    // Failed loading must not leave the previous profile selected.
    m_profile.clear();
    m_error.clear();

    const auto fail = [this](const QString& message) {
        m_error = message;
        emit stateChanged();
        return false;
    };

    // Only these bundled development examples may be loaded here.
    if (fileName != "basic-aircraft.json"
        && fileName != "aircraft-with-gripper.json") {
        return fail(QStringLiteral("Unknown example profile."));
    }

    QFile file(QStringLiteral(":/control-center/profiles/") + fileName);

    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("Cannot open profile: ") + file.errorString());
    }

    QJsonParseError parseError;
    const auto document =
        QJsonDocument::fromJson(file.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return fail(QStringLiteral("Invalid JSON: ") + parseError.errorString());
    }

    if (!document.isObject()) {
        return fail(QStringLiteral("The profile must contain a JSON object."));
    }

    const auto object = document.object();
    const QString validationError = validateExample(object);

    if (!validationError.isEmpty()) {
        return fail(validationError);
    }

    m_profile = object.toVariantMap();

    emit stateChanged();
    return true;
}

static void registerControlCenterTypes()
{
    qmlRegisterSingletonType<ProfileStore>(
        "Yousif.ControlCenter", 1, 0, "ProfileStore",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            return new ProfileStore;
        });
}

Q_COREAPP_STARTUP_FUNCTION(registerControlCenterTypes)

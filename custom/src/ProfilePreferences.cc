#include "ProfilePreferences.h"

#include <QRegularExpression>
#include <QSettings>

namespace {

bool validProfileId(const QString& profileId)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9_-]{1,100}$"));

    return pattern.match(profileId).hasMatch();
}

QString settingsGroup(const QString& profileId)
{
    return QStringLiteral("profilePreferences/v1/") + profileId;
}

} // namespace

ProfilePreferences::ProfilePreferences(QObject* parent)
    : QObject(parent)
{
}

QVariantMap ProfilePreferences::read(const QString& profileId) const
{
    QVariantMap result;
    result.insert("displayName", QString());
    result.insert("showAccessoryControls", true);
    result.insert("error", QString());

    if (!validProfileId(profileId)) {
        result.insert("error", QStringLiteral("Invalid profile ID."));
        return result;
    }

    QSettings settings(
        QSettings::IniFormat,
        QSettings::UserScope,
        QStringLiteral("YousifControlCenter"),
        QStringLiteral("ProfilePreferences"));

    settings.setFallbacksEnabled(false);
    settings.beginGroup(settingsGroup(profileId));

    result.insert(
        "displayName",
        settings.value("displayName", QString()).toString());

    result.insert(
        "showAccessoryControls",
        settings.value("showAccessoryControls", true).toBool());

    if (settings.status() != QSettings::NoError) {
        result.insert(
            "error",
            QStringLiteral("Could not read saved profile preferences."));
    }

    return result;
}

QString ProfilePreferences::save(
    const QString& profileId,
    const QString& displayName,
    bool showAccessoryControls)
{
    if (!validProfileId(profileId)) {
        return QStringLiteral("Invalid profile ID.");
    }

    const QString cleanName = displayName.trimmed();

    if (cleanName.size() > 80) {
        return QStringLiteral("The display name must be at most 80 characters.");
    }

    if (cleanName.contains('\n') || cleanName.contains('\r')) {
        return QStringLiteral("The display name must be a single line.");
    }

    QSettings settings(
        QSettings::IniFormat,
        QSettings::UserScope,
        QStringLiteral("YousifControlCenter"),
        QStringLiteral("ProfilePreferences"));

    settings.setFallbacksEnabled(false);
    settings.beginGroup(settingsGroup(profileId));

    settings.setValue("displayName", cleanName);
    settings.setValue("showAccessoryControls", showAccessoryControls);
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        return QStringLiteral("Could not save profile preferences.");
    }

    ++m_revision;
    emit preferencesChanged();

    return {};
}

QString ProfilePreferences::reset(const QString& profileId)
{
    // Saving the default values restores the default behavior.
    return save(profileId, QString(), true);
}

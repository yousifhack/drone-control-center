#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class ProfilePreferences : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int revision READ revision NOTIFY preferencesChanged)

public:
    explicit ProfilePreferences(QObject* parent = nullptr);

    int revision() const { return m_revision; }

    Q_INVOKABLE QVariantMap read(const QString& profileId) const;

    // An empty return string means success.
    // Otherwise, the string explains the error.
    Q_INVOKABLE QString save(
        const QString& profileId,
        const QString& displayName,
        bool showAccessoryControls);

    Q_INVOKABLE QString reset(const QString& profileId);

signals:
    void preferencesChanged();

private:
    int m_revision = 0;
};

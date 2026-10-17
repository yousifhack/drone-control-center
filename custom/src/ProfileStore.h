#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
class ProfileStore : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantMap profile READ profile NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY stateChanged)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY stateChanged)

public:
    explicit ProfileStore(QObject* parent = nullptr);

    QVariantMap profile() const { return m_profile; }
    QString error() const { return m_error; }
    bool loaded() const { return !m_profile.isEmpty(); }
    QVariantList actions() const;

    Q_INVOKABLE bool loadExample(const QString& fileName);

signals:
    void stateChanged();

private:
    QVariantMap m_profile;
    QString m_error;
};

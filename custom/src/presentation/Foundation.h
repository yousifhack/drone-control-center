#pragma once
#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtCore/QVariantMap>
#include <QtQml/QQmlEngine>
#include <QtQmlIntegration/qqmlintegration.h>

namespace ycc {
class FoundationWorker;

/// Stable panel API. The worker, database and inference process live off the UI thread.
class Foundation : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool aiReady READ aiReady NOTIFY aiReadyChanged)
public:
    static Foundation* instance();
    static Foundation* create(QQmlEngine*, QJSEngine*);
    explicit Foundation(QObject* parent = nullptr);
    ~Foundation() override;

    QVariantMap snapshot() const { return _snapshot; }

    bool aiReady() const { return _aiReady; }

    Q_INVOKABLE QString request(const QString& operation, const QVariantMap& arguments = {});
    // C++ read adapter only. Not callable by AI or QML.
    void observeVehicle(const QJsonObject& context);
signals:
    void snapshotChanged();
    void aiReadyChanged();
    void response(const QString& requestId, const QVariantMap& result);
    void aiResult(const QString& requestId, const QVariantMap& result);
    void commandChanged(const QVariantMap& result);

private:
    QThread _thread;
    FoundationWorker* _worker;
    QVariantMap _snapshot;
    bool _aiReady = false;
};
}  // namespace ycc

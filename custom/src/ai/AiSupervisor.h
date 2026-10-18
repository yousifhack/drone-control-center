#pragma once
#include <QtCore/QElapsedTimer>
#include <QtCore/QProcess>
#include <QtCore/QTimer>

#include "application/ControlCore.h"

namespace ycc {
class AiSupervisor : public QObject
{
    Q_OBJECT
public:
    explicit AiSupervisor(ControlCore* core, QObject* parent = nullptr);
    ~AiSupervisor() override;
    QJsonObject start(const QString& python, const QString& script, const QString& endpoint);
    QJsonObject ask(const QString& prompt);
    void stop();

    bool ready() const { return _ready; }

signals:
    void availabilityChanged(bool ready);
    void resultReady(const QString& requestId, const QJsonObject& result);

private:
    void _read();
    void _fail(const QString& code, const QString& message);
    ControlCore* _core;
    QProcess _process;
    QTimer _watchdog;
    QElapsedTimer _heartbeat;
    QElapsedTimer _deadline;
    QByteArray _buffer;
    QString _request;
    QString _session;
    int _revision = 0;
    bool _ready = false;
};
}  // namespace ycc

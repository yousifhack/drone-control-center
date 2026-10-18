#pragma once

#include <QtCore/QHash>

#include "domain/Contracts.h"

namespace ycc {
struct DeviceEvent
{
    QString requestId;
    QString state;
    QString reason;
    QJsonObject evidence;
};

/// Real adapters must implement this contract and their own verified eligibility policy.
class DeviceAdapter
{
public:
    virtual ~DeviceAdapter() = default;
    virtual void start(const QString& requestId, const Device& device, const QString& action, qint64 now) = 0;
    virtual QVector<DeviceEvent> poll(qint64 now) = 0;
    virtual void abandon(const QString& requestId) = 0;
};

class SimulatedGripper final : public DeviceAdapter
{
public:
    enum class Fault
    {
        None,
        Reject,
        NoAcknowledgement,
        NoFeedback
    };

    void setFault(Fault fault) { _fault = fault; }

    void start(const QString& requestId, const Device& device, const QString& action, qint64 now) override;
    QVector<DeviceEvent> poll(qint64 now) override;

    void abandon(const QString& requestId) override { _pending.remove(requestId); }

    [[nodiscard]] int dispatchCount() const { return _dispatchCount; }

private:
    struct Pending
    {
        qint64 started;
        Device device;
        QString action;
        Fault fault;
        bool accepted = false;
    };

    QHash<QString, Pending> _pending;
    Fault _fault = Fault::None;
    int _dispatchCount = 0;
};
}  // namespace ycc

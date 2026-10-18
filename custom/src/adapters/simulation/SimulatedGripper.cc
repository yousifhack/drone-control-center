#include "SimulatedGripper.h"

namespace ycc {
void SimulatedGripper::start(const QString& requestId, const Device& device, const QString& action, qint64 now)
{
    ++_dispatchCount;
    _pending.insert(requestId, {now, device, action, _fault, false});
}

QVector<DeviceEvent> SimulatedGripper::poll(qint64 now)
{
    QVector<DeviceEvent> events;
    for (auto it = _pending.begin(); it != _pending.end();) {
        auto& pending = it.value();
        if (pending.fault == Fault::NoAcknowledgement || now - pending.started < 10) {
            ++it;
            continue;
        }
        if (pending.fault == Fault::Reject) {
            events.append({it.key(), "Failed", "DEVICE_REJECTED", {}});
            it = _pending.erase(it);
            continue;
        }
        if (!pending.accepted) {
            pending.accepted = true;
            events.append({it.key(), "Accepted", {}, {{"type", "simulated-ack"}}});
        }
        if (pending.fault == Fault::NoFeedback || now - pending.started < 50) {
            ++it;
            continue;
        }
        const bool open = pending.action == "gripper.open";
        events.append({it.key(),
                       "Completed",
                       {},
                       {{"type", "simulated-position-feedback"},
                        {"deviceId", pending.device.id},
                        {"position", open ? pending.device.openPosition : pending.device.closePosition},
                        {"unit", "normalized"},
                        {"physicalVerification", false}}});
        it = _pending.erase(it);
    }
    return events;
}
}  // namespace ycc

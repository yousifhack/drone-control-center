#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <functional>

#include "adapters/simulation/SimulatedGripper.h"
#include "configuration/ProfileRepository.h"
#include "recording/EventJournal.h"

namespace ycc {
/// All mutations are serialized on the owning thread. No QGC write adapter is installed.
class ControlCore : public QObject
{
    Q_OBJECT
public:
    explicit ControlCore(const QString& dataDirectory, std::function<qint64()> clock = {}, QObject* parent = nullptr);
    [[nodiscard]] QJsonObject snapshot() const;
    [[nodiscard]] QJsonObject saveProfile(const QJsonObject& draft, int expectedRevision);
    [[nodiscard]] QJsonObject loadProfile(const QString& id) const;
    [[nodiscard]] QJsonObject connectSimulation(const QString& profileId);
    void disconnectSession();
    void setObservedVehicle(const QJsonObject& context);
    [[nodiscard]] QJsonObject propose(const QString& capabilityId, const QJsonObject& arguments = {});
    [[nodiscard]] QJsonObject acceptAiResult(const QJsonObject& result, const QString& expectedSession,
                                             int expectedRevision, const QString& turnId = {});
    [[nodiscard]] QJsonObject approve(const QString& proposalId, const QString& proposalHash);
    [[nodiscard]] QJsonObject execute(const QString& requestId, const QString& proposalId, const QString& approvalId);
    [[nodiscard]] QJsonObject outcome(const QString& requestId);

    [[nodiscard]] QJsonArray events() { return _journal.events(); }

    [[nodiscard]] QJsonObject acquire(const QString& deviceId, const QString& owner, int durationMs = 5000);
    void release(const QString& leaseId);
    void tick();

    void setSimulationFault(SimulatedGripper::Fault fault) { _simulator.setFault(fault); }

    [[nodiscard]] int dispatchCount() const { return _simulator.dispatchCount(); }

signals:
    void contextChanged(const QJsonObject& snapshot);
    void outcomeChanged(const QJsonObject& outcome);

private:
    struct Proposal
    {
        QJsonObject document;
        Device device;
        QString action;
        qint64 deadline;
        QString approval;
        bool used = false;
    };

    struct Pending
    {
        QJsonObject outcome;
        QString deviceId;
        QString lease;
        qint64 deadline;
    };

    struct Lease
    {
        QString id;
        QString owner;
        qint64 deadline;
    };

    qint64 _now() const;
    void _changed();
    Error _eligible(const Proposal& proposal) const;
    void _finish(const QString& id, const QString& state, const QString& reason, const QJsonObject& evidence = {});
    QJsonArray _capabilities() const;
    QElapsedTimer _elapsed;
    std::function<qint64()> _clock;
    ProfileRepository _profiles;
    EventJournal _journal;
    SimulatedGripper _simulator;
    DeviceAdapter& _adapter = _simulator;
    Profile _profile;
    QString _sessionId;
    QString _backend = "disconnected";
    QJsonObject _observed;
    int _sequence = 0;
    QHash<QString, Proposal> _proposals;
    QHash<QString, Pending> _pending;
    QHash<QString, Lease> _leases;
};
}  // namespace ycc

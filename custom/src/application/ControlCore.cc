#include "ControlCore.h"

#include <QtCore/QDir>

namespace ycc {
ControlCore::ControlCore(const QString& directory, std::function<qint64()> clock, QObject* parent)
    : QObject(parent),
      _clock(std::move(clock)),
      _profiles(QDir(directory).filePath("profiles")),
      _journal(QDir(directory).filePath("events.sqlite"))
{
    _elapsed.start();
    const bool recovered = _journal.recover();
    Q_UNUSED(recovered)
}

qint64 ControlCore::_now() const
{
    return _clock ? _clock() : _elapsed.elapsed();
}

void ControlCore::_changed()
{
    ++_sequence;
    emit contextChanged(snapshot());
}

QJsonArray ControlCore::_capabilities() const
{
    QJsonArray result;
    if (_backend != "simulation") {
        return result;
    }
    for (const auto& device : _profile.devices) {
        for (const auto& action : {QStringLiteral("gripper.open"), QStringLiteral("gripper.close")}) {
            const QString id = device.id + "/" + action;
            result.append(QJsonObject{{"capabilityId", id},
                                      {"deviceId", device.id},
                                      {"deviceName", device.name},
                                      {"actionType", action},
                                      {"arguments", QJsonObject{}},
                                      {"backend", "simulation"},
                                      {"physicalVerification", false},
                                      {"available", _journal.error().empty()},
                                      {"requiresApproval", true}});
        }
    }
    return result;
}

QJsonObject ControlCore::snapshot() const
{
    return {{"schemaVersion", 1},
            {"sessionId", _sessionId},
            {"backend", _backend},
            {"sequence", _sequence},
            {"profileRevision", _backend == "simulation" ? _profile.revision : 0},
            {"profileId", _backend == "simulation" ? _profile.id : QString()},
            {"displayName", _backend == "simulation" ? _profile.name : _observed["displayName"].toString()},
            {"capabilities", _capabilities()},
            {"observedVehicle", _observed},
            {"storageError", _journal.error().json()},
            {"realHardwareDispatch", false}};
}

QJsonObject ControlCore::saveProfile(const QJsonObject& draft, int expectedRevision)
{
    if (!_pending.isEmpty()) {
        return failure({"BUSY", "Finish or reconcile active requests before changing configuration."});
    }
    Profile profile;
    const auto error = _profiles.save(draft, expectedRevision, profile);
    if (!error.empty()) {
        return failure(error);
    }
    if (_backend == "simulation" && _profile.id == profile.id) {
        _profile = profile;
        _leases.clear();
        _changed();
    }
    return success(profile.json());
}

QJsonObject ControlCore::loadProfile(const QString& id) const
{
    Profile profile;
    const auto error = _profiles.load(id, profile);
    return error.empty() ? success(profile.json()) : failure(error);
}

QJsonObject ControlCore::connectSimulation(const QString& profileId)
{
    Profile profile;
    const auto error = _profiles.load(profileId, profile);
    if (!error.empty()) {
        return failure(error);
    }
    disconnectSession();
    _profile = profile;
    _backend = "simulation";
    _sessionId = newId();
    _changed();
    return success(snapshot());
}

void ControlCore::disconnectSession()
{
    const auto ids = _pending.keys();
    for (const auto& id : ids) {
        _finish(id, "Unknown", "SESSION_CLOSED");
    }
    _leases.clear();
    _proposals.clear();
    _observed = {};
    _backend = "disconnected";
    _sessionId.clear();
    _changed();
}

void ControlCore::setObservedVehicle(const QJsonObject& context)
{
    const auto session = context["sessionId"].toString();
    if (!isId(session)) {
        disconnectSession();
        return;
    }
    if (_sessionId != session || _backend != "qgc-read-only") {
        disconnectSession();
        _backend = "qgc-read-only";
        _sessionId = session;
    }
    _observed = context;
    _changed();
}

Error ControlCore::_eligible(const Proposal& proposal) const
{
    if (!_journal.error().empty()) {
        return _journal.error();
    }
    if (_backend != "simulation" || _sessionId != proposal.document["sessionId"].toString()) {
        return {"WRONG_SESSION", "Proposal does not target the current simulated session."};
    }
    if (_profile.revision != proposal.document["profileRevision"].toInt()) {
        return {"REVISION_CONFLICT", "Configuration changed after the proposal was created."};
    }
    if (_now() >= proposal.deadline) {
        return {"STALE_CONTEXT", "Proposal expired; request a new preview."};
    }
    return {};
}

QJsonObject ControlCore::propose(const QString& capabilityId, const QJsonObject& arguments)
{
    tick();
    if (!arguments.isEmpty()) {
        return failure({"INVALID_SCHEMA", "Open/close accept no arguments; endpoint values come from configuration."});
    }
    if (_proposals.size() >= 128) {
        return failure({"BUSY", "Too many pending proposals; wait for expiry."});
    }
    for (const auto& device : _profile.devices) {
        for (const auto& action : {QStringLiteral("gripper.open"), QStringLiteral("gripper.close")}) {
            if (capabilityId != device.id + "/" + action) {
                continue;
            }
            const QString id = newId();
            QJsonObject document{{"proposalId", id},
                                 {"sessionId", _sessionId},
                                 {"profileRevision", _profile.revision},
                                 {"capabilityId", capabilityId},
                                 {"arguments", arguments},
                                 {"validForMs", 30000},
                                 {"backend", "simulation"},
                                 {"source", "operator"},
                                 {"turnId", ""}};
            document.insert("proposalHash", digest(document));
            Proposal proposal{document, device, action, _now() + 30000, {}, false};
            const auto error = _eligible(proposal);
            if (!error.empty()) {
                return failure(error);
            }
            _proposals.insert(id, proposal);
            return success(document);
        }
    }
    return failure({"UNKNOWN_CAPABILITY", "The selected session does not expose this capability."});
}

QJsonObject ControlCore::acceptAiResult(const QJsonObject& result, const QString& expectedSession, int expectedRevision,
                                        const QString& turnId)
{
    if (_sessionId != expectedSession || (_backend == "simulation" ? _profile.revision : 0) != expectedRevision) {
        return failure({"STALE_CONTEXT", "Vehicle or configuration changed during inference."});
    }
    const auto kind = result["kind"].toString();
    if (kind == "actionProposal") {
        const auto error = exactKeys(result, {"kind", "capabilityId", "arguments"});
        if (!error.empty() || !result["capabilityId"].isString() || !result["arguments"].isObject()) {
            return failure({"INVALID_SCHEMA", "Invalid model action proposal."});
        }
        const auto proposed = propose(result["capabilityId"].toString(), result["arguments"].toObject());
        if (!proposed["ok"].toBool())
            return proposed;
        auto& proposal = _proposals[proposed["value"].toObject()["proposalId"].toString()];
        proposal.document.remove("proposalHash");
        proposal.document["source"] = "ai";
        proposal.document["turnId"] = turnId;
        proposal.document["proposalHash"] = digest(proposal.document);
        return success(proposal.document);
    }
    if (!QStringList{"answer", "clarification", "unsupported"}.contains(kind) ||
        !exactKeys(result, {"kind", "text"}).empty() || !result["text"].isString() ||
        result["text"].toString().trimmed().isEmpty() || result["text"].toString().size() > 8192) {
        return failure({"INVALID_SCHEMA", "Unknown or malformed model result."});
    }
    return success(QJsonObject{{"kind", kind},
                               {"text", result["text"]},
                               {"generated", true},
                               {"verifiedFact", false},
                               {"turnId", turnId},
                               {"executed", false}});
}

QJsonObject ControlCore::approve(const QString& proposalId, const QString& proposalHash)
{
    auto it = _proposals.find(proposalId);
    if (it == _proposals.end() || it->document["proposalHash"].toString() != proposalHash || it->used) {
        return failure({"INVALID_APPROVAL", "Proposal is missing, modified, or already used."});
    }
    const auto error = _eligible(*it);
    if (!error.empty()) {
        return failure(error);
    }
    it->approval = newId();
    return success({{"approvalId", it->approval}});
}

QJsonObject ControlCore::acquire(const QString& deviceId, const QString& owner, int durationMs)
{
    if (_backend != "simulation" || owner.isEmpty() || owner.size() > 80 || durationMs < 100 || durationMs > 10000) {
        return failure({"NO_AUTHORITY", "Invalid simulated resource lease."});
    }
    bool found = false;
    for (const auto& device : _profile.devices) {
        found |= device.id == deviceId;
    }
    if (!found) {
        return failure({"UNKNOWN_CAPABILITY", "Device is not configured."});
    }
    if (_leases.contains(deviceId) && _leases[deviceId].deadline > _now()) {
        return failure({"BUSY", "Resource has an active control owner."});
    }
    const Lease lease{newId(), owner, _now() + durationMs};
    _leases.insert(deviceId, lease);
    return success({{"leaseId", lease.id}, {"owner", lease.owner}});
}

void ControlCore::release(const QString& leaseId)
{
    for (auto it = _leases.begin(); it != _leases.end();) {
        if (it->id == leaseId) {
            it = _leases.erase(it);
        } else {
            ++it;
        }
    }
}

QJsonObject ControlCore::execute(const QString& requestId, const QString& proposalId, const QString& approvalId)
{
    if (!isId(requestId)) {
        return failure({"INVALID_SCHEMA", "Request ID must be a UUID."});
    }
    const QString fingerprint = digest({{"proposalId", proposalId}, {"approvalId", approvalId}});
    const auto previous = _journal.find(requestId);
    if (!previous.isEmpty()) {
        return previous["fingerprint"].toString() == fingerprint
                   ? success(previous["outcome"].toObject())
                   : failure({"DUPLICATE_CONFLICT", "Request ID was already used with a different payload."});
    }
    auto it = _proposals.find(proposalId);
    if (it == _proposals.end() || it->approval.isEmpty() || it->approval != approvalId || it->used) {
        return failure({"INVALID_APPROVAL", "An unused operator approval for this proposal is required."});
    }
    const auto error = _eligible(*it);
    if (!error.empty()) {
        return failure(error);
    }
    for (const auto& pending : _pending) {
        if (pending.deviceId == it->device.id) {
            return failure({"BUSY", "This device has an unresolved operation."});
        }
    }
    const auto leaseResult = acquire(it->device.id, "operator-approved-request");
    if (!leaseResult["ok"].toBool()) {
        return leaseResult;
    }
    const QString lease = leaseResult["value"].toObject()["leaseId"].toString();
    QJsonObject current{{"requestId", requestId},
                        {"proposalId", proposalId},
                        {"sessionId", _sessionId},
                        {"profileRevision", _profile.revision},
                        {"capabilityId", it->document["capabilityId"]},
                        {"state", "Dispatched"},
                        {"reason", ""},
                        {"backend", "simulation"},
                        {"source", it->document["source"]},
                        {"turnId", it->document["turnId"]},
                        {"evidence", QJsonObject{}}};
    if (!_journal.beginRequest(requestId, fingerprint, current)) {
        release(lease);
        return failure({"STORAGE_FAILURE", "Cannot journal dispatch; nothing was sent."});
    }
    it->used = true;
    _pending.insert(requestId, {current, it->device.id, lease, _now() + 2000});
    _adapter.start(requestId, it->device, it->action, _now());
    emit outcomeChanged(current);
    return success(current);
}

void ControlCore::_finish(const QString& id, const QString& state, const QString& reason, const QJsonObject& evidence)
{
    auto it = _pending.find(id);
    if (it == _pending.end()) {
        return;
    }
    auto result = it->outcome;
    result["state"] = state;
    result["reason"] = reason;
    result["evidence"] = evidence;
    if (!_journal.update(result)) {
        result["journalError"] = "STORAGE_FAILURE";
    }
    release(it->lease);
    _adapter.abandon(id);
    _pending.erase(it);
    emit outcomeChanged(result);
}

void ControlCore::tick()
{
    const auto now = _now();
    // Deadline/authority checks precede feedback processing: late feedback cannot
    // turn a request with an invalid lease into a successful operation.
    for (const auto& id : _pending.keys()) {
        const auto& pending = _pending[id];
        const auto lease = _leases.value(pending.deviceId);
        if (lease.id != pending.lease || lease.deadline <= now) {
            _finish(id, "Unknown", "NO_AUTHORITY");
        } else if (pending.deadline <= now) {
            _finish(id, "Unknown",
                    pending.outcome["state"] == QJsonValue("Accepted") ? "FEEDBACK_TIMEOUT" : "ACK_TIMEOUT");
        }
    }
    for (const auto& event : _adapter.poll(now)) {
        auto it = _pending.find(event.requestId);
        if (it == _pending.end()) {
            continue;
        }
        if (event.state == "Accepted") {
            it->outcome["state"] = "Accepted";
            it->outcome["evidence"] = event.evidence;
            if (!_journal.update(it->outcome)) {
                _finish(event.requestId, "Unknown", "STORAGE_FAILURE");
            } else {
                emit outcomeChanged(it->outcome);
            }
        } else {
            _finish(event.requestId, event.state, event.reason, event.evidence);
        }
    }
    for (auto it = _proposals.begin(); it != _proposals.end();) {
        if (it->deadline <= now) {
            it = _proposals.erase(it);
        } else {
            ++it;
        }
    }
}

QJsonObject ControlCore::outcome(const QString& requestId)
{
    const auto record = _journal.find(requestId);
    if (!_journal.error().empty()) {
        return failure(_journal.error());
    }
    return record.isEmpty() ? failure({"NOT_FOUND", "Request is not in this journal."})
                            : success(record["outcome"].toObject());
}
}  // namespace ycc

#include "AiSupervisor.h"

#include <QtCore/QFileInfo>

namespace ycc {
AiSupervisor::AiSupervisor(ControlCore* core, QObject* parent) : QObject(parent), _core(core)
{
    connect(&_process, &QProcess::readyReadStandardOutput, this, &AiSupervisor::_read);
    connect(&_process, &QProcess::readyReadStandardError, this, [this] { _process.readAllStandardError(); });
    connect(&_process, &QProcess::errorOccurred, this, [this] { _fail("AI_PROCESS_ERROR", _process.errorString()); });
    connect(&_process, &QProcess::finished, this, [this] { _fail("AI_EXITED", "Inference worker exited."); });
    _watchdog.setInterval(250);
    connect(&_watchdog, &QTimer::timeout, this, [this] {
        if (_heartbeat.elapsed() > 5000 || (!_request.isEmpty() && _deadline.elapsed() > 100000)) {
            _fail("AI_TIMEOUT", "Inference worker exceeded its heartbeat or response deadline.");
        }
    });
}

AiSupervisor::~AiSupervisor()
{
    stop();
    _process.waitForFinished(1000);
}

QJsonObject AiSupervisor::start(const QString& python, const QString& script, const QString& endpoint)
{
    if (_process.state() != QProcess::NotRunning) {
        return failure({"BUSY", "Stop the current worker before starting another."});
    }
    if (!QFileInfo(python).isAbsolute() || !QFileInfo(python).isExecutable() || !QFileInfo(script).isAbsolute() ||
        !QFileInfo(script).isFile()) {
        return failure({"INVALID_CONFIGURATION", "Use absolute paths to Python and worker.py."});
    }
    _buffer.clear();
    _ready = false;
    _heartbeat.start();
    _watchdog.start();
    _process.setProgram(python);
    _process.setArguments({"-u", script, "--endpoint", endpoint});
    _process.start();
    return success({{"state", "Starting"}});
}

QJsonObject AiSupervisor::ask(const QString& prompt)
{
    if (!_ready)
        return failure({"AI_UNAVAILABLE", "Start a local worker first."});
    if (!_request.isEmpty())
        return failure({"BUSY", "One inference request is already active."});
    if (prompt.trimmed().isEmpty() || prompt.size() > 4096)
        return failure({"INVALID_SCHEMA", "Prompt length must be 1–4096."});
    const auto context = _core->snapshot();
    _request = newId();
    _session = context["sessionId"].toString();
    _revision = context["profileRevision"].toInt();
    const auto bytes =
        canonical(
            {{"version", 1}, {"kind", "infer"}, {"requestId", _request}, {"prompt", prompt}, {"context", context}}) +
        '\n';
    if (bytes.size() > 65536) {
        _request.clear();
        return failure({"INVALID_SCHEMA", "Context exceeds transport limit."});
    }
    _deadline.start();
    if (_process.write(bytes) != bytes.size()) {
        _fail("AI_WRITE_ERROR", "Cannot send inference request.");
        return failure({"AI_WRITE_ERROR", "Request was not sent."});
    }
    return success({{"requestId", _request}});
}

void AiSupervisor::stop()
{
    _fail("AI_CANCELLED", "Inference cancelled; no action was executed.");
}

void AiSupervisor::_fail(const QString& code, const QString& message)
{
    _watchdog.stop();
    _ready = false;
    const auto id = _request;
    _request.clear();
    if (_process.state() != QProcess::NotRunning)
        _process.kill();
    emit availabilityChanged(false);
    if (!id.isEmpty())
        emit resultReady(id, failure({code, message}));
}

void AiSupervisor::_read()
{
    if (!_watchdog.isActive()) {
        _process.readAllStandardOutput();
        return;
    }
    _buffer += _process.readAllStandardOutput();
    if (_buffer.size() > 131072) {
        _fail("AI_PROTOCOL_ERROR", "Worker output exceeded limit.");
        _buffer.clear();
        return;
    }
    while (_buffer.contains('\n')) {
        const auto end = _buffer.indexOf('\n');
        const auto line = _buffer.left(end);
        _buffer.remove(0, end + 1);
        QJsonObject message;
        if (!parseObject(line, message, 65536).empty() || message["version"] != QJsonValue(1)) {
            _fail("AI_PROTOCOL_ERROR", "Invalid worker JSON/version.");
            return;
        }
        const auto kind = message["kind"].toString();
        if (kind == "hello" && !_ready && exactKeys(message, {"version", "kind", "provider", "execution"}).empty() &&
            message["execution"] == QJsonValue(false) && message["provider"] == QJsonValue("llama.cpp")) {
            _heartbeat.restart();
            _ready = true;
            emit availabilityChanged(true);
        } else if (kind == "heartbeat" && _ready && exactKeys(message, {"version", "kind"}).empty()) {
            _heartbeat.restart();
        } else if ((kind == "result" || kind == "error") && _ready && !_request.isEmpty() &&
                   message["requestId"] == QJsonValue(_request)) {
            if (_deadline.elapsed() > 100000) {
                _fail("AI_TIMEOUT", "Result arrived after the deadline.");
                return;
            }
            const QStringList fields = kind == "result" ? QStringList{"version", "kind", "requestId", "result"}
                                                        : QStringList{"version", "kind", "requestId", "message"};
            if (!exactKeys(message, fields).empty()) {
                _fail("AI_PROTOCOL_ERROR", "Unexpected result fields.");
                return;
            }
            const auto id = _request;
            _request.clear();
            const auto result = kind == "result"
                                    ? _core->acceptAiResult(message["result"].toObject(), _session, _revision, id)
                                    : failure({"AI_INFERENCE_ERROR", message["message"].toString().left(1024)});
            emit resultReady(id, result);
        } else {
            _fail("AI_PROTOCOL_ERROR", "Unexpected or uncorrelated worker message.");
            return;
        }
    }
    if (_buffer.size() > 65536) {
        _fail("AI_PROTOCOL_ERROR", "Unterminated oversized frame.");
        _buffer.clear();
    }
}
}  // namespace ycc

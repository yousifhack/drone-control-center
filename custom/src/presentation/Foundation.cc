#include "Foundation.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>

#include "ai/AiSupervisor.h"

namespace ycc {
class FoundationWorker : public QObject
{
    Q_OBJECT
public:
    ControlCore* core = nullptr;
    AiSupervisor* ai = nullptr;
    QJsonObject observed;

    void initialize()
    {
        core = new ControlCore(
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/foundation-v1", {}, this);
        ai = new AiSupervisor(core, this);
        connect(core, &ControlCore::contextChanged, this, &FoundationWorker::context);
        connect(core, &ControlCore::outcomeChanged, this, &FoundationWorker::command);
        connect(ai, &AiSupervisor::availabilityChanged, this, &FoundationWorker::available);
        connect(ai, &AiSupervisor::resultReady, this, &FoundationWorker::inference);
        auto* timer = new QTimer(this);
        timer->setInterval(20);
        connect(timer, &QTimer::timeout, core, &ControlCore::tick);
        timer->start();
        emit context(core->snapshot());
    }

    QJsonObject call(const QString& op, const QJsonObject& a)
    {
        if (!core)
            return failure({"NOT_READY", "Foundation is starting."});
        static const QHash<QString, QStringList> fields{{"snapshot", {}},
                                                        {"events", {}},
                                                        {"profile.save", {"draft", "expectedRevision"}},
                                                        {"profile.load", {"profileId"}},
                                                        {"simulation.connect", {"profileId"}},
                                                        {"session.disconnect", {}},
                                                        {"action.propose", {"capabilityId", "arguments"}},
                                                        {"action.approve", {"proposalId", "proposalHash"}},
                                                        {"action.execute", {"requestId", "proposalId", "approvalId"}},
                                                        {"action.outcome", {"requestId"}},
                                                        {"ai.start", {"python", "script", "endpoint"}},
                                                        {"ai.ask", {"prompt"}},
                                                        {"ai.stop", {}}};
        if (!fields.contains(op))
            return failure({"UNSUPPORTED", "Unknown foundation operation."});
        const auto error = exactKeys(a, fields[op]);
        if (!error.empty())
            return failure(error);
        for (const auto& key : fields[op]) {
            if (key == "draft" || key == "arguments") {
                if (!a[key].isObject())
                    return failure({"INVALID_SCHEMA", "Expected an object: " + key});
            } else if (key != "expectedRevision" && !a[key].isString())
                return failure({"INVALID_SCHEMA", "Expected a string: " + key});
        }
        if (op == "snapshot")
            return success(core->snapshot());
        if (op == "profile.load")
            return core->loadProfile(a["profileId"].toString());
        if (op == "profile.save") {
            if (!a["draft"].isObject() || !a["expectedRevision"].isDouble() ||
                a["expectedRevision"].toDouble() != a["expectedRevision"].toInt())
                return failure({"INVALID_SCHEMA", "Expected a draft and integer revision."});
            return core->saveProfile(a["draft"].toObject(), a["expectedRevision"].toInt());
        }
        if (op == "simulation.connect") {
            if (!observed.isEmpty())
                return failure({"BUSY", "Disconnect the real QGC vehicle before using simulation."});
            return core->connectSimulation(a["profileId"].toString());
        }
        if (op == "session.disconnect") {
            core->disconnectSession();
            return success();
        }
        if (op == "action.propose")
            return core->propose(a["capabilityId"].toString(), a["arguments"].toObject());
        if (op == "action.approve")
            return core->approve(a["proposalId"].toString(), a["proposalHash"].toString());
        if (op == "action.execute")
            return core->execute(a["requestId"].toString(), a["proposalId"].toString(), a["approvalId"].toString());
        if (op == "action.outcome")
            return core->outcome(a["requestId"].toString());
        if (op == "events")
            return success({{"events", core->events()}});
        if (op == "ai.start")
            return ai->start(a["python"].toString(), a["script"].toString(), a["endpoint"].toString());
        if (op == "ai.ask")
            return ai->ask(a["prompt"].toString());
        if (op == "ai.stop") {
            ai->stop();
            return success();
        }
        return failure({"UNSUPPORTED", "Unknown foundation operation."});
    }

signals:
    void context(const QJsonObject& context);
    void command(const QJsonObject& outcome);
    void available(bool ready);
    void inference(const QString& id, const QJsonObject& result);
};

Foundation* Foundation::instance()
{
    static Foundation* service = new Foundation(QCoreApplication::instance());
    return service;
}

Foundation* Foundation::create(QQmlEngine*, QJSEngine*)
{
    auto* service = instance();
    QQmlEngine::setObjectOwnership(service, QQmlEngine::CppOwnership);
    return service;
}

Foundation::Foundation(QObject* parent) : QObject(parent), _worker(new FoundationWorker)
{
    _worker->moveToThread(&_thread);
    connect(&_thread, &QThread::started, _worker, &FoundationWorker::initialize);
    connect(&_thread, &QThread::finished, _worker, &QObject::deleteLater);
    connect(_worker, &FoundationWorker::context, this, [this](const QJsonObject& v) {
        _snapshot = v.toVariantMap();
        emit snapshotChanged();
    });
    connect(_worker, &FoundationWorker::command, this,
            [this](const QJsonObject& v) { emit commandChanged(v.toVariantMap()); });
    connect(_worker, &FoundationWorker::available, this, [this](bool v) {
        _aiReady = v;
        emit aiReadyChanged();
    });
    connect(_worker, &FoundationWorker::inference, this,
            [this](const QString& id, const QJsonObject& v) { emit aiResult(id, v.toVariantMap()); });
    _thread.start();
}

Foundation::~Foundation()
{
    _thread.quit();
    _thread.wait();
}

QString Foundation::request(const QString& operation, const QVariantMap& arguments)
{
    const auto id = newId();
    const auto object = QJsonObject::fromVariantMap(arguments);
    QMetaObject::invokeMethod(
        _worker,
        [this, id, operation, object] {
            const auto result = canonical(object).size() <= 262144
                                    ? _worker->call(operation, object)
                                    : failure({"INVALID_SCHEMA", "Request exceeds size limit."});
            QMetaObject::invokeMethod(
                this, [this, id, result] { emit response(id, result.toVariantMap()); }, Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
    return id;
}

void Foundation::observeVehicle(const QJsonObject& context)
{
    QMetaObject::invokeMethod(
        _worker,
        [this, context] {
            if (!_worker->core)
                return;
            _worker->observed = context;
            if (!context.isEmpty())
                _worker->core->setObservedVehicle(context);
            else if (_worker->core->snapshot()["backend"] == QJsonValue("qgc-read-only"))
                _worker->core->disconnectSession();
        },
        Qt::QueuedConnection);
}
}  // namespace ycc

#include "Foundation.moc"

#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>

#include "ai/AiSupervisor.h"

using namespace ycc;

static void print(const QJsonObject& value)
{
    QTextStream(stdout) << canonical(value) << Qt::endl;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("YccFoundationCLI");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({{"data", "State directory (separate from QGC).", "path"},
                       {"profile", "Schema-2 simulation profile to import.", "file"},
                       {"exercise", "Exercise one simulated gripper through approval, dispatch and feedback."},
                       {"prompt", "Ask the local model; returns a proposal only.", "text"},
                       {"python", "Absolute Python executable.", "file"},
                       {"worker", "Absolute worker.py path.", "file"},
                       {"endpoint", "Local llama-server URL.", "url", "http://127.0.0.1:8080"}});
    parser.process(app);
    const auto directory = parser.isSet("data")
                               ? parser.value("data")
                               : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    ControlCore core(directory);
    if (parser.isSet("profile")) {
        QFile file(parser.value("profile"));
        QJsonObject draft;
        if (!file.open(QIODevice::ReadOnly) || !parseObject(file.read(262145), draft).empty()) {
            print(failure({"INVALID_PROFILE", "Cannot read profile file."}));
            return 2;
        }
        const auto saved = core.saveProfile(draft, draft["revision"].toInt(-1));
        // A pre-existing revision is deliberately not overwritten by rerunning a diagnostic.
        if (!saved["ok"].toBool() && saved["error"].toObject()["code"] != QJsonValue("REVISION_CONFLICT")) {
            print(saved);
            return 2;
        }
        const auto connected = core.connectSimulation(draft["profileId"].toString());
        if (!connected["ok"].toBool()) {
            print(connected);
            return 2;
        }
    }
    print(success(core.snapshot()));
    QTimer tick;
    tick.setInterval(20);
    QObject::connect(&tick, &QTimer::timeout, &core, &ControlCore::tick);
    tick.start();
    if (parser.isSet("exercise")) {
        const auto capabilities = core.snapshot()["capabilities"].toArray();
        if (capabilities.isEmpty()) {
            print(failure({"UNKNOWN_CAPABILITY", "Import a gripper profile first."}));
            return 2;
        }
        const auto proposed = core.propose(capabilities.first().toObject()["capabilityId"].toString());
        print(proposed);
        if (!proposed["ok"].toBool())
            return 2;
        const auto p = proposed["value"].toObject();
        const auto approval = core.approve(p["proposalId"].toString(), p["proposalHash"].toString());
        if (!approval["ok"].toBool()) {
            print(approval);
            return 2;
        }
        QObject::connect(&core, &ControlCore::outcomeChanged, &app, [&](const QJsonObject& r) {
            print(r);
            const auto state = r["state"].toString();
            if (QStringList{"Completed", "Failed", "Unknown"}.contains(state))
                app.exit(state == "Completed" ? 0 : 2);
        });
        const auto dispatched =
            core.execute(newId(), p["proposalId"].toString(), approval["value"].toObject()["approvalId"].toString());
        if (!dispatched["ok"].toBool()) {
            print(dispatched);
            return 2;
        }
        return app.exec();
    }
    if (!parser.isSet("prompt"))
        return 0;
    AiSupervisor ai(&core);
    QObject::connect(&ai, &AiSupervisor::availabilityChanged, &app, [&](bool ready) {
        if (ready) {
            const auto result = ai.ask(parser.value("prompt"));
            print(result);
            if (!result["ok"].toBool())
                app.exit(2);
        } else
            app.exit(2);
    });
    QObject::connect(&ai, &AiSupervisor::resultReady, &app, [&](const QString& id, const QJsonObject& result) {
        print({{"requestId", id}, {"response", result}});
        app.exit(result["ok"].toBool() ? 0 : 2);
    });
    const auto started = ai.start(parser.value("python"), parser.value("worker"), parser.value("endpoint"));
    if (!started["ok"].toBool()) {
        print(started);
        return 2;
    }
    return app.exec();
}

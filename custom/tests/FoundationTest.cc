#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "ai/AiSupervisor.h"
#include "application/ControlCore.h"
#include "presentation/Foundation.h"

Q_IMPORT_QML_PLUGIN(YccFoundationQmlPlugin)

using namespace ycc;

class FoundationTest : public QObject
{
    Q_OBJECT
private:
    static QJsonObject draft()
    {
        Profile p;
        p.id = newId();
        p.name = "Test aircraft";
        p.devices.append({newId(), "Cargo gripper", newId(), 0.9, 0.1, 0, 1});
        return p.json();
    }

    static QJsonObject value(const QJsonObject& r) { return r["value"].toObject(); }

    static QString code(const QJsonObject& r) { return r["error"].toObject()["code"].toString(); }

    static QJsonObject setup(ControlCore& c)
    {
        const auto saved = c.saveProfile(draft(), 0);
        return c.connectSimulation(value(saved)["profileId"].toString());
    }

    static QJsonObject proposal(ControlCore& c)
    {
        const auto cap = c.snapshot()["capabilities"].toArray().first().toObject()["capabilityId"].toString();
        return value(c.propose(cap));
    }

    static QJsonObject dispatch(ControlCore& c, const QString& id)
    {
        const auto p = proposal(c);
        const auto a = value(c.approve(p["proposalId"].toString(), p["proposalHash"].toString()));
        return c.execute(id, p["proposalId"].toString(), a["approvalId"].toString());
    }

private slots:

    void supervisedAi_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<QString>("expected");
        QTest::newRow("proposal") << QString("proposal") << QString();
        QTest::newRow("forged approval") << QString("forged") << QString("INVALID_SCHEMA");
        QTest::newRow("crash") << QString("crash") << QString("AI_PROCESS_ERROR");
        QTest::newRow("oversized") << QString("oversized") << QString("AI_PROTOCOL_ERROR");
        QTest::newRow("changed vehicle") << QString("stale") << QString("STALE_CONTEXT");
    }

    void supervisedAi()
    {
        QFETCH(QString, mode);
        QFETCH(QString, expected);
        QTemporaryDir dir;
        ControlCore c(dir.path());
        QVERIFY(setup(c)["ok"].toBool());
        AiSupervisor ai(&c);
        QSignalSpy results(&ai, &AiSupervisor::resultReady);
        QVERIFY(ai.start(QString::fromUtf8(YCC_TEST_PYTHON), QString::fromUtf8(YCC_TEST_WORKER), mode)["ok"].toBool());
        QTRY_VERIFY_WITH_TIMEOUT(ai.ready(), 3000);
        QVERIFY(ai.ask("Open cargo gripper")["ok"].toBool());
        if (mode == "stale")
            c.disconnectSession();
        QTRY_COMPARE_WITH_TIMEOUT(results.size(), 1, 3000);
        const auto result = results.first().at(1).value<QJsonObject>();
        if (expected.isEmpty())
            QVERIFY(result["ok"].toBool());
        else if (mode == "crash")
            QVERIFY((QStringList{"AI_PROCESS_ERROR", "AI_EXITED"}.contains(code(result))));
        else
            QCOMPARE(code(result), expected);
        QCOMPARE(c.dispatchCount(), 0);
    }

    void revisionInvalidatesApproval()
    {
        QTemporaryDir dir;
        ControlCore c(dir.path());
        auto saved = c.saveProfile(draft(), 0);
        QVERIFY(c.connectSimulation(value(saved)["profileId"].toString())["ok"].toBool());
        const auto p = proposal(c);
        QVERIFY(c.saveProfile(value(saved), 1)["ok"].toBool());
        QCOMPARE(code(c.approve(p["proposalId"].toString(), p["proposalHash"].toString())), "REVISION_CONFLICT");
    }

    void resourceOwnership()
    {
        QTemporaryDir dir;
        ControlCore c(dir.path());
        QVERIFY(setup(c)["ok"].toBool());
        const auto device = c.snapshot()["capabilities"].toArray().first().toObject()["deviceId"].toString();
        auto lease = c.acquire(device, "other-controller");
        QVERIFY(lease["ok"].toBool());
        QCOMPARE(code(dispatch(c, newId())), "BUSY");
        c.release(value(lease)["leaseId"].toString());
        QVERIFY(dispatch(c, newId())["ok"].toBool());
        QCOMPARE(code(dispatch(c, newId())), "BUSY");
    }

    void missingAckAndReject_data()
    {
        QTest::addColumn<int>("fault");
        QTest::addColumn<QString>("state");
        QTest::newRow("ack lost") << int(SimulatedGripper::Fault::NoAcknowledgement) << QString("Unknown");
        QTest::newRow("rejected") << int(SimulatedGripper::Fault::Reject) << QString("Failed");
    }

    void missingAckAndReject()
    {
        QFETCH(int, fault);
        QFETCH(QString, state);
        QTemporaryDir dir;
        qint64 now = 0;
        ControlCore c(dir.path(), [&] { return now; });
        QVERIFY(setup(c)["ok"].toBool());
        c.setSimulationFault(static_cast<SimulatedGripper::Fault>(fault));
        const auto id = newId();
        QVERIFY(dispatch(c, id)["ok"].toBool());
        now = 100;
        c.tick();
        now = 2100;
        c.tick();
        QCOMPARE(value(c.outcome(id))["state"].toString(), state);
    }

    void qmlModuleLoads()
    {
        QStandardPaths::setTestModeEnabled(true);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(
            "import QtQml\nimport Yousif.Foundation 1.0\nQtObject { property var state: Foundation.snapshot }", QUrl());
        QTRY_VERIFY_WITH_TIMEOUT(!component.isLoading(), 3000);
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QTRY_VERIFY_WITH_TIMEOUT(!object->property("state").toMap().isEmpty(), 3000);
        auto* api = Foundation::instance();
        QSignalSpy replies(api, &Foundation::response);
        const auto profile = draft();
        const auto saveId = api->request("profile.save", {{"draft", profile.toVariantMap()}, {"expectedRevision", 0}});
        QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 3000);
        QCOMPARE(replies.first().at(0).toString(), saveId);
        QVERIFY(replies.first().at(1).toMap()["ok"].toBool());
        replies.clear();
        api->request("simulation.connect", {{"profileId", profile["profileId"].toString()}});
        QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 3000);
        QVERIFY(replies.first().at(1).toMap()["ok"].toBool());
        QTRY_COMPARE_WITH_TIMEOUT(api->snapshot()["backend"].toString(), QString("simulation"), 3000);
        replies.clear();
        api->request("action.propose", {{"capabilityId", "invalid"}, {"arguments", 42}});
        QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 3000);
        QCOMPARE(replies.first().at(1).toMap()["error"].toMap()["code"].toString(), QString("INVALID_SCHEMA"));
    }

    void strictJson()
    {
        QJsonObject o;
        QVERIFY(!parseObject(R"({"x":1,"x":2})", o).empty());
        QVERIFY(!parseObject(R"({"x":{"a":1,"\u0061":2}})", o).empty());
        QVERIFY(parseObject(R"({"x":[{"a":1},{"a":2}]})", o).empty());
    }

    void configurationRevision()
    {
        QTemporaryDir dir;
        ControlCore c(dir.path());
        auto d = draft();
        auto saved = c.saveProfile(d, 0);
        QVERIFY(saved["ok"].toBool());
        QCOMPARE(value(saved)["revision"].toInt(), 1);
        QCOMPARE(code(c.saveProfile(d, 0)), "REVISION_CONFLICT");
        auto invalid = value(saved);
        invalid["backend"] = "hardware";
        QVERIFY(!c.saveProfile(invalid, 1)["ok"].toBool());
        QVERIFY(c.saveProfile(value(saved), 1)["ok"].toBool());
    }

    void feedbackAndIdempotency()
    {
        QTemporaryDir dir;
        qint64 now = 0;
        ControlCore c(dir.path(), [&] { return now; });
        QVERIFY(setup(c)["ok"].toBool());
        const auto p = proposal(c);
        const auto id = newId();
        QCOMPARE(code(c.execute(id, p["proposalId"].toString(), "forged")), "INVALID_APPROVAL");
        const auto a = value(c.approve(p["proposalId"].toString(), p["proposalHash"].toString()));
        const auto run = [&] { return c.execute(id, p["proposalId"].toString(), a["approvalId"].toString()); };
        QCOMPARE(value(run())["state"].toString(), "Dispatched");
        QVERIFY(run()["ok"].toBool());
        QCOMPARE(c.dispatchCount(), 1);
        now = 10;
        c.tick();
        QCOMPARE(value(c.outcome(id))["state"].toString(), "Accepted");
        now = 50;
        c.tick();
        QCOMPARE(value(c.outcome(id))["state"].toString(), "Completed");
        QCOMPARE(value(c.outcome(id))["evidence"].toObject()["position"].toDouble(), 0.9);
        QCOMPARE(code(c.execute(id, newId(), newId())), "DUPLICATE_CONFLICT");
        QCOMPARE(c.dispatchCount(), 1);
    }

    void missingFeedback()
    {
        QTemporaryDir dir;
        qint64 now = 0;
        ControlCore c(dir.path(), [&] { return now; });
        QVERIFY(setup(c)["ok"].toBool());
        c.setSimulationFault(SimulatedGripper::Fault::NoFeedback);
        const auto id = newId();
        QVERIFY(dispatch(c, id)["ok"].toBool());
        now = 10;
        c.tick();
        QCOMPARE(value(c.outcome(id))["state"].toString(), "Accepted");
        now = 2001;
        c.tick();
        QCOMPARE(value(c.outcome(id))["reason"].toString(), "FEEDBACK_TIMEOUT");
        QCOMPARE(value(c.outcome(id))["state"].toString(), "Unknown");
    }

    void staleAiAndApproval()
    {
        QTemporaryDir dir;
        ControlCore c(dir.path());
        QVERIFY(setup(c)["ok"].toBool());
        const auto snapshot = c.snapshot();
        const auto p = proposal(c);
        c.disconnectSession();
        QCOMPARE(
            code(c.acceptAiResult({{"kind", "answer"}, {"text", "old answer"}}, snapshot["sessionId"].toString(), 1)),
            "STALE_CONTEXT");
        QVERIFY(!c.approve(p["proposalId"].toString(), p["proposalHash"].toString())["ok"].toBool());
        QCOMPARE(c.dispatchCount(), 0);
    }

    void recoveryNeverReplays()
    {
        QTemporaryDir dir;
        const auto id = newId();
        {
            ControlCore c(dir.path());
            QVERIFY(setup(c)["ok"].toBool());
            QVERIFY(dispatch(c, id)["ok"].toBool());
        }
        ControlCore c(dir.path());
        QCOMPARE(value(c.outcome(id))["state"].toString(), "Unknown");
        QCOMPARE(value(c.outcome(id))["reason"].toString(), "PROCESS_RESTART");
        QCOMPARE(c.dispatchCount(), 0);
    }

    void exclusiveJournal()
    {
        QTemporaryDir dir;
        ControlCore first(dir.path());
        ControlCore second(dir.path());
        QVERIFY(!second.snapshot()["storageError"].toObject()["code"].toString().isEmpty());
    }
};
QTEST_GUILESS_MAIN(FoundationTest)
#include "FoundationTest.moc"

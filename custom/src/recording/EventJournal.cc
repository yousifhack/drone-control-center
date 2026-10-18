#include "EventJournal.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

namespace ycc {
EventJournal::EventJournal(const QString& path) : _connection(newId())
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        _error = {"STORAGE_FAILURE", "Cannot create journal directory."};
        return;
    }
    _lock = std::make_unique<QLockFile>(path + ".lock");
    if (!_lock->tryLock(0)) {
        _error = {"BUSY", "Journal is already open by another control-center process."};
        return;
    }
    _database = QSqlDatabase::addDatabase("QSQLITE", _connection);
    _database.setDatabaseName(path);
    _database.setConnectOptions("QSQLITE_BUSY_TIMEOUT=100");
    if (!_database.open()) {
        _error = {"STORAGE_FAILURE", _database.lastError().text()};
        return;
    }
    QSqlQuery query(_database);
    if (!query.exec("PRAGMA user_version") || !query.next() || query.value(0).toInt() > 1) {
        _error = {"STORAGE_FAILURE", "Unsupported journal version."};
        return;
    }
    for (const auto& sql :
         {"PRAGMA journal_mode=WAL", "PRAGMA synchronous=FULL",
          "CREATE TABLE IF NOT EXISTS commands (id TEXT PRIMARY KEY, fingerprint TEXT NOT NULL, outcome TEXT NOT NULL)",
          "CREATE TABLE IF NOT EXISTS events (sequence INTEGER PRIMARY KEY AUTOINCREMENT, utc TEXT NOT NULL, payload "
          "TEXT NOT NULL)",
          "PRAGMA user_version=1"}) {
        if (!query.exec(sql)) {
            _error = {"STORAGE_FAILURE", query.lastError().text()};
            return;
        }
    }
}

EventJournal::~EventJournal()
{
    if (_database.isValid()) {
        _database.close();
        _database = QSqlDatabase();
        QSqlDatabase::removeDatabase(_connection);
    }
}

bool EventJournal::_event(const QJsonObject& outcome)
{
    QSqlQuery query(_database);
    query.prepare("INSERT INTO events(utc,payload) VALUES(?,?)");
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.addBindValue(QString::fromUtf8(canonical(outcome)));
    return query.exec();
}

bool EventJournal::_commit(bool ok)
{
    if (ok && _database.commit()) {
        return true;
    }
    _database.rollback();
    _error = {"STORAGE_FAILURE", "Cannot persist the command journal; new dispatch is blocked."};
    return false;
}

bool EventJournal::beginRequest(const QString& id, const QString& fingerprint, const QJsonObject& outcome)
{
    if (!_error.empty()) {
        return false;
    }
    if (!_database.transaction()) {
        _error = {"STORAGE_FAILURE", "Cannot start journal transaction."};
        return false;
    }
    QSqlQuery query(_database);
    query.prepare("INSERT INTO commands(id,fingerprint,outcome) VALUES(?,?,?)");
    query.addBindValue(id);
    query.addBindValue(fingerprint);
    query.addBindValue(QString::fromUtf8(canonical(outcome)));
    return _commit(query.exec() && _event(outcome));
}

bool EventJournal::update(const QJsonObject& outcome)
{
    if (!_error.empty()) {
        return false;
    }
    if (!_database.transaction()) {
        _error = {"STORAGE_FAILURE", "Cannot start journal transaction."};
        return false;
    }
    QSqlQuery query(_database);
    query.prepare("UPDATE commands SET outcome=? WHERE id=?");
    query.addBindValue(QString::fromUtf8(canonical(outcome)));
    query.addBindValue(outcome["requestId"].toString());
    return _commit(query.exec() && query.numRowsAffected() == 1 && _event(outcome));
}

QJsonObject EventJournal::find(const QString& id)
{
    if (!_error.empty()) {
        return {};
    }
    QSqlQuery query(_database);
    query.prepare("SELECT fingerprint,outcome FROM commands WHERE id=?");
    query.addBindValue(id);
    if (!query.exec()) {
        _error = {"STORAGE_FAILURE", query.lastError().text()};
        return {};
    }
    if (!query.next()) {
        return {};
    }
    return {{"fingerprint", query.value(0).toString()},
            {"outcome", QJsonDocument::fromJson(query.value(1).toByteArray()).object()}};
}

QJsonArray EventJournal::events(int limit)
{
    QJsonArray result;
    if (!_error.empty()) {
        return result;
    }
    QSqlQuery query(_database);
    query.prepare("SELECT sequence,utc,payload FROM events ORDER BY sequence DESC LIMIT ?");
    query.addBindValue(qBound(1, limit, 500));
    if (!query.exec()) {
        _error = {"STORAGE_FAILURE", query.lastError().text()};
        return result;
    }
    while (query.next()) {
        result.prepend(QJsonObject{{"sequence", query.value(0).toDouble()},
                                   {"utc", query.value(1).toString()},
                                   {"outcome", QJsonDocument::fromJson(query.value(2).toByteArray()).object()}});
    }
    return result;
}

bool EventJournal::recover()
{
    if (!_error.empty()) {
        return false;
    }
    QVector<QJsonObject> unfinished;
    {
        QSqlQuery query(_database);
        if (!query.exec("SELECT outcome FROM commands")) {
            _error = {"STORAGE_FAILURE", query.lastError().text()};
            return false;
        }
        while (query.next()) {
            auto outcome = QJsonDocument::fromJson(query.value(0).toByteArray()).object();
            if (QStringList{"Dispatched", "Accepted", "Running"}.contains(outcome["state"].toString())) {
                outcome["state"] = "Unknown";
                outcome["reason"] = "PROCESS_RESTART";
                unfinished.append(outcome);
            }
        }
    }
    for (const auto& outcome : unfinished) {
        if (!update(outcome)) {
            return false;
        }
    }
    return true;
}
}  // namespace ycc

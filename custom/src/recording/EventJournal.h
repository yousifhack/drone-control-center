#pragma once

#include <QtCore/QLockFile>
#include <QtSql/QSqlDatabase>
#include <memory>

#include "domain/Contracts.h"

namespace ycc {
class EventJournal
{
public:
    explicit EventJournal(const QString& path);
    ~EventJournal();
    EventJournal(const EventJournal&) = delete;
    EventJournal& operator=(const EventJournal&) = delete;

    [[nodiscard]] Error error() const { return _error; }

    [[nodiscard]] bool beginRequest(const QString& id, const QString& fingerprint, const QJsonObject& outcome);
    [[nodiscard]] bool update(const QJsonObject& outcome);
    [[nodiscard]] QJsonObject find(const QString& id);
    [[nodiscard]] QJsonArray events(int limit = 100);
    [[nodiscard]] bool recover();

private:
    bool _event(const QJsonObject& outcome);
    bool _commit(bool success);
    QString _connection;
    QSqlDatabase _database;
    std::unique_ptr<QLockFile> _lock;
    Error _error;
};
}  // namespace ycc

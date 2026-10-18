#pragma once

#include <QtCore/QString>

#include "domain/Contracts.h"

namespace ycc {
/// Stores operator configuration only, never autopilot parameters or hardware verification.
class ProfileRepository
{
public:
    explicit ProfileRepository(QString directory);
    [[nodiscard]] Error load(const QString& id, Profile& profile) const;
    [[nodiscard]] Error save(const QJsonObject& draft, int expectedRevision, Profile& saved);

private:
    QString _directory;
};
}  // namespace ycc

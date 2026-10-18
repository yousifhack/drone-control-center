#include "ProfileRepository.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QLockFile>
#include <QtCore/QSaveFile>

namespace ycc {
ProfileRepository::ProfileRepository(QString directory) : _directory(std::move(directory)) {}

Error ProfileRepository::load(const QString& id, Profile& profile) const
{
    if (!isId(id)) {
        return {"INVALID_SCHEMA", "Invalid profile ID."};
    }
    QFile file(QDir(_directory).filePath(id + ".json"));
    if (!file.exists()) {
        return {"NOT_FOUND", "Profile does not exist."};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return {"STORAGE_FAILURE", file.errorString()};
    }
    QJsonObject object;
    auto error = parseObject(file.read(262145), object);
    if (!error.empty()) {
        return error;
    }
    error = Profile::parse(object, profile);
    if (error.empty() && profile.id != id) {
        return {"IDENTITY_MISMATCH", "Profile identity does not match its stored filename."};
    }
    return error;
}

Error ProfileRepository::save(const QJsonObject& draft, int expectedRevision, Profile& saved)
{
    Profile candidate;
    auto error = Profile::parse(draft, candidate);
    if (!error.empty()) {
        return error;
    }
    if (expectedRevision != candidate.revision || expectedRevision < 0 || expectedRevision >= 1000000000) {
        return {"REVISION_CONFLICT", "Draft base revision does not match."};
    }
    if (!QDir().mkpath(_directory)) {
        return {"STORAGE_FAILURE", "Cannot create profile directory."};
    }
    QLockFile lock(QDir(_directory).filePath("profiles.lock"));
    if (!lock.tryLock(0)) {
        return {"BUSY", "Another configuration writer holds the profile lock."};
    }
    Profile previous;
    error = load(candidate.id, previous);
    if (!error.empty() && error.code != "NOT_FOUND") {
        return error;
    }
    if ((error.code == "NOT_FOUND" && expectedRevision != 0) ||
        (error.empty() && previous.revision != expectedRevision)) {
        return {"REVISION_CONFLICT", "Saved profile changed; reload before applying edits."};
    }
    candidate.revision = expectedRevision + 1;
    QSaveFile file(QDir(_directory).filePath(candidate.id + ".json"));
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        return {"STORAGE_FAILURE", file.errorString()};
    }
    const auto bytes = canonical(candidate.json());
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        return {"STORAGE_FAILURE", file.errorString()};
    }
    saved = candidate;
    return {};
}
}  // namespace ycc

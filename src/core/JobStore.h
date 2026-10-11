#pragma once

#include "JobOptions.h"

#include <QString>
#include <QStringList>

#include <optional>

namespace obor {

// Stores job profiles as one JSON file per job in a directory.
class JobStore
{
public:
    explicit JobStore(QString directory);

    QString directory() const { return m_dir; }
    QStringList jobNames() const;
    bool exists(const QString &name) const;
    std::optional<JobOptions> load(const QString &name, QString *error = nullptr) const;
    bool save(const JobOptions &job, QString *error = nullptr) const;
    bool remove(const QString &name) const;
    bool rename(const QString &oldName, const QString &newName, QString *error = nullptr) const;

    // Loads a job from an arbitrary JSON file (used by the CLI).
    static std::optional<JobOptions> loadFile(const QString &path, QString *error = nullptr);
    static bool saveFile(const JobOptions &job, const QString &path, QString *error = nullptr);

private:
    QString filePathFor(const QString &name) const;
    QString findFile(const QString &name) const;

    QString m_dir;
};

} // namespace obor

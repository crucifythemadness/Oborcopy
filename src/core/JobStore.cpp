#include "JobStore.h"

#include "AppPaths.h"
#include "JobJson.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace obor {

JobStore::JobStore(QString directory)
    : m_dir(std::move(directory))
{
}

QStringList JobStore::jobNames() const
{
    QStringList names;
    const QStringList files = QDir(m_dir).entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString &file : files) {
        const auto job = loadFile(m_dir + QLatin1Char('/') + file);
        if (job && !job->name.isEmpty())
            names << job->name;
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

bool JobStore::exists(const QString &name) const
{
    return !findFile(name).isEmpty();
}

std::optional<JobOptions> JobStore::load(const QString &name, QString *error) const
{
    const QString path = findFile(name);
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("Job '%1' not found in %2").arg(name, m_dir);
        return std::nullopt;
    }
    return loadFile(path, error);
}

bool JobStore::save(const JobOptions &job, QString *error) const
{
    if (!AppPaths::ensureDir(m_dir)) {
        if (error)
            *error = QStringLiteral("Cannot create folder %1").arg(m_dir);
        return false;
    }
    const QString existing = findFile(job.name);
    return saveFile(job, existing.isEmpty() ? filePathFor(job.name) : existing, error);
}

bool JobStore::remove(const QString &name) const
{
    const QString path = findFile(name);
    return !path.isEmpty() && QFile::remove(path);
}

bool JobStore::rename(const QString &oldName, const QString &newName, QString *error) const
{
    if (exists(newName)) {
        if (error)
            *error = QStringLiteral("A job named '%1' already exists").arg(newName);
        return false;
    }
    auto job = load(oldName, error);
    if (!job)
        return false;
    job->name = newName;
    const QString oldPath = findFile(oldName);
    if (!saveFile(*job, filePathFor(newName), error))
        return false;
    QFile::remove(oldPath);
    return true;
}

std::optional<JobOptions> JobStore::loadFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot open %1: %2").arg(path, file.errorString());
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("Invalid JSON in %1: %2").arg(path, parseError.errorString());
        return std::nullopt;
    }
    return JobJson::fromJson(doc.object());
}

bool JobStore::saveFile(const JobOptions &job, const QString &path, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    file.write(QJsonDocument(JobJson::toJson(job)).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("Cannot save %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

QString JobStore::filePathFor(const QString &name) const
{
    const QString base = m_dir + QLatin1Char('/') + AppPaths::sanitizeFileName(name);
    QString path = base + QStringLiteral(".json");
    for (int i = 2; QFile::exists(path); ++i)
        path = QStringLiteral("%1-%2.json").arg(base).arg(i);
    return path;
}

QString JobStore::findFile(const QString &name) const
{
    const QStringList files = QDir(m_dir).entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString &file : files) {
        const QString path = m_dir + QLatin1Char('/') + file;
        const auto job = loadFile(path);
        if (job && job->name.compare(name, Qt::CaseInsensitive) == 0)
            return path;
    }
    return {};
}

} // namespace obor

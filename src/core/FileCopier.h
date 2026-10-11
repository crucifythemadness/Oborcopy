#pragma once

#include <QByteArray>
#include <QFileDevice>
#include <QString>

#include <functional>

class QCryptographicHash;
class QFile;

namespace obor {

class SyncControl;
class Throttle;

struct OpResult
{
    bool ok = false;
    bool retryable = true;
    bool cancelled = false;
    QString error;

    static OpResult success() { return {true, false, false, {}}; }
    static OpResult failure(const QString &error, bool retryable = true)
    {
        return {false, retryable, false, error};
    }
    static OpResult aborted() { return {false, false, true, QStringLiteral("Cancelled")}; }
};

struct CopyRequest
{
    QString source;
    QString target;
    qint64 mtimeMs = 0;
    QFileDevice::Permissions permissions;
};

struct CopySettings
{
    bool fileTimes = true;
    bool attributes = true;
    bool permissions = true;
    bool verify = false;
};

// Copies one file safely: data goes to "<target>.oborcopy-tmp" first and is
// renamed over the target only when complete, so an interrupted run never
// leaves a half-written file under the real name.
class FileCopier
{
public:
    using BytesFn = std::function<void(qint64 delta)>;

    FileCopier(const CopySettings &settings, SyncControl &control, Throttle *throttle,
               BytesFn onBytes);

    OpResult copy(const CopyRequest &request);

private:
    OpResult openFiles(const CopyRequest &request, QFile &in, QFile &out);
    OpResult transfer(QFile &in, QFile &out, QCryptographicHash *hash, qint64 &written);
    OpResult finishTemp(QFile &out, const CopyRequest &request);
    OpResult verifyTemp(const QString &tempPath, const QByteArray &expected);
    OpResult install(const QString &tempPath, const CopyRequest &request);
    static OpResult fileError(const QFile &file, const QString &what);

    CopySettings m_settings;
    SyncControl &m_control;
    Throttle *m_throttle;
    BytesFn m_onBytes;
    QByteArray m_buffer;
};

} // namespace obor

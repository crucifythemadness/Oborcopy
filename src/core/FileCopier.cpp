#include "FileCopier.h"

#include "FileEntry.h"
#include "Hash.h"
#include "Platform.h"
#include "SyncControl.h"
#include "Throttle.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QTimeZone>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <memory>

namespace obor {

namespace {
constexpr qint64 kChunkSize = 1 << 20; // 1 MiB
}

FileCopier::FileCopier(const CopySettings &settings, SyncControl &control, Throttle *throttle,
                       BytesFn onBytes)
    : m_settings(settings)
    , m_control(control)
    , m_throttle(throttle)
    , m_onBytes(std::move(onBytes))
    , m_buffer(kChunkSize, Qt::Uninitialized)
{
}

OpResult FileCopier::copy(const CopyRequest &request)
{
    const QString tempPath = request.target + kTempSuffix;
    QFile in(request.source);
    QFile out(tempPath);
    OpResult result = openFiles(request, in, out);
    if (!result.ok)
        return result;

    std::unique_ptr<QCryptographicHash> hash;
    if (m_settings.verify)
        hash = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);

    qint64 written = 0;
    result = transfer(in, out, hash.get(), written);
    in.close();
    if (result.ok)
        result = finishTemp(out, request);
    if (result.ok && hash)
        result = verifyTemp(tempPath, hash->result());
    if (result.ok)
        result = install(tempPath, request);
    if (!result.ok) {
        out.close();
        QFile::remove(tempPath);
        if (m_onBytes && written)
            m_onBytes(-written); // the next attempt starts from zero again
    }
    return result;
}

OpResult FileCopier::openFiles(const CopyRequest &request, QFile &in, QFile &out)
{
    if (!in.open(QIODevice::ReadOnly)) {
        if (!QFileInfo::exists(request.source))
            return OpResult::failure(QStringLiteral("Source file no longer exists"), false);
        return fileError(in, QStringLiteral("Cannot open source"));
    }
    const QString parent = QFileInfo(request.target).absolutePath();
    if (!QDir().mkpath(parent))
        return OpResult::failure(QStringLiteral("Cannot create folder %1").arg(parent));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fileError(out, QStringLiteral("Cannot create target"));
    return OpResult::success();
}

OpResult FileCopier::transfer(QFile &in, QFile &out, QCryptographicHash *hash, qint64 &written)
{
    while (true) {
        if (!m_control.checkpoint())
            return OpResult::aborted();
        const qint64 n = in.read(m_buffer.data(), m_buffer.size());
        if (n < 0)
            return fileError(in, QStringLiteral("Read error"));
        if (n == 0)
            return OpResult::success();
        if (out.write(m_buffer.constData(), n) != n)
            return fileError(out, QStringLiteral("Write error"));
        if (hash)
            hash->addData(QByteArrayView(m_buffer.constData(), n));
        written += n;
        if (m_onBytes)
            m_onBytes(n);
        if (m_throttle)
            m_throttle->consume(n, m_control);
    }
}

OpResult FileCopier::finishTemp(QFile &out, const CopyRequest &request)
{
    if (!out.flush())
        return fileError(out, QStringLiteral("Write error"));
    if (m_settings.fileTimes) {
        const QDateTime mtime = QDateTime::fromMSecsSinceEpoch(request.mtimeMs, QTimeZone::utc());
        if (!out.setFileTime(mtime, QFileDevice::FileModificationTime))
            return fileError(out, QStringLiteral("Cannot set file time"));
    }
    out.close();
    if (out.error() != QFileDevice::NoError)
        return fileError(out, QStringLiteral("Write error"));
    Platform::flushFileToDisk(out.fileName());
#ifndef Q_OS_WIN
    if (m_settings.permissions && request.permissions)
        QFile::setPermissions(out.fileName(), request.permissions);
#endif
    return OpResult::success();
}

OpResult FileCopier::verifyTemp(const QString &tempPath, const QByteArray &expected)
{
    QString error;
    const QByteArray actual = Hash::fileSha256(tempPath, &m_control, &error);
    if (m_control.isCancelled())
        return OpResult::aborted();
    if (actual.isEmpty())
        return OpResult::failure(QStringLiteral("Verification read failed: %1").arg(error));
    if (actual != expected)
        return OpResult::failure(QStringLiteral("Verification failed: checksum mismatch"));
    return OpResult::success();
}

OpResult FileCopier::install(const QString &tempPath, const CopyRequest &request)
{
    QString error;
    if (!Platform::replaceFile(tempPath, request.target, &error))
        return OpResult::failure(QStringLiteral("Cannot replace target: %1").arg(error));
    if (m_settings.attributes)
        Platform::copyAttributes(request.source, request.target);
    return OpResult::success();
}

OpResult FileCopier::fileError(const QFile &file, const QString &what)
{
    // A full disk does not get better by waiting; everything else (locks,
    // sharing violations, flaky USB/network) is worth a retry.
    const bool retryable = file.error() != QFileDevice::ResourceError;
    return OpResult::failure(QStringLiteral("%1: %2").arg(what, file.errorString()), retryable);
}

} // namespace obor

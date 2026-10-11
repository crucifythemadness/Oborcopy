#include "Platform.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace obor::Platform {

#ifdef Q_OS_WIN

namespace {

// Extended-length path so operations work beyond MAX_PATH (260 characters).
std::wstring winPath(const QString &path)
{
    QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    if (native.startsWith(QLatin1String("\\\\?\\")))
        return native.toStdWString();
    if (native.startsWith(QLatin1String("\\\\")))
        native = QStringLiteral("\\\\?\\UNC\\") + native.mid(2);
    else
        native = QStringLiteral("\\\\?\\") + native;
    return native.toStdWString();
}

QString lastErrorText()
{
    const DWORD code = GetLastError();
    wchar_t *buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                       | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    QString text = buffer ? QString::fromWCharArray(buffer).trimmed() : QString();
    LocalFree(buffer);
    return QStringLiteral("%1 (code %2)").arg(text).arg(code);
}

constexpr DWORD kCopiedAttributes = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN
                                    | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE;
constexpr DWORD kProtective = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN
                              | FILE_ATTRIBUTE_SYSTEM;

} // namespace

bool isSystemFile(const QString &path)
{
    const DWORD attrs = GetFileAttributesW(winPath(path).c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_SYSTEM);
}

void clearProtectiveAttributes(const QString &path)
{
    const std::wstring p = winPath(path);
    const DWORD attrs = GetFileAttributesW(p.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & kProtective))
        return;
    DWORD cleared = attrs & ~kProtective;
    SetFileAttributesW(p.c_str(), cleared ? cleared : FILE_ATTRIBUTE_NORMAL);
}

bool copyAttributes(const QString &from, const QString &to)
{
    const DWORD src = GetFileAttributesW(winPath(from).c_str());
    const std::wstring target = winPath(to);
    const DWORD dst = GetFileAttributesW(target.c_str());
    if (src == INVALID_FILE_ATTRIBUTES || dst == INVALID_FILE_ATTRIBUTES)
        return false;
    DWORD merged = (dst & ~kCopiedAttributes) | (src & kCopiedAttributes);
    if (merged == dst)
        return true;
    return SetFileAttributesW(target.c_str(), merged ? merged : FILE_ATTRIBUTE_NORMAL) != 0;
}

bool replaceFile(const QString &from, const QString &to, QString *error)
{
    clearProtectiveAttributes(to);
    if (MoveFileExW(winPath(from).c_str(), winPath(to).c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return true;
    if (error)
        *error = lastErrorText();
    return false;
}

bool setDirModificationTime(const QString &path, qint64 msecsSinceEpoch)
{
    HANDLE h = CreateFileW(winPath(path).c_str(), FILE_WRITE_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    // FILETIME counts 100 ns intervals since 1601-01-01.
    const quint64 ticks = quint64(msecsSinceEpoch + 11644473600000LL) * 10000ULL;
    FILETIME ft;
    ft.dwLowDateTime = DWORD(ticks & 0xFFFFFFFFULL);
    ft.dwHighDateTime = DWORD(ticks >> 32);
    const bool ok = SetFileTime(h, nullptr, nullptr, &ft) != 0;
    CloseHandle(h);
    return ok;
}

bool flushFileToDisk(const QString &path)
{
    HANDLE h = CreateFileW(winPath(path).c_str(), GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    const bool ok = FlushFileBuffers(h) != 0;
    CloseHandle(h);
    return ok;
}

#else // POSIX

bool isSystemFile(const QString &)
{
    return false;
}

void clearProtectiveAttributes(const QString &)
{
}

bool copyAttributes(const QString &, const QString &)
{
    return true;
}

bool replaceFile(const QString &from, const QString &to, QString *error)
{
    if (::rename(QFile::encodeName(from).constData(), QFile::encodeName(to).constData()) == 0)
        return true;
    if (error)
        *error = QString::fromLocal8Bit(std::strerror(errno));
    return false;
}

bool setDirModificationTime(const QString &path, qint64 msecsSinceEpoch)
{
    struct timespec times[2];
    times[0].tv_sec = 0;
    times[0].tv_nsec = UTIME_OMIT;
    times[1].tv_sec = msecsSinceEpoch / 1000;
    times[1].tv_nsec = (msecsSinceEpoch % 1000) * 1000000;
    return utimensat(AT_FDCWD, QFile::encodeName(path).constData(), times, 0) == 0;
}

bool flushFileToDisk(const QString &path)
{
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY);
    if (fd < 0)
        return false;
    const bool ok = ::fsync(fd) == 0;
    ::close(fd);
    return ok;
}

#endif

} // namespace obor::Platform

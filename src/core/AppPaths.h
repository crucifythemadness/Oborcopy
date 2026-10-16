#pragma once

#include <QString>

// Locations of configuration, job profiles and logs.
// Setting the environment variable OBORCOPY_CONFIG_DIR redirects everything
// into one folder (used by tests and for a "portable" setup).
namespace obor::AppPaths {

QString configDir();
QString jobsDir();
QString settingsFile();
QString defaultLogDir();
// Schedule, state and log of the scheduler service (see ServiceFiles.h).
QString serviceDir();
bool ensureDir(const QString &path);

// Replaces characters that are not allowed in file names on Windows or Linux.
QString sanitizeFileName(const QString &name);

} // namespace obor::AppPaths

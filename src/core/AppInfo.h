#pragma once

#include <QString>

// Static application metadata shown in the About dialog, logs and the CLI.
namespace obor::AppInfo {

QString name();
QString version();
QString buildDate();
QString qtVersion();
QString compiler();
QString platform();

} // namespace obor::AppInfo

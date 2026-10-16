#pragma once

#include <QString>

// Windows service host and the administrative commands the GUI runs elevated
// (one UAC prompt). Every admin command writes its outcome to 'resultFile'
// ("OK", "CANCELLED" or "ERROR\n<message>"), because an elevated process
// started with ShellExecuteEx has no stdout the GUI could read.
namespace obor::WinService {

// Connects to the service control manager. Returns -1 if the process was not
// started by the SCM (e.g. double-clicked), otherwise the exit code.
int runDispatcher();
// Runs the scheduler in the console until Ctrl+C (for testing).
int runConsole();

// 'user' is the account the GUI runs as (DOMAIN\name); the service runs under it.
int install(const QString &user, const QString &resultFile);
int uninstall(const QString &resultFile);
int updatePassword(const QString &user, const QString &resultFile);
// Points the service to this executable (Oborcopy folder moved or updated).
int setPath(const QString &resultFile);

} // namespace obor::WinService

#pragma once

// Runs the scheduler in the foreground, as systemd (Type=simple) expects:
// SIGTERM/SIGINT stop, SIGUSR1 pauses, SIGUSR2 resumes, SIGHUP reloads the schedule.
namespace obor::PosixDaemon {

int run();

} // namespace obor::PosixDaemon

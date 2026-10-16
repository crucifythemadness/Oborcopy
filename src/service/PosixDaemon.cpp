#include "PosixDaemon.h"

#include "Scheduler.h"

#include <atomic>
#include <csignal>

namespace obor::PosixDaemon {

namespace {

std::atomic<Scheduler *> g_scheduler{nullptr};

// Only atomic stores happen here, which is async-signal-safe.
void onSignal(int signal)
{
    Scheduler *scheduler = g_scheduler.load();
    if (!scheduler)
        return;
    switch (signal) {
    case SIGUSR1: scheduler->setPaused(true); break;
    case SIGUSR2: scheduler->setPaused(false); break;
    case SIGHUP: scheduler->requestReload(); break;
    default: scheduler->requestStop(); break;
    }
}

void install(int signal)
{
    struct sigaction action {};
    action.sa_handler = onSignal;
    sigemptyset(&action.sa_mask);
    sigaction(signal, &action, nullptr);
}

} // namespace

int run()
{
    Scheduler scheduler;
    g_scheduler = &scheduler;
    for (int signal : {SIGTERM, SIGINT, SIGUSR1, SIGUSR2, SIGHUP})
        install(signal);
    scheduler.run();
    g_scheduler = nullptr;
    return 0;
}

} // namespace obor::PosixDaemon

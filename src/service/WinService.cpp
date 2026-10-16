#include "WinService.h"

#include "Scheduler.h"
#include "ServiceFiles.h"

#include <windows.h>

#include <atomic>
#include <string>

namespace obor::WinService {

namespace {

SERVICE_STATUS_HANDLE g_handle = nullptr;
SERVICE_STATUS g_status{};
std::atomic<Scheduler *> g_scheduler{nullptr};

void report(DWORD state, DWORD waitHint = 0)
{
    static DWORD checkPoint = 1;
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = state;
    g_status.dwWin32ExitCode = NO_ERROR;
    g_status.dwWaitHint = waitHint;
    g_status.dwControlsAccepted = state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING
                                      ? 0
                                      : SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_SHUTDOWN;
    g_status.dwCheckPoint = (state == SERVICE_RUNNING || state == SERVICE_STOPPED || state == SERVICE_PAUSED)
                                ? 0
                                : checkPoint++;
    SetServiceStatus(g_handle, &g_status);
}

DWORD WINAPI controlHandler(DWORD control, DWORD, LPVOID, LPVOID)
{
    Scheduler *scheduler = g_scheduler.load();
    switch (control) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        // A running job gets up to 30 s to cancel cleanly.
        report(SERVICE_STOP_PENDING, 45000);
        if (scheduler)
            scheduler->requestStop();
        return NO_ERROR;
    case SERVICE_CONTROL_PAUSE:
        if (scheduler)
            scheduler->setPaused(true);
        report(SERVICE_PAUSED);
        return NO_ERROR;
    case SERVICE_CONTROL_CONTINUE:
        if (scheduler)
            scheduler->setPaused(false);
        report(SERVICE_RUNNING);
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void WINAPI serviceMain(DWORD, LPWSTR *)
{
    const std::wstring name = ServiceInfo::serviceName().toStdWString();
    g_handle = RegisterServiceCtrlHandlerExW(name.c_str(), controlHandler, nullptr);
    if (!g_handle)
        return;
    report(SERVICE_START_PENDING, 10000);
    Scheduler scheduler;
    g_scheduler = &scheduler;
    report(SERVICE_RUNNING);
    scheduler.run();
    g_scheduler = nullptr;
    report(SERVICE_STOPPED);
}

BOOL WINAPI consoleHandler(DWORD)
{
    if (Scheduler *scheduler = g_scheduler.load())
        scheduler->requestStop();
    return TRUE;
}

} // namespace

int runDispatcher()
{
    std::wstring name = ServiceInfo::serviceName().toStdWString();
    SERVICE_TABLE_ENTRYW table[] = {{name.data(), serviceMain}, {nullptr, nullptr}};
    if (StartServiceCtrlDispatcherW(table))
        return 0;
    return GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT ? -1 : 1;
}

int runConsole()
{
    Scheduler scheduler;
    g_scheduler = &scheduler;
    SetConsoleCtrlHandler(consoleHandler, TRUE);
    scheduler.run();
    g_scheduler = nullptr;
    return 0;
}

} // namespace obor::WinService

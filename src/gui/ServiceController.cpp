#include "ServiceController.h"

#include "AppPaths.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#ifdef Q_OS_WIN
#include <QUuid>
#define SECURITY_WIN32
#include <windows.h>
#include <security.h>
#include <shellapi.h>
#else
#include <QProcess>
#include <QStandardPaths>
#include <csignal>
#include <sys/types.h>
#endif

namespace obor {

namespace {

constexpr int kPollMs = 2000;
constexpr int kRestartTimeoutMs = 90000;

bool samePath(const QString &a, const QString &b)
{
    const QString ca = QDir::cleanPath(QDir::fromNativeSeparators(a));
    const QString cb = QDir::cleanPath(QDir::fromNativeSeparators(b));
#ifdef Q_OS_WIN
    return ca.compare(cb, Qt::CaseInsensitive) == 0;
#else
    return ca == cb;
#endif
}

} // namespace

// ---------------------------------------------------------------- ServiceState

bool ServiceState::isInstalled() const
{
    return kind != Kind::Unknown && kind != Kind::Unavailable && kind != Kind::NotInstalled;
}

bool ServiceState::isRunning() const
{
    return kind == Kind::Running || kind == Kind::Busy || kind == Kind::Paused;
}

QString ServiceState::title() const
{
    switch (kind) {
    case Kind::Unknown: return QObject::tr("Checking service…");
    case Kind::Unavailable: return QObject::tr("Service not available");
    case Kind::NotInstalled: return QObject::tr("Scheduler not set up");
    case Kind::Stopped: return QObject::tr("Service stopped");
    case Kind::Starting: return QObject::tr("Service starting…");
    case Kind::Running: return QObject::tr("Service running");
    case Kind::Busy: return QObject::tr("Backing up: %1").arg(status.currentJob);
    case Kind::Paused: return QObject::tr("Service paused");
    case Kind::Stopping: return QObject::tr("Service stopping…");
    case Kind::Failed: return QObject::tr("Service error");
    }
    return {};
}

// ---------------------------------------------------------------- common

ServiceController::ServiceController(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(kPollMs);
    connect(m_timer, &QTimer::timeout, this, &ServiceController::refresh);
    m_timer->start();
#ifdef Q_OS_WIN
    m_elevatedTimer = new QTimer(this);
    m_elevatedTimer->setInterval(300);
    connect(m_elevatedTimer, &QTimer::timeout, this, &ServiceController::pollElevated);
#endif
    QTimer::singleShot(0, this, &ServiceController::refresh);
}

ServiceController::~ServiceController()
{
#ifdef Q_OS_WIN
    if (m_elevatedProcess)
        CloseHandle(static_cast<HANDLE>(m_elevatedProcess));
#endif
}

QString ServiceController::serviceBinary()
{
    return QDir::toNativeSeparators(
        QDir(QCoreApplication::applicationDirPath()).filePath(ServiceInfo::serviceExecutable()));
}

bool ServiceController::supportsPasswordUpdate()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

void ServiceController::apply(const OsInfo &os)
{
    m_os = os;
    ServiceState s;
    s.account = os.account;
    s.installedBinary = os.binary;
    s.message = m_lastError;
    if (!os.available) {
        s.kind = ServiceState::Kind::Unavailable;
        s.message = os.unavailableReason;
    } else if (!os.installed) {
        s.kind = ServiceState::Kind::NotInstalled;
    } else {
        s.kind = os.kind;
        s.binaryMismatch = !os.binary.isEmpty() && !samePath(os.binary, serviceBinary());
        if (os.kind == ServiceState::Kind::Failed && !os.failure.isEmpty())
            s.message = os.failure;
        if (os.kind == ServiceState::Kind::Running || os.kind == ServiceState::Kind::Paused) {
            bool ok = false;
            s.status = ServiceStatus::load(ServiceInfo::statusPath(), &ok);
            s.responding = ok && s.status.isFresh() && s.status.activity != ServiceActivity::Stopped;
            if (s.responding) {
                if (s.status.activity == ServiceActivity::Busy)
                    s.kind = ServiceState::Kind::Busy;
                else if (s.status.activity == ServiceActivity::Paused)
                    s.kind = ServiceState::Kind::Paused;
                else if (s.status.activity == ServiceActivity::Stopping)
                    s.kind = ServiceState::Kind::Stopping;
            }
        }
    }
    m_state = s;

    if (m_restartPending) {
        if (s.kind == ServiceState::Kind::Stopped) {
            m_restartPending = false;
            start();
            return;
        }
        if (QDateTime::currentMSecsSinceEpoch() > m_restartDeadline) {
            m_restartPending = false;
            endAction(false, tr("The service did not stop in time."));
            return;
        }
    }
    emit stateChanged(m_state);
}

void ServiceController::beginAction(const QString &text)
{
    m_action = text;
    m_lastError.clear();
    emit stateChanged(m_state);
}

void ServiceController::endAction(bool ok, const QString &message)
{
    m_action.clear();
    if (!ok)
        m_lastError = message;
    emit actionFinished(ok, message);
    refresh();
}

void ServiceController::restart()
{
    if (isBusy() || !m_state.isInstalled())
        return;
    if (m_state.kind == ServiceState::Kind::Stopped) {
        start();
        return;
    }
#ifndef Q_OS_WIN
    runCommands({{{QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("restart"),
                   ServiceInfo::unitName()}}},
                tr("Restarting…"), QString());
    return;
#endif
    stop();
    m_restartPending = true;
    m_restartDeadline = QDateTime::currentMSecsSinceEpoch() + kRestartTimeoutMs;
    m_action = tr("Restarting…");
}

#ifdef Q_OS_WIN

// ---------------------------------------------------------------- Windows

namespace {

class ScHandle
{
public:
    explicit ScHandle(SC_HANDLE handle = nullptr) : m_handle(handle) {}
    ~ScHandle()
    {
        if (m_handle)
            CloseServiceHandle(m_handle);
    }
    ScHandle(const ScHandle &) = delete;
    ScHandle &operator=(const ScHandle &) = delete;
    operator SC_HANDLE() const { return m_handle; }
    explicit operator bool() const { return m_handle != nullptr; }

private:
    SC_HANDLE m_handle;
};

QString systemMessage(DWORD code)
{
    wchar_t *buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    const QString text = buffer ? QString::fromWCharArray(buffer).trimmed() : QString();
    LocalFree(buffer);
    return text;
}

QString explain(DWORD code)
{
    if (code == ERROR_SERVICE_LOGON_FAILED)
        return QObject::tr("Windows could not log on the service with your account. If you changed your "
                           "Windows password, use “Update password…”.");
    if (code == ERROR_ACCESS_DENIED)
        return QObject::tr("Access denied. Reinstall the service to allow control from Oborcopy.");
    return systemMessage(code);
}

QString currentUser()
{
    wchar_t name[512];
    ULONG size = 512;
    if (GetUserNameExW(NameSamCompatible, name, &size))
        return QString::fromWCharArray(name, int(size));
    return qEnvironmentVariable("USERDOMAIN") + QLatin1Char('\\') + qEnvironmentVariable("USERNAME");
}

QString unquotedProgram(QString commandLine)
{
    commandLine = commandLine.trimmed();
    if (commandLine.startsWith(QLatin1Char('"'))) {
        const int end = commandLine.indexOf(QLatin1Char('"'), 1);
        return commandLine.mid(1, end > 0 ? end - 1 : -1);
    }
    return commandLine.section(QLatin1Char(' '), 0, 0);
}

} // namespace

ServiceController::OsInfo ServiceController::queryWindows() const
{
    OsInfo os;
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm) {
        os.available = false;
        os.unavailableReason = systemMessage(GetLastError());
        return os;
    }
    const std::wstring name = ServiceInfo::serviceName().toStdWString();
    ScHandle service(OpenServiceW(scm, name.c_str(), SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG));
    if (!service)
        return os; // not installed (or not visible to us)
    os.installed = true;

    SERVICE_STATUS_PROCESS status{};
    DWORD needed = 0;
    if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status),
                             &needed)) {
        os.pid = status.dwProcessId;
        switch (status.dwCurrentState) {
        case SERVICE_START_PENDING: os.kind = ServiceState::Kind::Starting; break;
        case SERVICE_RUNNING: os.kind = ServiceState::Kind::Running; break;
        case SERVICE_PAUSED:
        case SERVICE_PAUSE_PENDING: os.kind = ServiceState::Kind::Paused; break;
        case SERVICE_CONTINUE_PENDING: os.kind = ServiceState::Kind::Running; break;
        case SERVICE_STOP_PENDING: os.kind = ServiceState::Kind::Stopping; break;
        default: os.kind = ServiceState::Kind::Stopped; break;
        }
        if (os.kind == ServiceState::Kind::Stopped && status.dwWin32ExitCode != NO_ERROR
            && status.dwWin32ExitCode != ERROR_SERVICE_NEVER_STARTED) {
            os.kind = ServiceState::Kind::Failed;
            os.failure = status.dwWin32ExitCode == ERROR_SERVICE_SPECIFIC_ERROR
                             ? tr("The service stopped with error %1.").arg(status.dwServiceSpecificExitCode)
                             : explain(status.dwWin32ExitCode);
        }
    }

    QueryServiceConfigW(service, nullptr, 0, &needed);
    QByteArray buffer(int(needed), '\0');
    auto *config = reinterpret_cast<QUERY_SERVICE_CONFIGW *>(buffer.data());
    if (needed && QueryServiceConfigW(service, config, needed, &needed)) {
        os.binary = unquotedProgram(QString::fromWCharArray(config->lpBinaryPathName));
        os.account = QString::fromWCharArray(config->lpServiceStartName);
    }
    return os;
}

void ServiceController::refresh()
{
    if (m_elevatedProcess)
        return; // the elevated helper changes the service right now
    apply(queryWindows());
}

bool ServiceController::control(unsigned long code, QString *error)
{
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    const std::wstring name = ServiceInfo::serviceName().toStdWString();
    const DWORD access = code == 0 ? SERVICE_START : (SERVICE_STOP | SERVICE_PAUSE_CONTINUE);
    ScHandle service(scm ? OpenServiceW(scm, name.c_str(), access) : nullptr);
    BOOL ok = FALSE;
    if (service) {
        if (code == 0) {
            ok = StartServiceW(service, 0, nullptr) || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;
        } else {
            SERVICE_STATUS status{};
            ok = ControlService(service, code, &status);
        }
    }
    if (!ok)
        *error = explain(GetLastError());
    return ok;
}

void ServiceController::start()
{
    if (isBusy() && !m_action.startsWith(tr("Restarting")))
        return;
    beginAction(tr("Starting…"));
    QString error;
    const bool ok = control(0, &error);
    endAction(ok, ok ? QString() : tr("Could not start the service: %1").arg(error));
}

void ServiceController::stop()
{
    if (isBusy())
        return;
    beginAction(tr("Stopping…"));
    QString error;
    const bool ok = control(SERVICE_CONTROL_STOP, &error);
    endAction(ok, ok ? QString() : tr("Could not stop the service: %1").arg(error));
}

void ServiceController::pause()
{
    if (isBusy())
        return;
    QString error;
    const bool ok = control(SERVICE_CONTROL_PAUSE, &error);
    endAction(ok, ok ? QString() : tr("Could not pause the service: %1").arg(error));
}

void ServiceController::resume()
{
    if (isBusy())
        return;
    QString error;
    const bool ok = control(SERVICE_CONTROL_CONTINUE, &error);
    endAction(ok, ok ? QString() : tr("Could not resume the service: %1").arg(error));
}

void ServiceController::install()
{
    runElevated({QStringLiteral("--install"), QStringLiteral("--user"), currentUser()}, tr("Installing…"),
                tr("The Oborcopy Scheduler service is installed and running."));
}

void ServiceController::uninstall()
{
    runElevated({QStringLiteral("--uninstall")}, tr("Removing…"), tr("The service was removed."));
}

void ServiceController::updatePassword()
{
    runElevated({QStringLiteral("--update-password"), QStringLiteral("--user"), currentUser()},
                tr("Updating password…"), tr("The password was updated and the service started."));
}

void ServiceController::repair()
{
    runElevated({QStringLiteral("--set-path")}, tr("Updating service…"),
                tr("The service now uses this Oborcopy installation."));
}

void ServiceController::runElevated(const QStringList &arguments, const QString &text,
                                    const QString &successMessage)
{
    if (isBusy())
        return;
    const QString program = serviceBinary();
    if (!QFileInfo::exists(program)) {
        endAction(false, tr("%1 was not found next to Oborcopy.").arg(ServiceInfo::serviceExecutable()));
        return;
    }
    m_resultFile = QDir::toNativeSeparators(
        QDir::temp().filePath(QStringLiteral("oborcopy-service-%1.txt").arg(QUuid::createUuid().toString(QUuid::Id128))));
    QFile::remove(m_resultFile);
    QStringList args = arguments;
    args << QStringLiteral("--result-file") << m_resultFile;
    QStringList quoted;
    for (const QString &arg : std::as_const(args))
        quoted << QLatin1Char('"') + arg + QLatin1Char('"');

    const std::wstring file = program.toStdWString();
    const std::wstring params = quoted.join(QLatin1Char(' ')).toStdWString();
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.lpVerb = L"runas";
    info.lpFile = file.c_str();
    info.lpParameters = params.c_str();
    info.nShow = SW_HIDE;
    beginAction(text);
    if (!ShellExecuteExW(&info) || !info.hProcess) {
        const DWORD code = GetLastError();
        endAction(false, code == ERROR_CANCELLED ? tr("Administrator permission was not granted.")
                                                 : systemMessage(code));
        return;
    }
    m_elevatedProcess = info.hProcess;
    m_successMessage = successMessage;
    m_elevatedTimer->start();
}

void ServiceController::pollElevated()
{
    auto process = static_cast<HANDLE>(m_elevatedProcess);
    if (!process || WaitForSingleObject(process, 0) == WAIT_TIMEOUT)
        return;
    m_elevatedTimer->stop();
    CloseHandle(process);
    m_elevatedProcess = nullptr;

    QFile file(m_resultFile);
    QString result = file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    file.close();
    QFile::remove(m_resultFile);
    if (result.startsWith(QLatin1String("OK")))
        endAction(true, m_successMessage);
    else if (result.startsWith(QLatin1String("CANCELLED")))
        endAction(false, tr("Cancelled – nothing was changed."));
    else
        endAction(false, result.section(QLatin1Char('\n'), 1).trimmed().isEmpty()
                             ? tr("The service setup failed.")
                             : result.section(QLatin1Char('\n'), 1).trimmed());
}

#else

// ---------------------------------------------------------------- Linux (systemd user unit)

QString ServiceController::unitFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/systemd/user/") + ServiceInfo::unitName();
}

bool ServiceController::writeUnitFile(QString *error) const
{
    QString program = serviceBinary();
    program.replace(QLatin1Char('%'), QStringLiteral("%%"));
    const QString unit = QStringLiteral(
                             "# Created by Oborcopy (Service Manager). Remove it there, not by hand.\n"
                             "[Unit]\n"
                             "Description=%1 - scheduled Oborcopy backups\n"
                             "After=network-online.target\n\n"
                             "[Service]\n"
                             "Type=simple\n"
                             "ExecStart=\"%2\" --foreground\n"
                             "Restart=on-failure\n"
                             "RestartSec=30\n"
                             "KillMode=mixed\n"
                             "TimeoutStopSec=60\n\n"
                             "[Install]\n"
                             "WantedBy=default.target\n")
                             .arg(ServiceInfo::displayName(), program);
    const QString path = unitFilePath();
    AppPaths::ensureDir(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        *error = tr("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    file.write(unit.toUtf8());
    return true;
}

void ServiceController::refresh()
{
    if (m_querying || !m_commands.isEmpty())
        return;
    queryLinux();
}

void ServiceController::queryLinux()
{
    m_querying = true;
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus) {
        m_querying = false;
        process->deleteLater();
        OsInfo os;
        QMap<QString, QString> values;
        const QStringList lines = QString::fromUtf8(process->readAllStandardOutput()).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq > 0)
                values.insert(line.left(eq), line.mid(eq + 1).trimmed());
        }
        if (exitCode != 0 && values.isEmpty()) {
            os.available = false;
            os.unavailableReason = tr("No systemd user session is available: %1")
                                       .arg(QString::fromUtf8(process->readAllStandardError()).trimmed());
            apply(os);
            return;
        }
        os.installed = values.value(QStringLiteral("LoadState")) == QLatin1String("loaded")
                       && QFileInfo::exists(unitFilePath());
        const QString active = values.value(QStringLiteral("ActiveState"));
        if (active == QLatin1String("active") || active == QLatin1String("reloading"))
            os.kind = ServiceState::Kind::Running;
        else if (active == QLatin1String("activating"))
            os.kind = ServiceState::Kind::Starting;
        else if (active == QLatin1String("deactivating"))
            os.kind = ServiceState::Kind::Stopping;
        else if (active == QLatin1String("failed")) {
            os.kind = ServiceState::Kind::Failed;
            os.failure = tr("The service failed (%1). See the service log.")
                             .arg(values.value(QStringLiteral("Result")));
        } else
            os.kind = ServiceState::Kind::Stopped;
        os.pid = values.value(QStringLiteral("MainPID")).toLongLong();
        // ExecStart={ path=/opt/oborcopy/bin/oborcopy-service ; argv[]=... }
        const QString exec = values.value(QStringLiteral("ExecStart"));
        const int p = exec.indexOf(QLatin1String("path="));
        if (p >= 0)
            os.binary = exec.mid(p + 5).section(QLatin1Char(' '), 0, 0).section(QLatin1Char(';'), 0, 0).trimmed();
        os.account = qEnvironmentVariable("USER");
        apply(os);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_querying = false;
        process->deleteLater();
        OsInfo os;
        os.available = false;
        os.unavailableReason = tr("systemctl was not found – this system does not use systemd.");
        apply(os);
    });
    process->start(QStringLiteral("systemctl"),
                   {QStringLiteral("--user"), QStringLiteral("show"), ServiceInfo::unitName(),
                    QStringLiteral("--property=LoadState,ActiveState,Result,MainPID,ExecStart")});
}

void ServiceController::runCommands(QList<Command> commands, const QString &text,
                                    const QString &successMessage, std::function<void()> before)
{
    if (isBusy())
        return;
    beginAction(text);
    if (before)
        before();
    if (!m_lastError.isEmpty()) { // 'before' failed
        const QString error = m_lastError;
        endAction(false, error);
        return;
    }
    m_commands = std::move(commands);
    m_hints.clear();
    m_successMessage = successMessage;
    runNext();
}

void ServiceController::runNext()
{
    if (m_commands.isEmpty()) {
        QString message = m_successMessage;
        if (!m_hints.isEmpty())
            message += QStringLiteral("\n\n") + m_hints.join(QLatin1Char('\n'));
        endAction(true, message);
        return;
    }
    const Command command = m_commands.takeFirst();
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, command](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        if (exitCode == 0 && status == QProcess::NormalExit) {
            runNext();
            return;
        }
        const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
        if (command.optional) {
            m_hints << tr("Note: “%1” failed (%2). The service runs while you are logged in; ask an "
                          "administrator to run “loginctl enable-linger %3” so it also runs after logout.")
                           .arg(command.argv.join(QLatin1Char(' ')), error, qEnvironmentVariable("USER"));
            runNext();
            return;
        }
        m_commands.clear();
        endAction(false, tr("“%1” failed: %2").arg(command.argv.join(QLatin1Char(' ')), error));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, command](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        m_commands.clear();
        endAction(false, tr("Cannot run %1.").arg(command.argv.value(0)));
    });
    process->start(command.argv.value(0), command.argv.mid(1));
}

void ServiceController::start()
{
    runCommands({{{QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("start"), ServiceInfo::unitName()}}},
                tr("Starting…"), QString());
}

void ServiceController::stop()
{
    runCommands({{{QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("stop"), ServiceInfo::unitName()}}},
                tr("Stopping…"), QString());
}

void ServiceController::signalService(int signal, const QString &text)
{
    if (isBusy())
        return;
    const bool ok = m_os.pid > 0 && ::kill(pid_t(m_os.pid), signal) == 0;
    endAction(ok, ok ? QString() : tr("Could not %1 the service (no running process).").arg(text));
}

void ServiceController::pause()
{
    signalService(SIGUSR1, tr("pause"));
}

void ServiceController::resume()
{
    signalService(SIGUSR2, tr("resume"));
}

void ServiceController::install()
{
    const QString user = qEnvironmentVariable("USER");
    QList<Command> commands = {
        {{QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("daemon-reload")}},
        {{QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("enable"), QStringLiteral("--now"),
          ServiceInfo::unitName()}},
        // Keeps the user's services running after logout and starts them at boot.
        {{QStringLiteral("loginctl"), QStringLiteral("enable-linger"), user}, true}};
    runCommands(commands, tr("Installing…"), tr("The Oborcopy Scheduler service is installed and running."),
                [this]() {
                    QString error;
                    if (!QFileInfo::exists(serviceBinary()))
                        m_lastError = tr("%1 was not found next to Oborcopy.").arg(ServiceInfo::serviceExecutable());
                    else if (!writeUnitFile(&error))
                        m_lastError = error;
                });
}

void ServiceController::uninstall()
{
    QList<Command> commands = {
        {{QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("disable"), QStringLiteral("--now"),
          ServiceInfo::unitName()}},
        {{QStringLiteral("rm"), QStringLiteral("-f"), unitFilePath()}},
        {{QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("daemon-reload")}}};
    runCommands(commands, tr("Removing…"), tr("The service was removed."));
}

void ServiceController::updatePassword()
{
}

void ServiceController::repair()
{
    QList<Command> commands = {
        {{QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("daemon-reload")}},
        {{QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("restart"), ServiceInfo::unitName()}}};
    runCommands(commands, tr("Updating service…"), tr("The service now uses this Oborcopy installation."),
                [this]() {
                    QString error;
                    if (!writeUnitFile(&error))
                        m_lastError = error;
                });
}

#endif

} // namespace obor

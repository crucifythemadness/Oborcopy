#pragma once

#include "ServiceFiles.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QTimer;

namespace obor {

// What the GUI shows about the scheduler service: the OS view (installed,
// running, ...) combined with the service's own status.json.
struct ServiceState
{
    enum class Kind {
        Unknown,      // not queried yet
        Unavailable,  // no service manager (e.g. no systemd user session)
        NotInstalled,
        Stopped,
        Starting,
        Running,      // idle, waiting for the next job
        Busy,         // running a job
        Paused,
        Stopping,
        Failed        // crashed / cannot log on
    };

    Kind kind = Kind::Unknown;
    QString message;          // reason for Unavailable / Failed, or the last action error
    QString account;          // Windows: account the service logs on as
    QString installedBinary;  // program the service manager starts
    bool binaryMismatch = false; // installed from another Oborcopy folder or version
    bool responding = true;   // heartbeat in status.json is fresh
    ServiceStatus status;     // meaningful while running

    bool isInstalled() const;
    bool isRunning() const; // Running, Busy or Paused
    QString title() const;
};

// Installs, controls and watches the service. Windows: service control
// manager (admin actions run elevated through one UAC prompt). Linux: systemd
// user unit through `systemctl --user`. Polls every 2 seconds.
class ServiceController : public QObject
{
    Q_OBJECT

public:
    explicit ServiceController(QObject *parent = nullptr);
    ~ServiceController() override;

    const ServiceState &state() const { return m_state; }
    bool isBusy() const { return !m_action.isEmpty(); }
    QString busyText() const { return m_action; }

    // The service program shipped next to this Oborcopy.
    static QString serviceBinary();
    static bool supportsPasswordUpdate();

public slots:
    void refresh();
    void install();
    void uninstall();
    void start();
    void stop();
    void restart();
    void pause();
    void resume();
    void updatePassword();
    // Points the installed service to this Oborcopy's service program.
    void repair();

signals:
    void stateChanged(const obor::ServiceState &state);
    void actionFinished(bool ok, const QString &message);

private:
    struct OsInfo
    {
        bool available = true;
        QString unavailableReason;
        bool installed = false;
        ServiceState::Kind kind = ServiceState::Kind::Stopped;
        QString binary;
        QString account;
        qint64 pid = 0;
        QString failure;
    };

    void apply(const OsInfo &os);
    void beginAction(const QString &text);
    void endAction(bool ok, const QString &message = QString());

#ifdef Q_OS_WIN
    OsInfo queryWindows() const;
    bool control(unsigned long code, QString *error);
    void runElevated(const QStringList &arguments, const QString &text, const QString &successMessage);
    void pollElevated();
    void *m_elevatedProcess = nullptr;
    QString m_resultFile;
    QString m_successMessage;
    QTimer *m_elevatedTimer = nullptr;
#else
    struct Command
    {
        QStringList argv;
        bool optional = false; // failure is reported as a hint, not as an error
    };
    void queryLinux();
    void runCommands(QList<Command> commands, const QString &text, const QString &successMessage,
                     std::function<void()> before = {});
    void runNext();
    bool writeUnitFile(QString *error) const;
    static QString unitFilePath();
    void signalService(int signal, const QString &text);
    bool m_querying = false;
    QList<Command> m_commands;
    QStringList m_hints;
    QString m_successMessage;
#endif

    ServiceState m_state;
    OsInfo m_os;
    QString m_action;
    QString m_lastError;
    bool m_restartPending = false;
    qint64 m_restartDeadline = 0;
    QTimer *m_timer;
};

} // namespace obor

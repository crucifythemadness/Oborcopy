// Administrative commands for the Windows service. They run elevated (started
// by the GUI through ShellExecuteEx "runas") and report via a result file.

#include "WinService.h"

#include "ServiceFiles.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QThread>

#include <windows.h>
#include <ntsecapi.h>
#include <objbase.h>
#include <sddl.h>
#include <wincred.h>

#include <optional>
#include <string>
#include <vector>

#ifndef CRED_PACK_GENERIC_CREDENTIALS
#define CRED_PACK_GENERIC_CREDENTIALS 0x4
#endif

namespace obor::WinService {

namespace {

enum Outcome { Ok = 0, Cancelled = 1, Failed = 2 };

class ScHandle
{
public:
    explicit ScHandle(SC_HANDLE handle = nullptr) : m_handle(handle) {}
    ~ScHandle() { reset(); }
    ScHandle(const ScHandle &) = delete;
    ScHandle &operator=(const ScHandle &) = delete;
    void reset(SC_HANDLE handle = nullptr)
    {
        if (m_handle)
            CloseServiceHandle(m_handle);
        m_handle = handle;
    }
    operator SC_HANDLE() const { return m_handle; }
    explicit operator bool() const { return m_handle != nullptr; }

private:
    SC_HANDLE m_handle;
};

struct Credentials
{
    QString account;
    std::wstring password;

    ~Credentials() { SecureZeroMemory(password.data(), password.size() * sizeof(wchar_t)); }
};

QString systemMessage(DWORD code)
{
    wchar_t *buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    const QString text = buffer ? QString::fromWCharArray(buffer).trimmed() : QString();
    LocalFree(buffer);
    return QStringLiteral("%1 (error %2)").arg(text).arg(code);
}

QString lastError()
{
    return systemMessage(GetLastError());
}

int finish(const QString &resultFile, Outcome outcome, const QString &message = QString())
{
    QFile file(resultFile);
    if (!resultFile.isEmpty() && file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        static const char *const tags[] = {"OK", "CANCELLED", "ERROR"};
        file.write(tags[outcome]);
        if (!message.isEmpty())
            file.write("\n" + message.toUtf8());
    }
    return outcome;
}

std::wstring w(const QString &s)
{
    return s.toStdWString();
}

QString computerName()
{
    wchar_t name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    return GetComputerNameW(name, &size) ? QString::fromWCharArray(name, int(size)) : QString();
}

// "name" -> ".\name"; "PC\name", ".\name" and "name@domain" stay.
QString normalizedAccount(const QString &account)
{
    if (account.contains(QLatin1Char('\\')) || account.contains(QLatin1Char('@')))
        return account;
    return QStringLiteral(".\\") + account;
}

std::vector<BYTE> sidFor(const QString &account, QString *error)
{
    QString name = account;
    if (name.startsWith(QLatin1String(".\\")))
        name = computerName() + name.mid(1);
    const std::wstring wname = w(name);
    DWORD sidSize = 0;
    DWORD domainSize = 0;
    SID_NAME_USE use;
    LookupAccountNameW(nullptr, wname.c_str(), nullptr, &sidSize, nullptr, &domainSize, &use);
    std::vector<BYTE> sid(sidSize ? sidSize : 1);
    std::wstring domain(domainSize ? domainSize : 1, L'\0');
    if (!LookupAccountNameW(nullptr, wname.c_str(), sid.data(), &sidSize, domain.data(), &domainSize, &use)) {
        *error = QStringLiteral("Unknown account '%1': %2").arg(account, lastError());
        return {};
    }
    return sid;
}

// Checks the password, so a typo is caught here and not when the service fails to log on.
bool verifyPassword(const Credentials &credentials, DWORD *error)
{
    QString domain;
    QString user = credentials.account;
    const int slash = user.indexOf(QLatin1Char('\\'));
    if (slash >= 0) {
        domain = user.left(slash);
        user = user.mid(slash + 1);
    }
    HANDLE token = nullptr;
    const std::wstring wuser = w(user);
    const std::wstring wdomain = w(domain);
    if (LogonUserW(wuser.c_str(), domain.isEmpty() ? nullptr : wdomain.c_str(), credentials.password.c_str(),
                   LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &token)) {
        CloseHandle(token);
        return true;
    }
    *error = GetLastError();
    return false;
}

std::optional<Credentials> promptCredentials(const QString &user, QString *error, bool *cancelled)
{
    const std::wstring caption = w(ServiceInfo::displayName());
    const std::wstring message =
        w(QStringLiteral("Enter the Windows password of %1.\n\nThe Oborcopy Scheduler service runs your "
                         "scheduled backups under this account, so it can reach the same folders and "
                         "network shares as you. Windows Hello PINs cannot be used here.")
              .arg(user));
    CREDUI_INFOW info{};
    info.cbSize = sizeof(info);
    info.pszCaptionText = caption.c_str();
    info.pszMessageText = message.c_str();

    std::wstring wuser = w(user);
    wchar_t empty[] = L"";
    DWORD inSize = 0;
    CredPackAuthenticationBufferW(CRED_PACK_GENERIC_CREDENTIALS, wuser.data(), empty, nullptr, &inSize);
    std::vector<BYTE> in(inSize);
    if (inSize == 0 || !CredPackAuthenticationBufferW(CRED_PACK_GENERIC_CREDENTIALS, wuser.data(), empty,
                                                      in.data(), &inSize))
        in.clear();

    DWORD authError = 0;
    for (int attempt = 0; attempt < 3; ++attempt) {
        ULONG package = 0;
        LPVOID out = nullptr;
        ULONG outSize = 0;
        BOOL save = FALSE;
        const DWORD rc = CredUIPromptForWindowsCredentialsW(&info, authError, &package,
                                                            in.empty() ? nullptr : in.data(), ULONG(in.size()),
                                                            &out, &outSize, &save, CREDUIWIN_GENERIC);
        if (rc == ERROR_CANCELLED) {
            *cancelled = true;
            return std::nullopt;
        }
        if (rc != ERROR_SUCCESS) {
            *error = systemMessage(rc);
            return std::nullopt;
        }
        wchar_t name[CREDUI_MAX_USERNAME_LENGTH + 1];
        wchar_t domain[CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1];
        wchar_t password[CREDUI_MAX_PASSWORD_LENGTH + 1];
        DWORD nameLen = CREDUI_MAX_USERNAME_LENGTH + 1;
        DWORD domainLen = CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1;
        DWORD passwordLen = CREDUI_MAX_PASSWORD_LENGTH + 1;
        const BOOL unpacked = CredUnPackAuthenticationBufferW(CRED_PACK_GENERIC_CREDENTIALS, out, outSize, name,
                                                              &nameLen, domain, &domainLen, password, &passwordLen);
        SecureZeroMemory(out, outSize);
        CoTaskMemFree(out);
        if (!unpacked) {
            *error = lastError();
            return std::nullopt;
        }
        Credentials credentials;
        credentials.account = normalizedAccount(QString::fromWCharArray(name));
        credentials.password.assign(password);
        SecureZeroMemory(password, sizeof(password));

        DWORD logonError = 0;
        if (credentials.password.empty()) {
            authError = ERROR_LOGON_FAILURE;
            continue; // Windows does not let services log on with an empty password
        }
        if (verifyPassword(credentials, &logonError) || logonError != ERROR_LOGON_FAILURE)
            return credentials;
        authError = ERROR_LOGON_FAILURE;
    }
    *error = QStringLiteral("The user name or password was not accepted.");
    return std::nullopt;
}

bool grantServiceLogonRight(std::vector<BYTE> &sid, QString *error)
{
    LSA_OBJECT_ATTRIBUTES attributes{};
    LSA_HANDLE policy = nullptr;
    NTSTATUS status = LsaOpenPolicy(nullptr, &attributes, POLICY_CREATE_ACCOUNT | POLICY_LOOKUP_NAMES, &policy);
    if (status != 0) {
        *error = QStringLiteral("Cannot open the security policy: %1").arg(systemMessage(LsaNtStatusToWinError(status)));
        return false;
    }
    wchar_t rightName[] = L"SeServiceLogonRight";
    LSA_UNICODE_STRING right;
    right.Buffer = rightName;
    right.Length = USHORT(wcslen(rightName) * sizeof(wchar_t));
    right.MaximumLength = USHORT(right.Length + sizeof(wchar_t));
    status = LsaAddAccountRights(policy, sid.data(), &right, 1);
    LsaClose(policy);
    if (status != 0) {
        *error = QStringLiteral("Cannot grant 'Log on as a service': %1").arg(systemMessage(LsaNtStatusToWinError(status)));
        return false;
    }
    return true;
}

QString sidString(std::vector<BYTE> &sid)
{
    LPWSTR text = nullptr;
    if (!ConvertSidToStringSidW(sid.data(), &text))
        return {};
    const QString result = QString::fromWCharArray(text);
    LocalFree(text);
    return result;
}

// Description, delayed auto-start, restart on failure, and a DACL that lets the
// given users start/stop/pause the service without elevation.
void configureService(SC_HANDLE service, const QStringList &controllerSids)
{
    std::wstring description = w(ServiceInfo::description());
    SERVICE_DESCRIPTIONW desc{description.data()};
    ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &desc);

    SERVICE_DELAYED_AUTO_START_INFO delayed{TRUE};
    ChangeServiceConfig2W(service, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delayed);

    SC_ACTION actions[3] = {{SC_ACTION_RESTART, 60000}, {SC_ACTION_RESTART, 60000}, {SC_ACTION_RESTART, 300000}};
    SERVICE_FAILURE_ACTIONSW failure{};
    failure.dwResetPeriod = 24 * 3600;
    failure.cActions = 3;
    failure.lpsaActions = actions;
    ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &failure);

    // Windows' default service DACL plus start/stop/pause/query for the user(s).
    QString sddl = QStringLiteral("D:(A;;CCLCSWRPWPDTLOCRRC;;;SY)(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;BA)"
                                  "(A;;CCLCSWLOCRRC;;;IU)(A;;CCLCSWLOCRRC;;;SU)");
    for (const QString &sid : controllerSids) {
        if (!sid.isEmpty())
            sddl += QStringLiteral("(A;;CCLCSWRPWPDTLOCRRC;;;%1)").arg(sid);
    }
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(w(sddl).c_str(), SDDL_REVISION_1, &sd, nullptr)) {
        SetServiceObjectSecurity(service, DACL_SECURITY_INFORMATION, sd);
        LocalFree(sd);
    }
}

bool waitForState(SC_HANDLE service, DWORD state, int timeoutMs)
{
    for (int waited = 0; waited <= timeoutMs; waited += 250) {
        SERVICE_STATUS status{};
        if (!QueryServiceStatus(service, &status))
            return false;
        if (status.dwCurrentState == state)
            return true;
        QThread::msleep(250);
    }
    return false;
}

QString binaryPath()
{
    return QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
}

bool startService(SC_HANDLE service, QString *error)
{
    if (StartServiceW(service, 0, nullptr) || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
        return true;
    *error = lastError();
    return false;
}

} // namespace

int install(const QString &user, const QString &resultFile)
{
    QString error;
    bool cancelled = false;
    std::vector<BYTE> userSid = sidFor(user, &error);
    if (userSid.empty())
        return finish(resultFile, Failed, error);
    const auto credentials = promptCredentials(user, &error, &cancelled);
    if (!credentials)
        return finish(resultFile, cancelled ? Cancelled : Failed, error);
    std::vector<BYTE> serviceSid = sidFor(credentials->account, &error);
    if (serviceSid.empty() || !grantServiceLogonRight(serviceSid, &error))
        return finish(resultFile, Failed, error);

    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
    if (!scm)
        return finish(resultFile, Failed, QStringLiteral("Cannot open the service manager: %1").arg(lastError()));
    const std::wstring name = w(ServiceInfo::serviceName());
    const std::wstring display = w(ServiceInfo::displayName());
    const std::wstring path = w(binaryPath());
    const std::wstring account = w(credentials->account);
    ScHandle service(CreateServiceW(scm, name.c_str(), display.c_str(), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                                    SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path.c_str(), nullptr, nullptr,
                                    nullptr, account.c_str(), credentials->password.c_str()));
    if (!service) {
        if (GetLastError() != ERROR_SERVICE_EXISTS)
            return finish(resultFile, Failed, QStringLiteral("Cannot create the service: %1").arg(lastError()));
        service.reset(OpenServiceW(scm, name.c_str(), SERVICE_ALL_ACCESS));
        if (!service
            || !ChangeServiceConfigW(service, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                                     path.c_str(), nullptr, nullptr, nullptr, account.c_str(),
                                     credentials->password.c_str(), display.c_str()))
            return finish(resultFile, Failed, QStringLiteral("Cannot update the service: %1").arg(lastError()));
    }
    configureService(service, {sidString(userSid), sidString(serviceSid)});
    if (!startService(service, &error))
        return finish(resultFile, Failed, QStringLiteral("The service was installed but could not start: %1").arg(error));
    return finish(resultFile, Ok);
}

int uninstall(const QString &resultFile)
{
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm)
        return finish(resultFile, Failed, QStringLiteral("Cannot open the service manager: %1").arg(lastError()));
    const std::wstring name = w(ServiceInfo::serviceName());
    ScHandle service(OpenServiceW(scm, name.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE));
    if (!service) {
        if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
            return finish(resultFile, Ok);
        return finish(resultFile, Failed, lastError());
    }
    SERVICE_STATUS status{};
    if (ControlService(service, SERVICE_CONTROL_STOP, &status))
        waitForState(service, SERVICE_STOPPED, 60000);
    if (!DeleteService(service) && GetLastError() != ERROR_SERVICE_MARKED_FOR_DELETE)
        return finish(resultFile, Failed, QStringLiteral("Cannot remove the service: %1").arg(lastError()));
    return finish(resultFile, Ok);
}

int updatePassword(const QString &user, const QString &resultFile)
{
    QString error;
    bool cancelled = false;
    const auto credentials = promptCredentials(user, &error, &cancelled);
    if (!credentials)
        return finish(resultFile, cancelled ? Cancelled : Failed, error);
    std::vector<BYTE> serviceSid = sidFor(credentials->account, &error);
    if (serviceSid.empty() || !grantServiceLogonRight(serviceSid, &error))
        return finish(resultFile, Failed, error);

    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    const std::wstring name = w(ServiceInfo::serviceName());
    ScHandle service(scm ? OpenServiceW(scm, name.c_str(), SERVICE_CHANGE_CONFIG | SERVICE_START | SERVICE_QUERY_STATUS)
                         : nullptr);
    if (!service)
        return finish(resultFile, Failed, QStringLiteral("Cannot open the service: %1").arg(lastError()));
    const std::wstring account = w(credentials->account);
    if (!ChangeServiceConfigW(service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, nullptr, nullptr,
                              nullptr, nullptr, account.c_str(), credentials->password.c_str(), nullptr))
        return finish(resultFile, Failed, QStringLiteral("Cannot change the password: %1").arg(lastError()));
    if (!startService(service, &error))
        return finish(resultFile, Failed, QStringLiteral("Password saved, but the service could not start: %1").arg(error));
    return finish(resultFile, Ok);
}

int setPath(const QString &resultFile)
{
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    const std::wstring name = w(ServiceInfo::serviceName());
    ScHandle service(scm ? OpenServiceW(scm, name.c_str(),
                                        SERVICE_CHANGE_CONFIG | SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS)
                         : nullptr);
    if (!service)
        return finish(resultFile, Failed, QStringLiteral("Cannot open the service: %1").arg(lastError()));
    SERVICE_STATUS status{};
    QueryServiceStatus(service, &status);
    const bool wasRunning = status.dwCurrentState != SERVICE_STOPPED;
    const std::wstring path = w(binaryPath());
    if (!ChangeServiceConfigW(service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, path.c_str(),
                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr))
        return finish(resultFile, Failed, QStringLiteral("Cannot update the service: %1").arg(lastError()));
    if (wasRunning) {
        if (ControlService(service, SERVICE_CONTROL_STOP, &status))
            waitForState(service, SERVICE_STOPPED, 60000);
        QString error;
        if (!startService(service, &error))
            return finish(resultFile, Failed, QStringLiteral("Updated, but the service could not start: %1").arg(error));
    }
    return finish(resultFile, Ok);
}

} // namespace obor::WinService

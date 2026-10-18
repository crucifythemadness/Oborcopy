# End-to-end test of the Linux scheduler service in Docker: a container with
# systemd as PID 1, a normal user with a systemd user session, the release
# package unpacked in the user's home and oborcopy-servicectl-test driving the
# real Service Manager code (install, scheduled run, retry, pause, resume, stop,
# start, restart, uninstall).
# Usage: .\scripts\test-linux-service.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$image = "oborcopy-systemd-test"
$name = "oborcopy-systemd-test"

function Invoke-Docker {
    # docker prints progress on stderr; Windows PowerShell must not treat that as an error.
    $ErrorActionPreference = "Continue"
    & docker @args 2>&1 | ForEach-Object { "$_" }
    $global:DockerExit = $LASTEXITCODE
    $ErrorActionPreference = "Stop"

}

function Invoke-AsTester([string]$Command) {
    $run = "/run/user/$global:TesterUid"
    Invoke-Docker exec -u tester -e "XDG_RUNTIME_DIR=$run" `
        -e "DBUS_SESSION_BUS_ADDRESS=unix:path=$run/bus" -e USER=tester -e HOME=/home/tester `
        $name bash -lc $Command
}

Push-Location $root
try {
    Invoke-Docker build -f docker/Dockerfile.linux --target systemd-test -t $image . | Select-Object -Last 3
    if ($global:DockerExit -ne 0) { throw "Image build failed" }
    Invoke-Docker rm -f $name | Out-Null
    Invoke-Docker run -d --name $name --privileged --cgroupns=host -v /sys/fs/cgroup:/sys/fs/cgroup:rw `
        --tmpfs /run --tmpfs /run/lock $image | Out-Null
    if ($global:DockerExit -ne 0) { throw "Container did not start" }

    # Wait for systemd, then give 'tester' a user session (linger = as after a login).
    Invoke-Docker exec $name bash -c "for i in `$(seq 60); do systemctl is-system-running 2>/dev/null | grep -qE 'running|degraded' && exit 0; sleep 1; done; exit 1" | Out-Null
    $global:TesterUid = (Invoke-Docker exec $name id -u tester | Select-Object -Last 1).Trim()
    Invoke-Docker exec $name loginctl enable-linger tester | Out-Null
    Invoke-Docker exec $name bash -c "for i in `$(seq 60); do [ -S /run/user/$global:TesterUid/bus ] && exit 0; sleep 1; done; exit 1" | Out-Null
    if ($global:DockerExit -ne 0) { throw "No systemd user session for 'tester'" }

    Write-Host "`n=== Service test (takes about 4 minutes) ===" -ForegroundColor Cyan
    $output = Invoke-AsTester 'cd ~/pkg/oborcopy-*/bin && ./oborcopy-servicectl-test'
    $output | ForEach-Object { Write-Host $_ }
    $passed = ($output -join "`n") -match "ALL PASSED"

    Write-Host "`n=== Service log ===" -ForegroundColor Cyan
    Invoke-AsTester 'cat ~/.config/Oborcopy/service/service.log 2>/dev/null | tail -40' | ForEach-Object { Write-Host $_ }
    Write-Host "`n=== journalctl --user -u oborcopy ===" -ForegroundColor Cyan
    Invoke-AsTester 'journalctl --user -u oborcopy --no-pager -n 25 2>/dev/null' | ForEach-Object { Write-Host $_ }

    if (-not $passed) { throw "Linux service test FAILED" }
    Write-Host "`nLinux service test passed." -ForegroundColor Green
}
finally {
    Invoke-Docker rm -f $name | Out-Null
    Pop-Location
}

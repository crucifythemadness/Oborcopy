# Oborcopy

A friendly GUI for incremental copying and mirroring of folders – the most important
features of *Robocopy* and *rsync*, with a preview before anything is deleted.

* **Update** – copy new and changed files, never delete
* **Mirror** – make the destination an exact copy (deletes extras, with preview and confirmation)
* FAT/exFAT timestamp tolerance, retries for locked files, pause/resume/cancel
* Live progress, log file per run, detailed summary tree, run history
* Job profiles in JSON, light/dark theme, command-line tool
* **Oborcopy Scheduler** – background service (Windows service / systemd user service) that runs
  jobs hourly, daily, weekly or monthly, also when the GUI is closed; managed from the GUI (`Ctrl+M`)

## Build

Windows (Qt 6.10.2 MinGW kit in `C:\Qt`):

```powershell
.\scripts\build-windows.ps1          # configure, build, run tests
.\build\windows-mingw\Oborcopy.exe   # start the GUI (Qt bin folder must be on PATH)
```

Linux (via Docker Desktop):

```powershell
.\scripts\build-linux.ps1            # builds + tests in Ubuntu 24.04, binaries in build\linux-out
.\scripts\test-linux-service.ps1     # end-to-end test of the scheduler service with real systemd
```

## Deploy (ready-to-run packages)

```powershell
.\scripts\deploy-windows.ps1   # -> deploy-win64\  + Oborcopy-<version>-win64.zip
.\scripts\deploy-linux.ps1     # -> deploy-linux\oborcopy-<version>-linux-x86_64\ + .tar.gz (Qt bundled)
```

## Command line

```
oborcopy-cli --list-jobs
oborcopy-cli --job "Music Backup" --dry-run
oborcopy-cli --job "Music Backup" --yes
```

## Documentation

* **User Guide:** [docs/user-guide/Oborcopy-User-Guide.md](docs/user-guide/Oborcopy-User-Guide.md)
  (PDF: [Oborcopy-User-Guide.pdf](docs/user-guide/Oborcopy-User-Guide.pdf), rebuilt with `.\scripts\build-user-guide.ps1`)
* **Architecture:** [Oborcopy-SAD.md](Oborcopy-SAD.md)

## License

Oborcopy is free software under the [GNU General Public License v3.0](LICENSE).
It uses the [Qt framework](https://www.qt.io/) under the GNU LGPL v3.

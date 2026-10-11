# Oborcopy

A friendly GUI for incremental copying and mirroring of folders – the most important
features of *Robocopy* and *rsync*, with a preview before anything is deleted.

* **Update** – copy new and changed files, never delete
* **Mirror** – make the destination an exact copy (deletes extras, with preview and confirmation)
* FAT/exFAT timestamp tolerance, retries for locked files, pause/resume/cancel
* Live progress, log file per run, detailed summary tree, run history
* Job profiles in JSON, light/dark theme, command-line tool for scheduled runs

## Build

Windows (Qt 6.10.2 MinGW kit in `C:\Qt`):

```powershell
.\scripts\build-windows.ps1          # configure, build, run tests
.\build\windows-mingw\Oborcopy.exe   # start the GUI (Qt bin folder must be on PATH)
```

Linux (via Docker Desktop):

```powershell
.\scripts\build-linux.ps1            # builds + tests in Ubuntu 24.04, binaries in build\linux-out
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

See [Oborcopy-SAD.md](Oborcopy-SAD.md) for the architecture.

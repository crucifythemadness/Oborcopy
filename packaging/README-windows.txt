Oborcopy @VERSION@ for Windows 10/11 (64-bit)
=============================================

Oborcopy copies and mirrors folders incrementally (like Robocopy / rsync),
with a preview before anything is deleted.

START
  Double-click Oborcopy.exe. No installation is needed; keep all files of
  this folder together (the DLLs and sub-folders are required).

COMMAND LINE (for scheduled backups)
  oborcopy-cli.exe --list-jobs
  oborcopy-cli.exe --job "Music Backup" --dry-run     (show what would happen)
  oborcopy-cli.exe --job "Music Backup" --yes         (run, allow deletions in Mirror mode)
  oborcopy-cli.exe --help

  Exit codes: 0 nothing to do, 1 files copied, 2 extras deleted,
              8 some items failed, 16 fatal error / aborted (bits can combine)

  Weekly run with the Windows Task Scheduler (example, Sunday 20:00):
    schtasks /Create /TN "Oborcopy Music Backup" /SC WEEKLY /D SUN /ST 20:00 ^
      /TR "\"C:\Tools\Oborcopy\oborcopy-cli.exe\" --job \"Music Backup\" --yes --quiet"

WHERE ARE MY SETTINGS?
  Jobs      %LOCALAPPDATA%\Oborcopy\jobs\*.json
  Settings  %LOCALAPPDATA%\Oborcopy\settings.json
  Logs      %LOCALAPPDATA%\Oborcopy\logs\  (one .log and one .json report per run)

  Portable use: set the environment variable OBORCOPY_CONFIG_DIR to a folder
  (e.g. on a USB stick) and everything is stored there.

LICENSES
  Oborcopy uses the Qt framework under the GNU LGPL v3, see LICENSES\.

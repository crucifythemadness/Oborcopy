Oborcopy @VERSION@ for Linux (x86_64)
=====================================

Oborcopy copies and mirrors folders incrementally (like Robocopy / rsync),
with a preview before anything is deleted.

REQUIREMENTS
  64-bit Linux with glibc 2.39 or newer and a graphical desktop (X11 or
  Wayland), e.g. Ubuntu 24.04+, Linux Mint 22+, Debian 13+, Fedora 40+.
  Qt is bundled - no extra packages are needed.

USER GUIDE
  Oborcopy-User-Guide.pdf in this folder explains everything step by step.

START
  tar xzf oborcopy-@VERSION@-linux-x86_64.tar.gz
  cd oborcopy-@VERSION@-linux-x86_64
  ./oborcopy

  Optional: ./install-desktop-entry.sh adds Oborcopy to your application
  menu (for the current user, pointing to this folder). Keep the folder
  where it is afterwards, or run the script again after moving it.

SCHEDULED BACKUPS (Oborcopy Scheduler service)
  In Oborcopy: Service > Service Manager (Ctrl+M) > Install service.
  This creates the systemd user service "oborcopy.service" for your account
  (no root needed) which runs your scheduled jobs - also when Oborcopy is
  closed. Check it with:  systemctl --user status oborcopy
  See chapter 10 of the user guide. Before deleting or moving this folder,
  uninstall the service in the Service Manager (or click "Use this Oborcopy"
  there after moving it).

COMMAND LINE (for scripts or your own scheduler)
  ./oborcopy-cli --list-jobs
  ./oborcopy-cli --job "Music Backup" --dry-run     (show what would happen)
  ./oborcopy-cli --job "Music Backup" --yes         (run, allow deletions in Mirror mode)
  ./oborcopy-cli --help

  Exit codes: 0 nothing to do, 1 files copied, 2 extras deleted,
              8 some items failed, 16 fatal error / aborted (bits can combine)

  Weekly run with cron (crontab -e), Sunday 20:00:
    0 20 * * 0  /opt/oborcopy/oborcopy-cli --job "Music Backup" --yes --quiet

WHERE ARE MY SETTINGS?
  Jobs      ~/.config/Oborcopy/jobs/*.json
  Settings  ~/.config/Oborcopy/settings.json
  Logs      ~/.local/share/Oborcopy/logs/  (one .log and one .json report per run)
  Service   ~/.config/Oborcopy/service/  (schedule, status, service.log)

  Portable use: set OBORCOPY_CONFIG_DIR to a folder and everything is stored there.

TROUBLESHOOTING
  "could not load the Qt platform plugin xcb": install your distribution's
  basic X11/OpenGL libraries (e.g. Ubuntu: sudo apt install libgl1 libegl1
  libxkbcommon-x11-0 libfontconfig1).

LICENSES
  Oborcopy uses the Qt framework under the GNU LGPL v3, see LICENSES/.

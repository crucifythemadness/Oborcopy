#!/bin/sh
# Adds Oborcopy to the application menu of the current user.
# Run it again after moving this folder; remove with: rm ~/.local/share/applications/oborcopy.desktop
set -e
HERE="$(dirname "$(readlink -f "$0")")"
APPS="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
mkdir -p "$APPS"
cat > "$APPS/oborcopy.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Oborcopy
GenericName=Backup and folder sync
Comment=Incremental copy and mirror tool
Exec="$HERE/oborcopy"
Icon=$HERE/share/oborcopy.png
Terminal=false
Categories=Utility;Archiving;FileTools;
StartupWMClass=oborcopy
EOF
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APPS" || true
echo "Menu entry created: $APPS/oborcopy.desktop"

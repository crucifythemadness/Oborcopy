#!/bin/sh
# Starts the bundled Oborcopy GUI. Uses X11 (also works under Wayland via XWayland).
HERE="$(dirname "$(readlink -f "$0")")"
unset QT_PLUGIN_PATH
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
exec "$HERE/bin/oborcopy" "$@"

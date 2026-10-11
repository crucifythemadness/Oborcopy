#!/bin/sh
# Starts the bundled Oborcopy command-line tool.
HERE="$(dirname "$(readlink -f "$0")")"
unset QT_PLUGIN_PATH
exec "$HERE/bin/oborcopy-cli" "$@"

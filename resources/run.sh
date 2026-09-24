#!/bin/sh
#
# The Terminal App's start: what every App gets (the launcher's rc/app_env.sh: the App's folder and root,
# the binary for this machine, a home on the stick), then - on the console - the launcher's own SDL2 family
# on the library path: the terminal is drawn with the AutoBleem SDK, built against the SDL2 in
# Autobleem/lib/libs.tar.gz (unpacked to /tmp/lib at boot), and app_env.sh's library path names only the
# Apps' libs pack.
#
. "$(dirname "$0")/../../Autobleem/rc/app_env.sh"

if [ "$AB_APP_KEY" = psc ] && [ -d /tmp/lib ]; then
    LD_LIBRARY_PATH="/tmp/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export LD_LIBRARY_PATH
fi

if [ -z "$AB_APP_EXEC" ]; then
    echo "terminal: no program for this machine in $AB_APP_DIR" >&2
    exit 1
fi
cd "$AB_APP_DIR" || exit 1
exec "$AB_APP_EXEC" "$@"

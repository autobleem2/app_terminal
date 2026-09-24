#!/bin/sh
#
# The Terminal App's start, under any launcher.
#
# A launcher with multi-platform Apps (rc/app_resolve.sh next to rc/app_env.sh) has resolved app.ini's
# Exec= for this machine already (AB_APP_EXEC), and its app_env.sh sets up what every App gets: the folder
# and root, a home on the stick, and no virtual pad for us (VirtualPad=false). An older launcher knows only
# Startup=, so it runs this script alone. Its app_env.sh would preload the virtual pad into the terminal,
# which reads the pads itself. So this script does the little the terminal needs by itself: the folder,
# the root, the home, and the program for this machine.
#
# Either way, on the console the launcher's own SDL2 family (Autobleem/lib/libs.tar.gz, unpacked to /tmp/lib
# at boot) goes first on the library path: the terminal is drawn with the AutoBleem SDK, built against it.
#
AB_APP_DIR=${AB_APP_DIR:-$(cd "$(dirname "$0")" && pwd)}
AB_ROOT=${AB_ROOT:-$(cd "$AB_APP_DIR/../.." && pwd)}
export AB_APP_DIR AB_ROOT

if [ -f "$AB_ROOT/Autobleem/rc/app_resolve.sh" ] && [ -f "$AB_ROOT/Autobleem/rc/app_env.sh" ]; then
    . "$AB_ROOT/Autobleem/rc/app_env.sh"
else
    HOME="$AB_ROOT/Home"
    XDG_CONFIG_HOME="$HOME/.config"
    export HOME XDG_CONFIG_HOME
    mkdir -p "$XDG_CONFIG_HOME" 2>/dev/null
fi

# the program: what the launcher resolved, else this machine's own folder under bin/
if [ -z "$AB_APP_EXEC" ]; then
    if [ -d /usr/sony ]; then
        AB_APP_KEY=psc
    else
        case "$(uname -m)" in
            aarch64) AB_APP_KEY=rpi64 ;;
            arm*) AB_APP_KEY=rpi ;;
            i?86 | x86_64) AB_APP_KEY=pcusb ;;
        esac
    fi
    AB_APP_EXEC="$AB_APP_DIR/bin/$AB_APP_KEY/terminal"
    export AB_APP_KEY AB_APP_EXEC
fi

if [ "$AB_APP_KEY" = psc ] && [ -d /tmp/lib ]; then
    LD_LIBRARY_PATH="/tmp/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export LD_LIBRARY_PATH
fi

if [ ! -x "$AB_APP_EXEC" ]; then
    echo "terminal: no program for this machine ($AB_APP_EXEC)" >&2
    exit 1
fi
cd "$AB_APP_DIR" || exit 1
exec "$AB_APP_EXEC" "$@"

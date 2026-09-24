#!/usr/bin/env bash
# Builds the Terminal App in the autobleem-build image (ghcr.io/autobleem2/autobleem-build) and packages it:
#
#   ci/build.sh native            host build + the tests (build_linux/)
#   ci/build.sh psc|rpi|rpi64|pcusb   a target, checked and packed into dist/terminal-<key>-<version>.zip
#   ci/build.sh all               every one of them
#
# A package is the App's folder as a stick has it - Apps/terminal/app.ini, the shared files, and
# bin/<key>/terminal for its one platform - which is what the Store's AppInstaller lays over Apps/terminal/
# (keeping any other platform's bin/<key>/). The version is app.ini's Version=, or AB_VERSION.
#
# On the build server: docker run --rm -u $(id -u):$(id -g) -v $PWD:/src -w /src \
#                          ghcr.io/autobleem2/autobleem-build:develop ci/build.sh all
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="${AB_VERSION:-$(sed -n 's/^Version=//p' resources/app.ini | tr -d '\r')}"
JOBS="${JOBS:-$(nproc)}"
LAUNCHER=()
if [ -z "${AB_NO_SCCACHE:-}" ] && command -v sccache >/dev/null 2>&1; then
    LAUNCHER=(-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache)
fi

banner() { printf '\n==== %s ====\n' "$*"; }

configure() { # configure <dir> <cmake args...>
    local dir="$1"
    shift
    cmake -S . -B "$dir" -G Ninja "${LAUNCHER[@]}" "$@" >/dev/null
}

package() { # package <build dir> <key>
    local dir="$1" key="$2"
    local stage="$dir/Apps/terminal"
    local exe="$stage/bin/$key/terminal"
    test -x "$exe" || { echo "no $exe" >&2; exit 1; }
    sed -i "s/^Version=.*/Version=$VERSION/" "$stage/app.ini"
    # the console's tools and the Pi's: a smaller file on a slow stick (the launcher packs its binaries too)
    if command -v upx >/dev/null 2>&1 && [ -z "${AB_NO_UPX:-}" ]; then
        upx -q --best --lzma "$exe" >/dev/null || echo "upx: left unpacked"
    fi
    mkdir -p dist
    local zip="dist/terminal-$key-$VERSION.zip"
    rm -f "$zip"
    (cd "$dir" && python3 - "$OLDPWD/$zip" <<'EOF'
import os, sys, zipfile
# every file under Apps/terminal, with its mode (the program stays executable where the filesystem keeps it)
with zipfile.ZipFile(sys.argv[1], "w", zipfile.ZIP_DEFLATED) as z:
    for root, dirs, files in os.walk("Apps/terminal"):
        dirs.sort()
        for name in sorted(files):
            z.write(os.path.join(root, name))
EOF
    )
    ls -l "$zip"
}

build_native() {
    banner "native: build + tests (build_linux)"
    configure build_linux -DCMAKE_BUILD_TYPE=Debug
    ninja -C build_linux -j "$JOBS"
    ctest --test-dir build_linux --output-on-failure -j "$JOBS"
}

build_psc() {
    banner "psc: the console (build_psc)"
    rm -rf build_psc/Apps
    configure build_psc -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=toolchains/psc/PSCtoolchainV8.cmake \
        -DAB_PSC_TOOLCHAIN="${AB_PSC_TOOLCHAIN:-/opt/psc}"
    ninja -C build_psc -j "$JOBS"
    # the console's glibc 2.24 / GLIBCXX 3.4.22, no RPATH
    bash tools/check_psc_binary.sh build_psc/Apps/terminal/bin/psc/terminal "${AB_PSC_TOOLCHAIN:-/opt/psc}"
    package build_psc psc
}

build_rpi() { # build_rpi rpi|rpi64
    local key="$1" dir="build_$1" toolchain=toolchains/rpi/RPitoolchain.cmake
    [ "$key" = rpi64 ] && toolchain=toolchains/rpi64/RPi64toolchain.cmake
    banner "$key: Raspberry Pi ($dir)"
    rm -rf "$dir/Apps"
    configure "$dir" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$toolchain"
    ninja -C "$dir" -j "$JOBS"
    file "$dir/Apps/terminal/bin/$key/terminal"
    package "$dir" "$key"
}

build_pcusb() {
    banner "pcusb: the 32-bit PC stick (build_pcusb)"
    rm -rf build_pcusb/Apps
    configure build_pcusb -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=toolchains/pcusb/PcUsbToolchain.cmake
    ninja -C build_pcusb -j "$JOBS"
    file build_pcusb/Apps/terminal/bin/pcusb/terminal | grep -q 'ELF 32-bit LSB.*Intel 80386'
    # i386 runs here: the suites again, on the stick's own architecture
    ctest --test-dir build_pcusb --output-on-failure -j "$JOBS"
    package build_pcusb pcusb
}

[ $# -gt 0 ] || { echo "usage: $0 native|psc|rpi|rpi64|pcusb|all" >&2; exit 2; }
for target in "$@"; do
    case "$target" in
        native) build_native ;;
        psc) build_psc ;;
        rpi | rpi64) build_rpi "$target" ;;
        pcusb) build_pcusb ;;
        all) build_native; build_psc; build_rpi rpi; build_rpi rpi64; build_pcusb ;;
        *) echo "unknown target: $target" >&2; exit 2 ;;
    esac
done

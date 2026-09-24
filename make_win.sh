#!/usr/bin/env bash
# Windows development build with MSYS2 UCRT64 (gcc, cmake, ninja, SDL2 packages), as the launcher's
# make_win.sh: the dev build (AB_TARGET=dev) into build_win/, the App staged in build_win/Apps/terminal/
# with its program in bin/dev/, then the tests. On Windows the shell runs through ConPTY - MSYS2's bash
# when it is installed, else cmd.exe - which is enough to work on the screen; bash on a pty is Linux's.
#
#   ./make_win.sh              build + ctest
#   ./make_win.sh --no-tests   build only
#
# Run from an MSYS2 UCRT64 shell, or:
#   C:\msys64\usr\bin\bash.exe -lc "cd /e/Programming/app_terminal && ./make_win.sh"
set -e
cd "$(dirname "$0")"

RUN_TESTS=1
for arg in "$@"; do
    case "$arg" in
        --no-tests) RUN_TESTS=0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

LAUNCHER=()
if [ -z "$AB_NO_SCCACHE" ] && command -v sccache >/dev/null 2>&1; then
    LAUNCHER=(-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache)
fi

mkdir -p build_win
cd build_win
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DAB_TARGET=dev "${LAUNCHER[@]}" ../
ninja
if [ "$RUN_TESTS" = 1 ]; then ctest --output-on-failure; fi

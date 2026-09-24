# Terminal - an AutoBleem App

A terminal emulator for AutoBleem. It runs bash with colours (256 colours, truecolor, bold, underline,
line drawing), drawn in your AutoBleem theme. It runs on the PlayStation Classic, a Raspberry Pi (32 and
64-bit) and the PC stick. Type with a USB keyboard, or with the pad on the on-screen keyboard.
`resources/readme.txt` has the controls.

It is installed from the AutoBleem Store, or by unpacking `terminal-<platform>-<version>.zip` onto the
stick's root: every package holds `Apps/terminal/` with the shared files and `bin/<platform>/terminal`,
and packages for different platforms merge into the one folder.

## Building

```bash
git clone --recurse-submodules https://github.com/autobleem2/app_terminal.git
```

- Windows (dev, MSYS2 UCRT64): `./make_win.sh`. The App is staged in `build_win/Apps/terminal/`.
- Every target, in the autobleem-build image:

```bash
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp -v $PWD:/src -w /src ghcr.io/autobleem2/autobleem-build:develop ci/build.sh all
```

The packages land in `dist/`. GPL-3.0-or-later; the font is DejaVu Sans Mono (`resources/fonts/DejaVu-LICENSE.txt`).

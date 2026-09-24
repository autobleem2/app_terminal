# app_terminal - developer context

**Terminal** is an AutoBleem App: a terminal emulator that runs bash (or the machine's shell) on a
pseudo-terminal and draws it with the AutoBleem SDK's classic UI, in the user's theme and language. It is
typed on with a USB keyboard, or with the pad through its own on-screen keyboard. It is the first App made
for the AutoBleem Store (the launcher's `docs/store-plan.md`): a one-platform zip per target, in the
multi-platform App format (`docs/app-format-plan.md`). Started 2026-09-24. An App's repository is named
`app_<name>` (the owner's rule); the folder it installs to is `Apps/terminal/`, its Store id `app/terminal`.

## Layout

| path | what |
|---|---|
| `src/core/vt_screen.*` | `term::VtScreen` - the terminal without a picture: the xterm subset that xterm-256color's terminfo promises (C0, ESC, CSI cursor/erase/insert/delete/scroll, the scrolling region, the alternate screen 47/1047/1049, SGR with 16/256/24-bit colour and the colon forms, DEC line drawing via G0/G1 + SO/SI, DECCKM/DECKPAM/DECTCEM/DECAWM/DECOM/bracketed paste, DSR/DA answers through `onReply`, OSC 0/2 titles, DCS/APC/PM/SOS skipped, REP). The scrollback (main screen only), `viewLine(row, offset)` to look into it, per-row dirty flags and a `version()` counter for the view. `resize()` drops blank rows under the cursor first, then pushes rows from the top into the scrollback, and takes them back when the screen grows - so the pad keyboard opening and closing leaves the text where it was. No reflow of long lines. Every character is one cell wide (no CJK double width). |
| `src/core/key_encoder.*` | `term::KeyEncoder` - keys as xterm sends them: arrows/Home/End by DECCKM, xterm's `1;<mod>` modifier parameter, `~` keys, F1-F12, Ctrl+char control codes, Alt as an ESC prefix, bracketed paste. |
| `src/core/pty_process.*` | `term::PtyProcess` - POSIX pty (`posix_openpt`, `setsid` + `TIOCSCTTY`, every fd >= 3 closed and the signals reset in the child, non-blocking master, `TIOCSWINSZ`); on Windows (the dev host only) a ConPTY pseudo console, looked up at run time. `read()` never blocks; -1 once the program is gone (EIO waits up to 1 s for the exit status - the child may not be reaped yet). `terminate()` = SIGHUP to the group, SIGKILL after a grace. `environment(set)` = our env with changes. |
| `src/core/shell_launch.*` | `term::ShellLaunch::plan(ShellFacts)` - which shell with what: Linux `/bin/bash --rcfile <app>/bashrc -i`, else `$SHELL`, else `/bin/sh -i`; Windows MSYS2's bash `--login -i` or cmd.exe. TERM=xterm-256color, COLORTERM=truecolor, LD_PRELOAD and the pad variables removed, `TERMINFO_DIRS=:<app>/terminfo` (the system's first), cwd = `$HOME` (the stick's `Home/`). |
| `src/gui/terminal_view.*` | Draws a VtScreen into a render target the size of the grid, only the dirty rows; runs of ASCII in one colour/weight are one draw call, anything else a glyph at its own cell. Bold brightens colours 0-7. The cursor is a steady block (no blink: an idle terminal draws no frames - on the Xvfb test 20% CPU -> 0.5%). |
| `src/gui/pad_keyboard.*` | The on-screen keyboard: 5 rows, 15 units, Esc/Tab/Ctrl/Shift/Alt/Enter/arrows/PgUp/PgDn; modifiers latch for one key, twice locks. Labels are key legends, not translated. |
| `src/gui/gui_terminal.*` | The screen (a full classic panel, or the whole screen): the loop reads up to 256 KB of output per frame, renders only when something changed, `delay(8)` otherwise. Pad mapping with and without the keyboard, the USB keyboard (`Input::setRawKeyboard` + `setKeyboardAsPad(false)`), Select's `GuiActionMenu` (keyboard, text size, full screen, Ctrl+C/D/Z, close), L2/R2 and Shift+PgUp/PgDn scroll back. `exit` (status 0) closes; any other end waits for a press. |
| `src/gui/terminal_settings.*` | `<XDG_CONFIG_HOME or HOME/.config>/autobleem-terminal/terminal.ini`: fontsize (12-36), fullscreen, keyboard, scrollback. |
| `src/terminal_app.*`, `src/main.cpp` | `TerminalApp : AppBase("Terminal")` - the main GUI's config/theme/language, the App's `lang/` over it, the theme's music stopped. `main`: the App dir from `AB_APP_DIR`, else the folder above `bin/<key>/`; the root from the argument, `AB_ROOT`, `/media` (psc) or `<app>/../..`. `--shell <program>` / `AB_TERMINAL_SHELL` pick the shell. Log: `System/Logs/terminal.log`. |
| `resources/` | What the package ships next to `bin/<key>/`: `app.ini` (`Exec=bin/{key}/terminal`, `Startup=run.sh`, `VirtualPad=false`), `run.sh` (app_env.sh, then `/tmp/lib` - the launcher's SDL2 2.0.14 - on the console's library path: app_env.sh names only the Apps' libs pack), `bashrc`, `fonts/DejaVuSansMono{,-Bold}.ttf` + licence (Bitstream Vera licence, from Debian's fonts-dejavu-core 2.37), `terminfo/x/xterm-256color`, `lang/`, `icon.png`, `readme.txt`. |
| `tools/` | `make_lang.py` (the 29 strings x 16 languages - edit the table, rerun), `make_icon.py` (draws icon.png, Pillow), `check_psc_binary.sh` (from the console tools), `xterm-256color.src` (the terminfo source). |
| `ci/build.sh` | `native|psc|rpi|rpi64|pcusb|all` in the autobleem-build image -> `dist/terminal-<key>-<version>.zip`. |
| `toolchains/` | Copies of the launcher's psc/rpi/rpi64/pcusb toolchain files (the Pi's SysGCC mode needs the launcher's `sdl2-devkit`, not copied: build Pi targets in the image). |

## Things to know

- **The core change it needed** (autobleem-core `c28402c`, merged to develop as `1ac8733`, 2026-09-24):
  `ableem::Event` gained `mods`/`code`, `Key` Insert and F1-F12, `Input::setRawKeyboard()` (Esc as a key,
  not power off); the DebugDriver's `key ctrl+c`. **AB_SDK_ABI 2.**
- **terminfo**: the console's ncurses is old (Stretch-era, 5.x/6.0 readers); Bookworm's `tic` writes
  xterm-256color in the extended-number format those cannot read because of `pairs#0x10000`. The shipped
  entry is compiled from `tools/xterm-256color.src` with `pairs#0x7fff` - legacy format (magic `1a 01`).
  Rebuild: `tic -x -o resources/terminfo tools/xterm-256color.src` in the image, and check the magic.
- **The console**: bash is the firmware's `/bin/bash`; the stick is FAT, so Home and the settings are there.
  The App runs from the launcher's Apps set (it gives the display up for it).
- **On hardware** (the owner, 2026-09-24): works as expected on a **Raspberry Pi**. Not yet run on a
  **console**. The **PC stick** is untested because the PC stick itself is not debugged yet.
- **Windows** is a dev host only (ConPTY). Seen there: right after a resize, the first character typed may be
  lost - MSYS2's bash under ConPTY; the same sequence on Linux (a real pty) loses nothing.
- **Testing the UI**: `make_win.sh`, then the launcher's `tools/ab_drive.py` against the DebugDriver
  (`AB_DEBUG_PORT`; `--port` goes after the command). On the build server: the native build in the image
  under `xvfb-run -a` (the dummy video driver has no accelerated renderer, which core requires). `key` takes
  `ctrl+c`, `text` types; `;` splits a driver script, so put multi-command shell lines in a file under Home.
- Every string on screen is `_()` and in all 16 languages (`tools/make_lang.py`); strings the main GUI has
  (Delete, Space, Enter, Keyboard) are left to it.

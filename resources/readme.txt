================================================================================
                     Terminal - a command line for AutoBleem
================================================================================

A terminal: bash (or the machine's own shell) with colours, drawn in your
AutoBleem theme. Type with a USB keyboard, or with the pad on the on-screen
keyboard. Leave with `exit`, or Select -> Close the terminal.

The pad, with the on-screen keyboard showing
  D-pad            move over the keys (held: keeps moving)
  Cross            press the key
  Circle           Backspace
  Square           Space
  Start            Enter
  L1 / R1          Shift / Ctrl for the next key (twice: locked)
  L2 / R2          scroll back through what went off the screen / forward
  Triangle         hide the keyboard (more rows for the text)
  Select           the menu: text size, full screen, Ctrl+C / Ctrl+D / Ctrl+Z,
                   close

The pad, with the keyboard hidden - for full-screen programs (less, top, mc)
  D-pad            the arrow keys
  Cross / Start    Enter
  Circle           Esc
  Square           Tab
  L1 / R1          Page Up / Page Down
  Triangle         show the keyboard

A USB keyboard works as on any terminal, Ctrl and Alt included;
Shift+Page Up / Page Down scrolls back.

Your home folder is Home/ on the stick. ~/.bashrc there is read at every
start, after the App's own settings; the text size and the other choices
from the menu are kept in Home/.config/autobleem-terminal/terminal.ini.

The font is DejaVu Sans Mono (fonts/DejaVu-LICENSE.txt). Terminal is part
of AutoBleem, GPL-3.0-or-later.

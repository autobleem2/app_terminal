//
// KeyEncoder: a key as the bytes an xterm sends for it - what a program reading the terminal gets when the
// user presses it. The arrows and Home/End follow the cursor-key mode (DECCKM, which vi and less switch
// on); a modifier is sent the way xterm does (ESC [ 1 ; <1 + shift + 2*alt + 4*ctrl> A). Ctrl with a
// character is the control code; Alt puts ESC in front.
//
#pragma once

#include <string>

namespace term {

// the keys that are not characters, as the terminal knows them
enum class TermKey {
    Up,
    Down,
    Right,
    Left,
    Home,
    End,
    Insert,
    Delete,
    PageUp,
    PageDown,
    Return,
    Tab,
    Backspace,
    Escape,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12
};

struct TermMods {
    bool shift = false, alt = false, ctrl = false;
};

class KeyEncoder {
public:
    static std::string key(TermKey key, TermMods mods, bool appCursorKeys);
    // a character key (its label, unshifted: 'c', '[', '2') with Ctrl and/or Alt held; "" when the
    // combination sends nothing. Shift gives the upper-case letter under Alt
    static std::string character(int code, TermMods mods);
    // the control code Ctrl turns `code` into (Ctrl+C = 3, Ctrl+[ = ESC); -1 when there is none
    static int controlCode(int code);
    // text on its way to the program: with bracketed paste on (CSI ? 2004 h) a whole string is marked as
    // pasted, so a shell does not run it line by line
    static std::string paste(const std::string &text, bool bracketed);
};

} // namespace term

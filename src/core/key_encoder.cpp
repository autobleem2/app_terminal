//
// KeyEncoder: keys as xterm sends them.
//
#include "key_encoder.h"

#include <cctype>

using namespace std;

namespace term {

namespace {

int modifierParam(TermMods m) {
    return 1 + (m.shift ? 1 : 0) + (m.alt ? 2 : 0) + (m.ctrl ? 4 : 0);
}

bool any(TermMods m) {
    return m.shift || m.alt || m.ctrl;
}

// ESC [ final, ESC O final (SS3) in the application mode, ESC [ 1 ; m final with a modifier
string cursorKey(char final, TermMods mods, bool app) {
    if (any(mods))
        return "\x1b[1;" + to_string(modifierParam(mods)) + final;
    return string(app ? "\x1bO" : "\x1b[") + final;
}

// ESC [ n ~, ESC [ n ; m ~ with a modifier
string tildeKey(int n, TermMods mods) {
    if (any(mods))
        return "\x1b[" + to_string(n) + ";" + to_string(modifierParam(mods)) + "~";
    return "\x1b[" + to_string(n) + "~";
}

// F1..F4 are SS3 P..S; with a modifier ESC [ 1 ; m P
string pfKey(char final, TermMods mods) {
    if (any(mods))
        return "\x1b[1;" + to_string(modifierParam(mods)) + final;
    return string("\x1bO") + final;
}

} // namespace

//*******************************
// KeyEncoder::key
//*******************************
string KeyEncoder::key(TermKey key, TermMods mods, bool appCursorKeys) {
    const string alt = mods.alt ? "\x1b" : "";
    switch (key) {
    case TermKey::Up:
        return cursorKey('A', mods, appCursorKeys);
    case TermKey::Down:
        return cursorKey('B', mods, appCursorKeys);
    case TermKey::Right:
        return cursorKey('C', mods, appCursorKeys);
    case TermKey::Left:
        return cursorKey('D', mods, appCursorKeys);
    case TermKey::Home:
        return cursorKey('H', mods, appCursorKeys);
    case TermKey::End:
        return cursorKey('F', mods, appCursorKeys);
    case TermKey::Insert:
        return tildeKey(2, mods);
    case TermKey::Delete:
        return tildeKey(3, mods);
    case TermKey::PageUp:
        return tildeKey(5, mods);
    case TermKey::PageDown:
        return tildeKey(6, mods);
    case TermKey::Return:
        return alt + "\r";
    case TermKey::Tab:
        return mods.shift ? "\x1b[Z" : alt + "\t";
    case TermKey::Backspace:
        return alt + (mods.ctrl ? "\x08" : "\x7f");
    case TermKey::Escape:
        return alt + "\x1b";
    case TermKey::F1:
        return pfKey('P', mods);
    case TermKey::F2:
        return pfKey('Q', mods);
    case TermKey::F3:
        return pfKey('R', mods);
    case TermKey::F4:
        return pfKey('S', mods);
    case TermKey::F5:
        return tildeKey(15, mods);
    case TermKey::F6:
        return tildeKey(17, mods);
    case TermKey::F7:
        return tildeKey(18, mods);
    case TermKey::F8:
        return tildeKey(19, mods);
    case TermKey::F9:
        return tildeKey(20, mods);
    case TermKey::F10:
        return tildeKey(21, mods);
    case TermKey::F11:
        return tildeKey(23, mods);
    case TermKey::F12:
        return tildeKey(24, mods);
    }
    return "";
}

//*******************************
// KeyEncoder::controlCode
//*******************************
int KeyEncoder::controlCode(int code) {
    if (code >= 'a' && code <= 'z')
        return code - 'a' + 1;
    if (code >= 'A' && code <= 'Z')
        return code - 'A' + 1;
    switch (code) {
    case '@':
    case ' ':
    case '2':
        return 0;
    case '[':
    case '3':
        return 0x1b;
    case '\\':
    case '4':
        return 0x1c;
    case ']':
    case '5':
        return 0x1d;
    case '^':
    case '6':
        return 0x1e;
    case '_':
    case '-':
    case '/':
    case '7':
        return 0x1f;
    case '?':
    case '8':
        return 0x7f;
    default:
        return -1;
    }
}

//*******************************
// KeyEncoder::character
//*******************************
string KeyEncoder::character(int code, TermMods mods) {
    if (code <= 0 || code > 0x7e)
        return "";
    string out;
    if (mods.ctrl) {
        const int c = controlCode(code);
        if (c < 0)
            return "";
        out = string(1, static_cast<char>(c));
    } else if (mods.alt) {
        char c = static_cast<char>(code);
        if (mods.shift && c >= 'a' && c <= 'z')
            c = static_cast<char>(toupper(c));
        out = string(1, c);
    } else {
        return ""; // a plain character comes as text
    }
    return mods.alt ? "\x1b" + out : out;
}

//*******************************
// KeyEncoder::paste
//*******************************
string KeyEncoder::paste(const string &text, bool bracketed) {
    if (!bracketed)
        return text;
    return "\x1b[200~" + text + "\x1b[201~";
}

} // namespace term

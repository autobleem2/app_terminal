//
// KeyEncoder: keys as xterm sends them.
//
#include "core/key_encoder.h"

#include <doctest/doctest.h>

using namespace std;
using namespace term;

namespace {
TermMods mods(bool shift, bool alt, bool ctrl) {
    TermMods m;
    m.shift = shift;
    m.alt = alt;
    m.ctrl = ctrl;
    return m;
}
} // namespace

TEST_CASE("the arrows follow the cursor-key mode") {
    CHECK(KeyEncoder::key(TermKey::Up, TermMods(), false) == "\x1b[A");
    CHECK(KeyEncoder::key(TermKey::Up, TermMods(), true) == "\x1bOA");
    CHECK(KeyEncoder::key(TermKey::Left, TermMods(), false) == "\x1b[D");
    CHECK(KeyEncoder::key(TermKey::Home, TermMods(), false) == "\x1b[H");
    CHECK(KeyEncoder::key(TermKey::End, TermMods(), true) == "\x1bOF");
}

TEST_CASE("a modifier is sent as xterm's parameter, whatever the mode") {
    CHECK(KeyEncoder::key(TermKey::Right, mods(false, false, true), true) == "\x1b[1;5C");
    CHECK(KeyEncoder::key(TermKey::Up, mods(true, false, false), false) == "\x1b[1;2A");
    CHECK(KeyEncoder::key(TermKey::Delete, mods(false, true, false), false) == "\x1b[3;3~");
    CHECK(KeyEncoder::key(TermKey::F1, mods(true, false, true), false) == "\x1b[1;6P");
}

TEST_CASE("the editing and function keys") {
    CHECK(KeyEncoder::key(TermKey::Insert, TermMods(), false) == "\x1b[2~");
    CHECK(KeyEncoder::key(TermKey::Delete, TermMods(), false) == "\x1b[3~");
    CHECK(KeyEncoder::key(TermKey::PageUp, TermMods(), false) == "\x1b[5~");
    CHECK(KeyEncoder::key(TermKey::PageDown, TermMods(), false) == "\x1b[6~");
    CHECK(KeyEncoder::key(TermKey::F1, TermMods(), false) == "\x1bOP");
    CHECK(KeyEncoder::key(TermKey::F4, TermMods(), false) == "\x1bOS");
    CHECK(KeyEncoder::key(TermKey::F5, TermMods(), false) == "\x1b[15~");
    CHECK(KeyEncoder::key(TermKey::F10, TermMods(), false) == "\x1b[21~");
    CHECK(KeyEncoder::key(TermKey::F12, TermMods(), false) == "\x1b[24~");
}

TEST_CASE("Return, Tab, Backspace and Esc") {
    CHECK(KeyEncoder::key(TermKey::Return, TermMods(), false) == "\r");
    CHECK(KeyEncoder::key(TermKey::Return, mods(false, true, false), false) == "\x1b\r");
    CHECK(KeyEncoder::key(TermKey::Tab, TermMods(), false) == "\t");
    CHECK(KeyEncoder::key(TermKey::Tab, mods(true, false, false), false) == "\x1b[Z");
    CHECK(KeyEncoder::key(TermKey::Backspace, TermMods(), false) == "\x7f");
    CHECK(KeyEncoder::key(TermKey::Backspace, mods(false, false, true), false) == "\x08");
    CHECK(KeyEncoder::key(TermKey::Escape, TermMods(), false) == "\x1b");
}

TEST_CASE("Ctrl and Alt with a character") {
    CHECK(KeyEncoder::character('c', mods(false, false, true)) == "\x03");
    CHECK(KeyEncoder::character('C', mods(false, false, true)) == "\x03");
    CHECK(KeyEncoder::character('[', mods(false, false, true)) == "\x1b");
    CHECK(KeyEncoder::character(' ', mods(false, false, true)) == string(1, '\0'));
    CHECK(KeyEncoder::character('/', mods(false, false, true)) == "\x1f");
    CHECK(KeyEncoder::character('x', mods(false, true, false)) == "\x1bx");
    CHECK(KeyEncoder::character('x', mods(true, true, false)) == "\x1bX");
    CHECK(KeyEncoder::character('d', mods(false, true, true)) == "\x1b\x04");
    CHECK(KeyEncoder::character('x', TermMods()) == ""); // a plain character is text, not a key
    CHECK(KeyEncoder::character('=', mods(false, false, true)) == "");
}

TEST_CASE("bracketed paste") {
    CHECK(KeyEncoder::paste("ls\n", false) == "ls\n");
    CHECK(KeyEncoder::paste("ls\n", true) == "\x1b[200~ls\n\x1b[201~");
}

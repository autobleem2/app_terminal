//
// PadKeyboard: the on-screen keyboard a pad types on - a US layout of five rows with the keys a shell
// needs (Esc, Tab, Ctrl, Alt, the arrows, Page Up/Down), laid under the terminal. The d-pad moves over it,
// Cross presses a key. Shift, Ctrl and Alt latch for the next key (pressed twice they lock) - L1 and R1
// latch Shift and Ctrl without moving there. What a key sends is the terminal's business: press() says
// which key it was and with which modifiers.
//
#pragma once

#include "core/key_encoder.h"

#include <ableem/ui/font.h>
#include <ableem/ui/renderer.h>

#include <string>
#include <vector>

class Gui;

class PadKeyboard {
public:
    PadKeyboard();

    // what pressing the selected key comes to: text to type, a terminal key, or nothing (a modifier)
    struct Press {
        enum Kind { None, Text, Key, Character } kind = None;
        std::string text;                          // Text: the character, shifted as shown
        term::TermKey key = term::TermKey::Escape; // Key
        int code = 0;                              // Character: the key's label with Ctrl or Alt latched
        term::TermMods mods;
    };
    Press press();
    // after a key was typed: a latched (not locked) modifier lets go
    void releaseLatches();

    void move(int dx, int dy);
    enum class Modifier { Shift, Ctrl, Alt };
    void toggle(Modifier m);
    term::TermMods mods() const;

    // the height it takes at `width`
    int heightFor(int width) const;
    // the key caps in `font` (the terminal's own face: it has the arrows), in the theme's panel colours
    void render(Gui &gui, ableem::Renderer &renderer, const ableem::Rect &area, const ableem::Font &font) const;

private:
    struct KeyDef {
        std::string label, shifted; // what it types (and shows); a special key's label is its name
        float width = 1.0f;         // in key units
        enum Kind { Char, Special, Mod } kind = Char;
        term::TermKey key = term::TermKey::Escape;
        Modifier modifier = Modifier::Shift;
    };
    std::vector<std::vector<KeyDef>> rows_;
    int row_ = 1, col_ = 1;
    // 0: off, 1: for the next key, 2: locked
    int shift_ = 0, ctrl_ = 0, alt_ = 0;
    int &state(Modifier m);
    int stateOf(Modifier m) const;
    // the column in `row` under the middle of the selected key, so Up/Down keep to a straight line
    int columnBelow(int fromRow, int fromCol, int toRow) const;
    float rowUnits(int row) const;
};

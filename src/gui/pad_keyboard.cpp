//
// PadKeyboard: the on-screen keyboard.
//
#include "pad_keyboard.h"

#include "gui/gui.h"
#include "gui/panel_style.h"

#include <algorithm>
#include <cctype>
#include <cmath>

using namespace std;
using ableem::Color;
using ableem::Rect;
using term::TermKey;

namespace {
const float RowUnits = 15.0f; // every row is 15 key widths wide
const int Gap = 4;
} // namespace

//*******************************
// PadKeyboard::PadKeyboard
//*******************************
PadKeyboard::PadKeyboard() {
    auto chars = [](const string &plain, const string &shifted) {
        vector<KeyDef> keys;
        for (size_t i = 0; i < plain.size(); i++) {
            KeyDef k;
            k.label = string(1, plain[i]);
            k.shifted = string(1, shifted[i]);
            keys.push_back(k);
        }
        return keys;
    };
    auto special = [](const string &label, TermKey key, float width) {
        KeyDef k;
        k.label = label;
        k.kind = KeyDef::Special;
        k.key = key;
        k.width = width;
        return k;
    };
    auto modifier = [](const string &label, Modifier m, float width) {
        KeyDef k;
        k.label = label;
        k.kind = KeyDef::Mod;
        k.modifier = m;
        k.width = width;
        return k;
    };

    vector<KeyDef> r0 = chars("`1234567890-=", "~!@#$%^&*()_+");
    r0.push_back(special("Bksp", TermKey::Backspace, 2.0f));

    vector<KeyDef> r1 = {special("Tab", TermKey::Tab, 1.5f)};
    for (const KeyDef &k : chars("qwertyuiop[]", "QWERTYUIOP{}"))
        r1.push_back(k);
    KeyDef backslash = chars("\\", "|")[0];
    backslash.width = 1.5f;
    r1.push_back(backslash);

    vector<KeyDef> r2 = {modifier("Ctrl", Modifier::Ctrl, 1.75f)};
    for (const KeyDef &k : chars("asdfghjkl;'", "ASDFGHJKL:\""))
        r2.push_back(k);
    r2.push_back(special("Enter", TermKey::Return, 2.25f));

    vector<KeyDef> r3 = {modifier("Shift", Modifier::Shift, 2.25f)};
    for (const KeyDef &k : chars("zxcvbnm,./", "ZXCVBNM<>?"))
        r3.push_back(k);
    r3.push_back(special("\xe2\x86\x91", TermKey::Up, 1.0f)); // up arrow
    r3.push_back(special("Del", TermKey::Delete, 1.75f));

    KeyDef space = chars(" ", " ")[0];
    space.width = 6.5f;
    vector<KeyDef> r4 = {special("Esc", TermKey::Escape, 1.5f),
                         modifier("Alt", Modifier::Alt, 1.5f),
                         space,
                         special("\xe2\x86\x90", TermKey::Left, 1.0f),
                         special("\xe2\x86\x93", TermKey::Down, 1.0f),
                         special("\xe2\x86\x92", TermKey::Right, 1.0f),
                         special("PgUp", TermKey::PageUp, 1.25f),
                         special("PgDn", TermKey::PageDown, 1.25f)};
    rows_ = {r0, r1, r2, r3, r4};
}

int &PadKeyboard::state(Modifier m) {
    return m == Modifier::Shift ? shift_ : (m == Modifier::Ctrl ? ctrl_ : alt_);
}

int PadKeyboard::stateOf(Modifier m) const {
    return m == Modifier::Shift ? shift_ : (m == Modifier::Ctrl ? ctrl_ : alt_);
}

//*******************************
// PadKeyboard::toggle
//*******************************
// off -> for the next key -> locked -> off
void PadKeyboard::toggle(Modifier m) {
    int &s = state(m);
    s = (s + 1) % 3;
}

term::TermMods PadKeyboard::mods() const {
    term::TermMods m;
    m.shift = shift_ != 0;
    m.ctrl = ctrl_ != 0;
    m.alt = alt_ != 0;
    return m;
}

void PadKeyboard::releaseLatches() {
    for (int *s : {&shift_, &ctrl_, &alt_})
        if (*s == 1)
            *s = 0;
}

//*******************************
// PadKeyboard::press
//*******************************
PadKeyboard::Press PadKeyboard::press() {
    const KeyDef &k = rows_[row_][col_];
    Press p;
    p.mods = mods();
    if (k.kind == KeyDef::Mod) {
        toggle(k.modifier);
        return p;
    }
    if (k.kind == KeyDef::Special) {
        p.kind = Press::Key;
        p.key = k.key;
        return p;
    }
    if (p.mods.ctrl || p.mods.alt) {
        p.kind = Press::Character;
        p.code = static_cast<unsigned char>(k.label[0]);
        return p;
    }
    p.kind = Press::Text;
    p.text = p.mods.shift ? k.shifted : k.label;
    return p;
}

//*******************************
// PadKeyboard::rowUnits / columnBelow
//*******************************
float PadKeyboard::rowUnits(int row) const {
    float u = 0;
    for (const KeyDef &k : rows_[row])
        u += k.width;
    return u;
}

int PadKeyboard::columnBelow(int fromRow, int fromCol, int toRow) const {
    float left = 0;
    for (int c = 0; c < fromCol; c++)
        left += rows_[fromRow][c].width;
    const float middle = left + rows_[fromRow][fromCol].width / 2;
    float x = 0;
    for (int c = 0; c < static_cast<int>(rows_[toRow].size()); c++) {
        x += rows_[toRow][c].width;
        if (middle < x)
            return c;
    }
    return static_cast<int>(rows_[toRow].size()) - 1;
}

//*******************************
// PadKeyboard::move
//*******************************
void PadKeyboard::move(int dx, int dy) {
    const int rows = static_cast<int>(rows_.size());
    if (dy != 0) {
        const int to = (row_ + dy + rows) % rows;
        col_ = columnBelow(row_, col_, to);
        row_ = to;
    }
    if (dx != 0) {
        const int cols = static_cast<int>(rows_[row_].size());
        col_ = (col_ + dx + cols) % cols;
    }
}

//*******************************
// PadKeyboard::heightFor
//*******************************
int PadKeyboard::heightFor(int width) const {
    const float unit = width / RowUnits;
    const int keyH = max(28, min(36, static_cast<int>(unit * 0.6f)));
    return static_cast<int>(rows_.size()) * (keyH + Gap) + Gap;
}

//*******************************
// PadKeyboard::render
//*******************************
void PadKeyboard::render(Gui &gui, ableem::Renderer &renderer, const Rect &area, const ableem::Font &font) const {
    const PanelStyle style = gui.panelStyle();
    const float unit = area.w / RowUnits;
    const int keyH = (area.h - Gap) / static_cast<int>(rows_.size()) - Gap;
    const bool shifted = shift_ != 0;

    // a dark plate under the caps, so the theme's background does not show through them
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(Color(8, 10, 14, 235));
    renderer.fillRect(Rect(area.x - Gap, area.y, area.w + Gap, area.h));
    for (int r = 0; r < static_cast<int>(rows_.size()); r++) {
        const int y = area.y + Gap + r * (keyH + Gap);
        float units = (RowUnits - rowUnits(r)) / 2; // a shorter row is centred
        for (int c = 0; c < static_cast<int>(rows_[r].size()); c++) {
            const KeyDef &k = rows_[r][c];
            const int x0 = area.x + static_cast<int>(lround(units * unit));
            const int x1 = area.x + static_cast<int>(lround((units + k.width) * unit)) - Gap;
            units += k.width;
            const Rect key(x0, y, x1 - x0, keyH);
            const bool selected = r == row_ && c == col_;
            const int modState = k.kind == KeyDef::Mod ? stateOf(k.modifier) : 0;

            // the cap: faint, a latched modifier in the secondary colour (stronger when locked), the
            // selected key in the text colour with an edge
            Color fill(255, 255, 255, 22);
            if (modState)
                fill = Color(style.secondary.r, style.secondary.g, style.secondary.b, modState == 2 ? 200 : 120);
            if (selected)
                fill = Color(style.text.r, style.text.g, style.text.b, 90);
            renderer.setDrawColor(fill);
            renderer.fillRect(key);
            renderer.setDrawColor(selected ? style.text : Color(255, 255, 255, 40));
            renderer.drawRect(key);

            string label = k.label;
            if (k.kind == KeyDef::Char) {
                if (label == " ")
                    label = "Space";
                else if (shifted)
                    label = k.shifted;
            }
            const int w = font.width(label);
            const int h = font.lineHeight();
            font.drawColor(renderer, key.x + (key.w - w) / 2, key.y + (key.h - h) / 2, style.text, label);
        }
    }
}

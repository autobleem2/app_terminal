//
// TerminalView: the grid on screen.
//
#include "terminal_view.h"

#include <algorithm>
#include <cmath>

using namespace std;
using ableem::Color;
using ableem::Rect;

namespace {

const Color DefaultForeground(222, 222, 222, 255);
const Color CursorColor(230, 230, 230, 255);

void appendUtf8(string &out, uint32_t ch) {
    if (ch < 0x80) {
        out += static_cast<char>(ch);
    } else if (ch < 0x800) {
        out += static_cast<char>(0xc0 | (ch >> 6));
        out += static_cast<char>(0x80 | (ch & 0x3f));
    } else if (ch < 0x10000) {
        out += static_cast<char>(0xe0 | (ch >> 12));
        out += static_cast<char>(0x80 | ((ch >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (ch & 0x3f));
    } else {
        out += static_cast<char>(0xf0 | (ch >> 18));
        out += static_cast<char>(0x80 | ((ch >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((ch >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (ch & 0x3f));
    }
}

Color resolve(const term::Color &c, const Color &fallback, bool brighten) {
    if (c.kind == term::Color::Default)
        return fallback;
    if (c.kind == term::Color::Rgb)
        return Color(c.r, c.g, c.b, 255);
    uint8_t r, g, b;
    // bold text in one of the eight base colours is drawn in its bright version, as xterm does
    term::paletteColor(brighten && c.index < 8 ? c.index + 8 : c.index, r, g, b);
    return Color(r, g, b, 255);
}

bool sameColor(const Color &a, const Color &b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

} // namespace

//*******************************
// TerminalView::loadFonts
//*******************************
bool TerminalView::loadFonts(ableem::Renderer &renderer, const string &regularTtf, const string &boldTtf,
                             int pointSize) {
    ableem::Font regular = ableem::Font::load(renderer, regularTtf, pointSize);
    if (!regular.valid())
        return false;
    ableem::Font bold = ableem::Font::load(renderer, boldTtf, pointSize);
    regular_ = regular;
    bold_ = bold.valid() ? bold : regular;
    // the advance of the face, measured the way the font draws a run, so a run lands on the grid
    cellW_ = max(1.0f, regular_.width(string(100, 'M')) / 100.0f);
    cellH_ = max(1, regular_.lineHeight());
    invalidate();
    return true;
}

int TerminalView::colsFor(int w) const {
    return max(1, static_cast<int>(w / cellW_));
}

int TerminalView::rowsFor(int h) const {
    return max(1, h / cellH_);
}

int TerminalView::xOf(int col) const {
    return static_cast<int>(lround(col * cellW_));
}

//*******************************
// TerminalView::colorsOf
//*******************************
TerminalView::Colors TerminalView::colorsOf(const term::Cell &cell, bool screenReverse) const {
    Colors c;
    c.fg = resolve(cell.fg, DefaultForeground, (cell.attr & term::Attr::Bold) != 0);
    c.bg = resolve(cell.bg, defaultBackground(), false);
    if (((cell.attr & term::Attr::Reverse) != 0) != screenReverse) {
        swap(c.fg, c.bg);
        // the default background swapped in as a foreground would be too dark to read on the light block
        if (cell.bg.kind == term::Color::Default)
            c.fg = Color(0, 0, 0, 255);
        if (cell.fg.kind == term::Color::Default)
            c.bg = DefaultForeground;
    }
    if (cell.attr & term::Attr::Dim) {
        c.fg.r = static_cast<unsigned char>((c.fg.r * 2 + c.bg.r) / 3);
        c.fg.g = static_cast<unsigned char>((c.fg.g * 2 + c.bg.g) / 3);
        c.fg.b = static_cast<unsigned char>((c.fg.b * 2 + c.bg.b) / 3);
    }
    if (cell.attr & term::Attr::Invisible)
        c.fg = c.bg;
    return c;
}

//*******************************
// TerminalView::drawRow
//*******************************
void TerminalView::drawRow(ableem::Renderer &renderer, const term::Line &line, int row, bool screenReverse) {
    const int y = row * cellH_;
    const int cols = static_cast<int>(line.size());
    const int width = xOf(cols);

    // the backgrounds: the row in the default colour, then each run of another colour over it
    renderer.setBlendMode(ableem::BlendMode::None);
    const Color base = screenReverse ? DefaultForeground : defaultBackground();
    renderer.setDrawColor(base);
    renderer.fillRect(Rect(0, y, width, cellH_));
    for (int c = 0; c < cols;) {
        const Color bg = colorsOf(line[c], screenReverse).bg;
        int end = c + 1;
        while (end < cols && sameColor(colorsOf(line[end], screenReverse).bg, bg))
            end++;
        if (!sameColor(bg, base)) {
            renderer.setDrawColor(bg);
            renderer.fillRect(Rect(xOf(c), y, xOf(end) - xOf(c), cellH_));
        }
        c = end;
    }
    renderer.setBlendMode(ableem::BlendMode::Blend);

    // the text: runs of ASCII in one colour and weight; anything else one cell at a time, at its own place
    // on the grid, since a glyph from another block of the font need not have the face's advance
    for (int c = 0; c < cols;) {
        const term::Cell &cell = line[c];
        const Colors colors = colorsOf(cell, screenReverse);
        const bool bold = (cell.attr & term::Attr::Bold) != 0;
        const ableem::Font &font = bold ? bold_ : regular_;
        if (cell.ch <= ' ' || (cell.attr & term::Attr::Invisible)) {
            c++;
        } else if (cell.ch >= 0x80) {
            string glyph;
            appendUtf8(glyph, cell.ch);
            font.drawColor(renderer, xOf(c), y, colors.fg, glyph);
            c++;
        } else {
            string run;
            int end = c;
            while (end < cols) {
                const term::Cell &n = line[end];
                if (n.ch >= 0x80 || (n.attr & term::Attr::Invisible) || ((n.attr & term::Attr::Bold) != 0) != bold ||
                    !sameColor(colorsOf(n, screenReverse).fg, colors.fg))
                    break;
                run += static_cast<char>(n.ch < ' ' ? ' ' : n.ch);
                end++;
            }
            run.erase(run.find_last_not_of(' ') + 1);
            if (!run.empty())
                font.drawColor(renderer, xOf(c), y, colors.fg, run);
            c = end;
        }
    }

    // underline and strike-through, a line each in the text's colour
    for (int c = 0; c < cols; c++) {
        const term::Cell &cell = line[c];
        if (!(cell.attr & (term::Attr::Underline | term::Attr::Strike)))
            continue;
        renderer.setDrawColor(colorsOf(cell, screenReverse).fg);
        const int x = xOf(c), w = xOf(c + 1) - x;
        if (cell.attr & term::Attr::Underline)
            renderer.fillRect(Rect(x, y + cellH_ - 2, w, 1));
        if (cell.attr & term::Attr::Strike)
            renderer.fillRect(Rect(x, y + cellH_ / 2, w, 1));
    }
}

//*******************************
// TerminalView::render
//*******************************
void TerminalView::render(ableem::Renderer &renderer, term::VtScreen &vt, int x, int y, int scrollOffset,
                          bool showCursor) {
    const int cols = vt.cols(), rows = vt.rows();
    bool all = false;
    if (!texture_.valid() || texCols_ != cols || texRows_ != rows) {
        texture_ = ableem::Texture::createTarget(renderer, max(1, xOf(cols)), rows * cellH_);
        texCols_ = cols;
        texRows_ = rows;
        all = true;
    }
    // looking into the scrollback, every new line moves the whole view: all of it again
    if (scrollOffset != lastOffset_ || vt.reverseVideo() != lastReverse_ ||
        (scrollOffset > 0 && vt.version() != lastVersion_))
        all = true;
    if (all || vt.version() != lastVersion_) {
        renderer.setTarget(&texture_);
        for (int r = 0; r < rows; r++) {
            if (all || vt.rowDirty(r))
                drawRow(renderer, vt.viewLine(r, scrollOffset), r, vt.reverseVideo());
        }
        renderer.setTarget(nullptr);
        vt.clearDirty();
        lastVersion_ = vt.version();
        lastOffset_ = scrollOffset;
        lastReverse_ = vt.reverseVideo();
    }
    renderer.setBlendMode(ableem::BlendMode::Blend);
    const Rect dst(x, y, xOf(cols), rows * cellH_);
    renderer.copy(texture_, nullptr, &dst);

    // the cursor: a block in the text colour with its character drawn over it in the background's
    if (showCursor && scrollOffset == 0 && vt.cursorVisible()) {
        const int row = vt.cursorRow(), col = vt.cursorCol();
        const Rect block(x + xOf(col), y + row * cellH_, xOf(col + 1) - xOf(col), cellH_);
        renderer.setDrawColor(CursorColor);
        renderer.fillRect(block);
        const term::Cell &cell = vt.cell(row, col);
        if (cell.ch > ' ' && !(cell.attr & term::Attr::Invisible)) {
            string glyph;
            appendUtf8(glyph, cell.ch);
            const ableem::Font &font = (cell.attr & term::Attr::Bold) ? bold_ : regular_;
            font.drawColor(renderer, block.x, block.y, defaultBackground(), glyph);
        }
    }
}

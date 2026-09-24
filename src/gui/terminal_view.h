//
// TerminalView: a VtScreen drawn - the grid in a monospace font, its colours and attributes, the cursor.
// The cells are drawn into a texture the size of the grid, and only the rows that changed are drawn again,
// so a frame where nothing happened is one copy; a line of text of one colour is one draw call.
//
#pragma once

#include "core/vt_screen.h"

#include <ableem/ui/font.h>
#include <ableem/ui/renderer.h>
#include <ableem/ui/texture.h>

#include <string>

class TerminalView {
public:
    // the fonts (regular and bold, the same face) at `pointSize`; false when the regular one will not load
    bool loadFonts(ableem::Renderer &renderer, const std::string &regularTtf, const std::string &boldTtf,
                   int pointSize);
    // how many cells fit in an area of w x h logical pixels
    int colsFor(int w) const;
    int rowsFor(int h) const;
    float cellWidth() const { return cellW_; }
    int cellHeight() const { return cellH_; }

    // draws the view `scrollOffset` lines up into the scrollback at (x, y); the cursor when `showCursor`
    void render(ableem::Renderer &renderer, term::VtScreen &vt, int x, int y, int scrollOffset, bool showCursor);
    // the whole grid is drawn again on the next render (a new font, a device reset)
    void invalidate() { texture_ = ableem::Texture(); }
    // the colour every cell without one of its own is drawn on
    static ableem::Color defaultBackground() { return ableem::Color(10, 12, 16, 255); }

private:
    ableem::Font regular_, bold_;
    float cellW_ = 10.0f;
    int cellH_ = 20;
    ableem::Texture texture_;
    int texCols_ = 0, texRows_ = 0;
    int lastOffset_ = -1;
    unsigned long lastVersion_ = 0;
    bool lastReverse_ = false;

    struct Colors {
        ableem::Color fg, bg;
    };
    Colors colorsOf(const term::Cell &cell, bool screenReverse) const;
    void drawRow(ableem::Renderer &renderer, const term::Line &line, int row, bool screenReverse);
    int xOf(int col) const;
};

//
// VtScreen: the terminal itself, without a picture - what a program's output does to a grid of character
// cells. It understands the part of xterm that the xterm-256color terminfo entry promises and that bash,
// readline, vi, less, top, htop and mc use: the C0 controls, the ESC and CSI sequences for moving, erasing,
// inserting, deleting and scrolling, the scrolling region, the alternate screen, SGR attributes in 16, 256 and
// 24-bit colour, the DEC line-drawing character set, the cursor and keypad modes, and the answers to the
// status queries (DSR, DA). OSC is read and dropped, apart from the window title. No SDL here: the screen
// that draws it is gui/terminal_view.*, and the tests feed it bytes and look at the cells.
//
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace term {

//******************
// Color
//******************
// A cell's colour: the terminal's default, one of the 256 palette entries, or a 24-bit value
struct Color {
    enum Kind : uint8_t { Default, Indexed, Rgb };
    Kind kind = Default;
    uint8_t index = 0;
    uint8_t r = 0, g = 0, b = 0;

    static Color indexed(int i) {
        Color c;
        c.kind = Indexed;
        c.index = static_cast<uint8_t>(i);
        return c;
    }
    static Color rgb(int r, int g, int b) {
        Color c;
        c.kind = Rgb;
        c.r = static_cast<uint8_t>(r);
        c.g = static_cast<uint8_t>(g);
        c.b = static_cast<uint8_t>(b);
        return c;
    }
    bool operator==(const Color &o) const {
        return kind == o.kind &&
               (kind == Default || (kind == Indexed ? index == o.index : (r == o.r && g == o.g && b == o.b)));
    }
    bool operator!=(const Color &o) const { return !(*this == o); }
};

//******************
// Attr
//******************
struct Attr {
    enum : uint16_t {
        Bold = 1,
        Dim = 2,
        Italic = 4,
        Underline = 8,
        Blink = 16,
        Reverse = 32,
        Invisible = 64,
        Strike = 128
    };
};

//******************
// Cell
//******************
struct Cell {
    uint32_t ch = ' '; // a Unicode code point
    Color fg, bg;
    uint16_t attr = 0;
    bool operator==(const Cell &o) const { return ch == o.ch && fg == o.fg && bg == o.bg && attr == o.attr; }
    bool operator!=(const Cell &o) const { return !(*this == o); }
};

typedef std::vector<Cell> Line;

//******************
// VtScreen
//******************
class VtScreen {
public:
    VtScreen(int cols, int rows, int scrollbackLines = 1000);

    // the program's output, any number of bytes at a time: a sequence split between two calls is fine
    void write(const char *data, size_t size);
    void write(const std::string &s) { write(s.data(), s.size()); }

    // what the terminal answers a query with (DSR, DA): to be written to the program's input
    std::function<void(const std::string &)> onReply;
    // BEL
    std::function<void()> onBell;

    // a new size. Rows given up at the top go to the scrollback and come back from it when the screen
    // grows again (so an on-screen keyboard opening and closing leaves the text where it was); lines are
    // cut or padded on the right, not re-wrapped
    void resize(int cols, int rows);
    int cols() const { return cols_; }
    int rows() const { return rows_; }

    // the visible grid, row 0 at the top
    const Cell &cell(int row, int col) const { return screen()[row][col]; }
    const Line &line(int row) const { return screen()[row]; }
    // the lines scrolled off the top of the main screen, oldest first (none while the alternate screen shows)
    int scrollbackSize() const { return static_cast<int>(scrollback_.size()); }
    const Line &scrollbackLine(int i) const { return scrollback_[i]; }
    // the line `row` of the view `offset` lines up into the scrollback (0 = the live screen)
    const Line &viewLine(int row, int offset) const;

    int cursorRow() const { return cursorRow_; }
    int cursorCol() const { return cursorCol_; }
    bool cursorVisible() const { return cursorVisible_; }
    bool appCursorKeys() const { return appCursorKeys_; }
    bool appKeypad() const { return appKeypad_; }
    bool bracketedPaste() const { return bracketedPaste_; }
    bool altScreen() const { return altActive_; }
    bool reverseVideo() const { return reverseVideo_; }
    const std::string &title() const { return title_; }

    // grows by one with every change to what is shown - the view redraws when it moved
    unsigned long version() const { return version_; }
    // the rows changed since clearDirty(); a scroll marks every row
    bool rowDirty(int row) const { return dirty_[row]; }
    void clearDirty();

    // the whole visible grid as text, trailing blanks cut, rows joined by '\n' - for the tests and the log
    std::string text() const;

    void reset();

private:
    enum class State { Ground, Escape, EscapeIntermediate, Csi, Osc, OscEscape, IgnoreString, IgnoreStringEscape };

    struct Saved {
        int row = 0, col = 0;
        Cell pen;
        bool wrapPending = false;
        bool originMode = false;
        bool g0Graphics = false, g1Graphics = false;
        int gl = 0;
    };

    int cols_, rows_;
    size_t scrollbackMax_;
    std::vector<Line> main_, alt_;
    bool altActive_ = false;
    std::deque<Line> scrollback_;
    std::vector<bool> tabs_;
    std::vector<bool> dirty_;
    unsigned long version_ = 0;

    int cursorRow_ = 0, cursorCol_ = 0;
    bool wrapPending_ = false;
    Cell pen_;                 // the attributes new text is written with (its ch unused)
    int top_ = 0, bottom_ = 0; // the scrolling region, inclusive
    bool originMode_ = false, autoWrap_ = true, insertMode_ = false, newLineMode_ = false;
    bool cursorVisible_ = true, appCursorKeys_ = false, appKeypad_ = false, bracketedPaste_ = false;
    bool reverseVideo_ = false;
    bool g0Graphics_ = false, g1Graphics_ = false;
    int gl_ = 0; // 0: G0 is in use, 1: G1 (SO/SI)
    Saved saved_, savedAlt_;
    std::string title_;

    // the parser
    State state_ = State::Ground;
    uint32_t utf8_ = 0;
    int utf8Left_ = 0;
    std::vector<int> params_;
    std::vector<bool> paramIsSub_; // the parameter came after ':' (SGR 38:2::r:g:b)
    char private_ = 0;             // '?', '>', '<', '=' before the parameters
    std::string intermediates_;
    std::string osc_;
    uint32_t lastPrinted_ = ' ';

    std::vector<Line> &screen() { return altActive_ ? alt_ : main_; }
    const std::vector<Line> &screen() const { return altActive_ ? alt_ : main_; }
    Cell blank() const; // an erased cell: the pen's background, nothing else
    void touch(int row);
    void touchAll();

    void byte(unsigned char c);
    void control(unsigned char c);
    void print(uint32_t ch);
    void escape(unsigned char c);
    void csi(unsigned char final);
    void osc();
    void sgr();
    void setMode(bool on);
    void reply(const std::string &s);

    int param(size_t i, int fallback) const;
    void moveTo(int row, int col); // clamps; origin mode is the caller's
    void lineFeed();
    void reverseIndex();
    void scrollUp(int top, int bottom, int n);
    void scrollDown(int top, int bottom, int n);
    void eraseCells(int row, int from, int to); // [from, to)
    void saveCursor();
    void restoreCursor();
    void switchScreen(bool alt, bool clear);
    void resetTabs();
};

// the 256-colour palette entry `i` as r,g,b (the xterm defaults)
void paletteColor(int i, uint8_t &r, uint8_t &g, uint8_t &b);

} // namespace term

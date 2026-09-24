//
// VtScreen: the terminal's grid and the parser that drives it.
//
#include "vt_screen.h"

#include <algorithm>
#include <cstdio>

using namespace std;

namespace term {

namespace {

// the DEC special graphics set (ESC ( 0) for 0x5f..0x7e: what mc, dialog and ncurses draw boxes with
const uint32_t DecGraphics[] = {
    0x00a0, // _ blank
    0x25c6, // ` diamond
    0x2592, // a checkerboard
    0x2409, // b HT
    0x240c, // c FF
    0x240d, // d CR
    0x240a, // e LF
    0x00b0, // f degree
    0x00b1, // g plus/minus
    0x2424, // h NL
    0x240b, // i VT
    0x2518, // j lower right corner
    0x2510, // k upper right corner
    0x250c, // l upper left corner
    0x2514, // m lower left corner
    0x253c, // n crossing lines
    0x23ba, // o scan line 1
    0x23bb, // p scan line 3
    0x2500, // q horizontal line
    0x23bc, // r scan line 7
    0x23bd, // s scan line 9
    0x251c, // t left tee
    0x2524, // u right tee
    0x2534, // v bottom tee
    0x252c, // w top tee
    0x2502, // x vertical line
    0x2264, // y less than or equal
    0x2265, // z greater than or equal
    0x03c0, // { pi
    0x2260, // | not equal
    0x00a3, // } pound
    0x00b7, // ~ middle dot
};

const int MaxParams = 32;

} // namespace

//*******************************
// paletteColor
//*******************************
void paletteColor(int i, uint8_t &r, uint8_t &g, uint8_t &b) {
    static const uint8_t base[16][3] = {{0, 0, 0},       {205, 0, 0},   {0, 205, 0},   {205, 205, 0},
                                        {0, 0, 238},     {205, 0, 205}, {0, 205, 205}, {229, 229, 229},
                                        {127, 127, 127}, {255, 0, 0},   {0, 255, 0},   {255, 255, 0},
                                        {92, 92, 255},   {255, 0, 255}, {0, 255, 255}, {255, 255, 255}};
    if (i < 16) {
        r = base[i][0];
        g = base[i][1];
        b = base[i][2];
    } else if (i < 232) {
        // the 6x6x6 cube
        static const uint8_t level[6] = {0, 95, 135, 175, 215, 255};
        const int n = i - 16;
        r = level[n / 36];
        g = level[(n / 6) % 6];
        b = level[n % 6];
    } else {
        const uint8_t v = static_cast<uint8_t>(8 + (i - 232) * 10);
        r = g = b = v;
    }
}

//*******************************
// VtScreen::VtScreen
//*******************************
VtScreen::VtScreen(int cols, int rows, int scrollbackLines)
    : cols_(max(1, cols)), rows_(max(1, rows)), scrollbackMax_(static_cast<size_t>(max(0, scrollbackLines))) {
    main_.assign(rows_, Line(cols_));
    alt_.assign(rows_, Line(cols_));
    dirty_.assign(rows_, true);
    bottom_ = rows_ - 1;
    resetTabs();
}

//*******************************
// VtScreen::reset
//*******************************
// ESC c: everything back as it started, the scrollback kept
void VtScreen::reset() {
    altActive_ = false;
    pen_ = Cell();
    main_.assign(rows_, Line(cols_));
    alt_.assign(rows_, Line(cols_));
    cursorRow_ = cursorCol_ = 0;
    wrapPending_ = false;
    top_ = 0;
    bottom_ = rows_ - 1;
    originMode_ = insertMode_ = newLineMode_ = false;
    autoWrap_ = cursorVisible_ = true;
    appCursorKeys_ = appKeypad_ = bracketedPaste_ = reverseVideo_ = false;
    g0Graphics_ = g1Graphics_ = false;
    gl_ = 0;
    saved_ = savedAlt_ = Saved();
    title_.clear();
    state_ = State::Ground;
    resetTabs();
    touchAll();
}

//*******************************
// VtScreen::resetTabs
//*******************************
void VtScreen::resetTabs() {
    tabs_.assign(cols_, false);
    for (int c = 8; c < cols_; c += 8)
        tabs_[c] = true;
}

//*******************************
// VtScreen::viewLine
//*******************************
const Line &VtScreen::viewLine(int row, int offset) const {
    if (altActive_ || offset <= 0)
        return screen()[row];
    offset = min(offset, scrollbackSize());
    const int index = scrollbackSize() - offset + row;
    if (index < scrollbackSize())
        return scrollback_[index];
    return main_[index - scrollbackSize()];
}

//*******************************
// VtScreen::touch
//*******************************
void VtScreen::touch(int row) {
    if (row >= 0 && row < rows_)
        dirty_[row] = true;
    version_++;
}

void VtScreen::touchAll() {
    fill(dirty_.begin(), dirty_.end(), true);
    version_++;
}

void VtScreen::clearDirty() {
    fill(dirty_.begin(), dirty_.end(), false);
}

//*******************************
// VtScreen::blank
//*******************************
Cell VtScreen::blank() const {
    Cell c;
    c.bg = pen_.bg;
    return c;
}

//*******************************
// VtScreen::text
//*******************************
string VtScreen::text() const {
    string out;
    for (int r = 0; r < rows_; r++) {
        string row;
        for (const Cell &c : screen()[r]) {
            uint32_t ch = c.ch;
            if (ch < 0x80) {
                row += static_cast<char>(ch);
            } else if (ch < 0x800) {
                row += static_cast<char>(0xc0 | (ch >> 6));
                row += static_cast<char>(0x80 | (ch & 0x3f));
            } else if (ch < 0x10000) {
                row += static_cast<char>(0xe0 | (ch >> 12));
                row += static_cast<char>(0x80 | ((ch >> 6) & 0x3f));
                row += static_cast<char>(0x80 | (ch & 0x3f));
            } else {
                row += static_cast<char>(0xf0 | (ch >> 18));
                row += static_cast<char>(0x80 | ((ch >> 12) & 0x3f));
                row += static_cast<char>(0x80 | ((ch >> 6) & 0x3f));
                row += static_cast<char>(0x80 | (ch & 0x3f));
            }
        }
        row.erase(row.find_last_not_of(' ') + 1);
        out += row;
        if (r + 1 < rows_)
            out += '\n';
    }
    return out;
}

//*******************************
// VtScreen::resize
//*******************************
void VtScreen::resize(int cols, int rows) {
    cols = max(1, cols);
    rows = max(1, rows);
    if (cols == cols_ && rows == rows_)
        return;
    // the main screen: shrinking drops the rows under its cursor first, then pushes rows from the top into
    // the scrollback; growing takes them back from there (the cursor moving down with them)
    const int mainCursor = altActive_ ? saved_.row : cursorRow_;
    int shift = 0;
    if (rows < rows_) {
        int drop = rows_ - rows;
        const int cutBottom = min(rows_ - 1 - mainCursor, drop);
        main_.resize(rows_ - cutBottom);
        drop -= cutBottom;
        for (int i = 0; i < drop && scrollbackMax_ > 0; i++) {
            scrollback_.push_back(main_[i]);
            if (scrollback_.size() > scrollbackMax_)
                scrollback_.pop_front();
        }
        main_.erase(main_.begin(), main_.begin() + drop);
        shift = -drop;
    } else if (rows > rows_) {
        const int pull = min(rows - rows_, scrollbackSize());
        for (int i = 0; i < pull; i++) {
            main_.insert(main_.begin(), scrollback_.back());
            scrollback_.pop_back();
        }
        main_.resize(rows, Line(cols_));
        shift = pull;
    }
    for (Line &l : main_)
        l.resize(cols);
    for (Line &l : scrollback_)
        l.resize(cols);
    // the alternate screen is its program's to redraw after the resize: cut or padded at the bottom
    alt_.resize(rows, Line(cols));
    for (Line &l : alt_)
        l.resize(cols);

    int cursorRow;
    if (altActive_) {
        saved_.row = max(0, min(rows - 1, saved_.row + shift));
        cursorRow = cursorRow_;
    } else {
        cursorRow = mainCursor + shift;
    }

    cols_ = cols;
    rows_ = rows;
    cursorRow_ = max(0, min(rows_ - 1, cursorRow));
    cursorCol_ = min(cursorCol_, cols_ - 1);
    wrapPending_ = false;
    top_ = 0;
    bottom_ = rows_ - 1;
    dirty_.assign(rows_, true);
    resetTabs();
    version_++;
}

//*******************************
// VtScreen::write
//*******************************
void VtScreen::write(const char *data, size_t size) {
    for (size_t i = 0; i < size; i++)
        byte(static_cast<unsigned char>(data[i]));
}

//*******************************
// VtScreen::byte
//*******************************
void VtScreen::byte(unsigned char c) {
    // CAN and SUB end any sequence; ESC starts a new one from anywhere but a string
    if (c == 0x18 || c == 0x1a) {
        state_ = State::Ground;
        return;
    }
    switch (state_) {
    case State::Ground:
        if (utf8Left_ > 0) {
            if ((c & 0xc0) == 0x80) {
                utf8_ = (utf8_ << 6) | (c & 0x3f);
                if (--utf8Left_ == 0)
                    print(utf8_);
                return;
            }
            utf8Left_ = 0;
            print(0xfffd); // a sequence cut short, and then this byte on its own
        }
        if (c == 0x1b) {
            state_ = State::Escape;
            intermediates_.clear();
        } else if (c < 0x20 || c == 0x7f) {
            control(c);
        } else if (c < 0x80) {
            print(c);
        } else if ((c & 0xe0) == 0xc0) {
            utf8_ = c & 0x1f;
            utf8Left_ = 1;
        } else if ((c & 0xf0) == 0xe0) {
            utf8_ = c & 0x0f;
            utf8Left_ = 2;
        } else if ((c & 0xf8) == 0xf0) {
            utf8_ = c & 0x07;
            utf8Left_ = 3;
        } else {
            print(0xfffd);
        }
        return;

    case State::Escape:
        if (c == 0x1b) {
            intermediates_.clear();
            return;
        }
        if (c < 0x20) {
            control(c);
            return;
        }
        if (c == '[') {
            state_ = State::Csi;
            params_.clear();
            paramIsSub_.clear();
            private_ = 0;
            intermediates_.clear();
        } else if (c == ']') {
            state_ = State::Osc;
            osc_.clear();
        } else if (c == 'P' || c == 'X' || c == '^' || c == '_') {
            state_ = State::IgnoreString; // DCS, SOS, PM, APC: read up to ST and dropped
        } else if (c >= 0x20 && c <= 0x2f) {
            intermediates_ += static_cast<char>(c);
            state_ = State::EscapeIntermediate;
        } else {
            state_ = State::Ground;
            escape(c);
        }
        return;

    case State::EscapeIntermediate:
        if (c < 0x20) {
            control(c);
        } else if (c <= 0x2f) {
            intermediates_ += static_cast<char>(c);
        } else {
            state_ = State::Ground;
            escape(c);
        }
        return;

    case State::Csi:
        if (c == 0x1b) {
            state_ = State::Escape;
            intermediates_.clear();
        } else if (c < 0x20) {
            control(c); // a control inside a sequence is carried out and the sequence goes on
        } else if (c >= '0' && c <= '9') {
            if (params_.empty()) {
                params_.push_back(-1);
                paramIsSub_.push_back(false);
            }
            int &p = params_.back();
            p = min((p < 0 ? 0 : p) * 10 + (c - '0'), 99999);
        } else if (c == ';' || c == ':') {
            // -1 is an empty parameter: its default
            if (params_.empty()) {
                params_.push_back(-1);
                paramIsSub_.push_back(false);
            }
            if (params_.size() < MaxParams) {
                params_.push_back(-1);
                paramIsSub_.push_back(c == ':');
            }
        } else if (c >= '<' && c <= '?') {
            private_ = static_cast<char>(c);
        } else if (c >= 0x20 && c <= 0x2f) {
            intermediates_ += static_cast<char>(c);
        } else if (c >= 0x40 && c <= 0x7e) {
            state_ = State::Ground;
            csi(c);
        }
        return;

    case State::Osc:
        if (c == 0x07) {
            osc();
            state_ = State::Ground;
        } else if (c == 0x1b) {
            state_ = State::OscEscape;
        } else if (osc_.size() < 4096) {
            osc_ += static_cast<char>(c);
        }
        return;

    case State::OscEscape:
        // ESC \ ends it; ESC and anything else ends it too, and starts over
        osc();
        state_ = State::Ground;
        if (c != '\\')
            byte(0x1b), byte(c);
        return;

    case State::IgnoreString:
        if (c == 0x1b)
            state_ = State::IgnoreStringEscape;
        else if (c == 0x07)
            state_ = State::Ground;
        return;

    case State::IgnoreStringEscape:
        state_ = c == '\\' ? State::Ground : State::IgnoreString;
        return;
    }
}

//*******************************
// VtScreen::control
//*******************************
void VtScreen::control(unsigned char c) {
    switch (c) {
    case 0x07: // BEL
        if (onBell)
            onBell();
        break;
    case 0x08: // BS
        if (cursorCol_ > 0)
            cursorCol_--;
        wrapPending_ = false;
        touch(cursorRow_);
        break;
    case 0x09: { // HT
        int col = cursorCol_ + 1;
        while (col < cols_ - 1 && !tabs_[col])
            col++;
        cursorCol_ = min(col, cols_ - 1);
        wrapPending_ = false;
        touch(cursorRow_);
        break;
    }
    case 0x0a: // LF
    case 0x0b: // VT
    case 0x0c: // FF
        lineFeed();
        if (newLineMode_)
            cursorCol_ = 0;
        break;
    case 0x0d: // CR
        cursorCol_ = 0;
        wrapPending_ = false;
        touch(cursorRow_);
        break;
    case 0x0e: // SO
        gl_ = 1;
        break;
    case 0x0f: // SI
        gl_ = 0;
        break;
    default:
        break;
    }
}

//*******************************
// VtScreen::print
//*******************************
void VtScreen::print(uint32_t ch) {
    const bool graphics = gl_ == 0 ? g0Graphics_ : g1Graphics_;
    if (graphics && ch >= 0x5f && ch <= 0x7e)
        ch = DecGraphics[ch - 0x5f];
    if (wrapPending_ && autoWrap_) {
        cursorCol_ = 0;
        lineFeed();
    }
    wrapPending_ = false;
    Line &l = screen()[cursorRow_];
    if (insertMode_) {
        l.insert(l.begin() + cursorCol_, blank());
        l.pop_back();
    }
    Cell &cell = l[cursorCol_];
    cell = pen_;
    cell.ch = ch;
    lastPrinted_ = ch;
    if (cursorCol_ == cols_ - 1)
        wrapPending_ = true;
    else
        cursorCol_++;
    touch(cursorRow_);
}

//*******************************
// VtScreen::lineFeed
//*******************************
void VtScreen::lineFeed() {
    wrapPending_ = false;
    if (cursorRow_ == bottom_)
        scrollUp(top_, bottom_, 1);
    else if (cursorRow_ < rows_ - 1)
        cursorRow_++;
    touch(cursorRow_);
}

//*******************************
// VtScreen::reverseIndex
//*******************************
void VtScreen::reverseIndex() {
    wrapPending_ = false;
    if (cursorRow_ == top_)
        scrollDown(top_, bottom_, 1);
    else if (cursorRow_ > 0)
        cursorRow_--;
    touch(cursorRow_);
}

//*******************************
// VtScreen::scrollUp
//*******************************
// the lines of [top, bottom] move up n; what leaves the top of the whole main screen goes to the scrollback
void VtScreen::scrollUp(int top, int bottom, int n) {
    vector<Line> &s = screen();
    n = min(n, bottom - top + 1);
    for (int i = 0; i < n; i++) {
        if (!altActive_ && top == 0 && scrollbackMax_ > 0) {
            scrollback_.push_back(s[top]);
            if (scrollback_.size() > scrollbackMax_)
                scrollback_.pop_front();
        }
        s.erase(s.begin() + top);
        s.insert(s.begin() + bottom, Line(cols_, blank()));
    }
    touchAll();
}

//*******************************
// VtScreen::scrollDown
//*******************************
void VtScreen::scrollDown(int top, int bottom, int n) {
    vector<Line> &s = screen();
    n = min(n, bottom - top + 1);
    for (int i = 0; i < n; i++) {
        s.erase(s.begin() + bottom);
        s.insert(s.begin() + top, Line(cols_, blank()));
    }
    touchAll();
}

//*******************************
// VtScreen::eraseCells
//*******************************
void VtScreen::eraseCells(int row, int from, int to) {
    Line &l = screen()[row];
    from = max(0, from);
    to = min(cols_, to);
    const Cell b = blank();
    for (int c = from; c < to; c++)
        l[c] = b;
    touch(row);
}

//*******************************
// VtScreen::moveTo
//*******************************
void VtScreen::moveTo(int row, int col) {
    touch(cursorRow_);
    cursorRow_ = max(0, min(rows_ - 1, row));
    cursorCol_ = max(0, min(cols_ - 1, col));
    wrapPending_ = false;
    touch(cursorRow_);
}

//*******************************
// VtScreen::saveCursor / restoreCursor
//*******************************
void VtScreen::saveCursor() {
    Saved &s = altActive_ ? savedAlt_ : saved_;
    s.row = cursorRow_;
    s.col = cursorCol_;
    s.pen = pen_;
    s.wrapPending = wrapPending_;
    s.originMode = originMode_;
    s.g0Graphics = g0Graphics_;
    s.g1Graphics = g1Graphics_;
    s.gl = gl_;
}

void VtScreen::restoreCursor() {
    const Saved &s = altActive_ ? savedAlt_ : saved_;
    moveTo(s.row, s.col);
    pen_ = s.pen;
    wrapPending_ = s.wrapPending;
    originMode_ = s.originMode;
    g0Graphics_ = s.g0Graphics;
    g1Graphics_ = s.g1Graphics;
    gl_ = s.gl;
}

//*******************************
// VtScreen::switchScreen
//*******************************
void VtScreen::switchScreen(bool alt, bool clear) {
    if (alt == altActive_)
        return;
    altActive_ = alt;
    if (alt && clear)
        alt_.assign(rows_, Line(cols_, blank()));
    top_ = 0;
    bottom_ = rows_ - 1;
    touchAll();
}

//*******************************
// VtScreen::escape
//*******************************
void VtScreen::escape(unsigned char c) {
    if (!intermediates_.empty()) {
        const char i = intermediates_[0];
        if (i == '(' || i == ')') {
            // G0 / G1: '0' is the line-drawing set, anything else is taken as ASCII
            bool &g = i == '(' ? g0Graphics_ : g1Graphics_;
            g = c == '0';
        } else if (i == '#' && c == '8') {
            // DECALN: the screen full of E
            for (Line &l : screen())
                for (Cell &cell : l) {
                    cell = Cell();
                    cell.ch = 'E';
                }
            touchAll();
        }
        intermediates_.clear();
        return;
    }
    switch (c) {
    case '7':
        saveCursor();
        break;
    case '8':
        restoreCursor();
        break;
    case 'D': // IND
        lineFeed();
        break;
    case 'E': // NEL
        cursorCol_ = 0;
        lineFeed();
        break;
    case 'M': // RI
        reverseIndex();
        break;
    case 'H': // HTS
        tabs_[cursorCol_] = true;
        break;
    case 'c': // RIS
        reset();
        break;
    case '=':
        appKeypad_ = true;
        break;
    case '>':
        appKeypad_ = false;
        break;
    default:
        break; // ESC \ (a lone ST) and the rest
    }
}

//*******************************
// VtScreen::param
//*******************************
// the i-th parameter, or `fallback` when it is absent, empty or 0 where 0 means the default
int VtScreen::param(size_t i, int fallback) const {
    if (i >= params_.size() || params_[i] < 0)
        return fallback;
    return params_[i];
}

//*******************************
// VtScreen::reply
//*******************************
void VtScreen::reply(const string &s) {
    if (onReply)
        onReply(s);
}

//*******************************
// VtScreen::csi
//*******************************
void VtScreen::csi(unsigned char final) {
    auto count = [this](size_t i) { return max(1, param(i, 1)); };
    const int originTop = originMode_ ? top_ : 0;

    if (private_ == '?' && (final == 'h' || final == 'l')) {
        setMode(final == 'h');
        return;
    }
    if (private_ == '>' && final == 'c') {
        reply("\x1b[>0;276;0c"); // DA2: an xterm, patch level 276
        return;
    }
    if (private_ != 0 && private_ != '?')
        return; // xterm's key modifier options and the like
    if (!intermediates_.empty()) {
        if (intermediates_ == "!" && final == 'p') { // DECSTR, the soft reset
            pen_ = Cell();
            insertMode_ = originMode_ = false;
            autoWrap_ = cursorVisible_ = true;
            appCursorKeys_ = appKeypad_ = false;
            top_ = 0;
            bottom_ = rows_ - 1;
            g0Graphics_ = g1Graphics_ = false;
            gl_ = 0;
        }
        return; // DECSCUSR (cursor style, ' q') and the rest
    }

    switch (final) {
    case '@': { // ICH
        Line &l = screen()[cursorRow_];
        const int n = min(count(0), cols_ - cursorCol_);
        l.insert(l.begin() + cursorCol_, n, blank());
        l.resize(cols_);
        wrapPending_ = false;
        touch(cursorRow_);
        break;
    }
    case 'A': // CUU
        moveTo(max(cursorRow_ >= top_ ? top_ : 0, cursorRow_ - count(0)), cursorCol_);
        break;
    case 'B': // CUD
    case 'e': // VPR
        moveTo(min(cursorRow_ <= bottom_ ? bottom_ : rows_ - 1, cursorRow_ + count(0)), cursorCol_);
        break;
    case 'C': // CUF
    case 'a': // HPR
        moveTo(cursorRow_, cursorCol_ + count(0));
        break;
    case 'D': // CUB
        moveTo(cursorRow_, cursorCol_ - count(0));
        break;
    case 'E': // CNL
        moveTo(min(cursorRow_ <= bottom_ ? bottom_ : rows_ - 1, cursorRow_ + count(0)), 0);
        break;
    case 'F': // CPL
        moveTo(max(cursorRow_ >= top_ ? top_ : 0, cursorRow_ - count(0)), 0);
        break;
    case 'G': // CHA
    case '`': // HPA
        moveTo(cursorRow_, count(0) - 1);
        break;
    case 'H': // CUP
    case 'f': // HVP
        moveTo(originTop + count(0) - 1, count(1) - 1);
        if (originMode_)
            cursorRow_ = min(cursorRow_, bottom_);
        break;
    case 'I': // CHT
        for (int i = 0; i < count(0); i++)
            control(0x09);
        break;
    case 'Z': { // CBT
        for (int i = 0; i < count(0) && cursorCol_ > 0; i++) {
            int col = cursorCol_ - 1;
            while (col > 0 && !tabs_[col])
                col--;
            cursorCol_ = col;
        }
        wrapPending_ = false;
        touch(cursorRow_);
        break;
    }
    case 'J': { // ED
        const int mode = param(0, 0);
        if (mode == 0) {
            eraseCells(cursorRow_, cursorCol_, cols_);
            for (int r = cursorRow_ + 1; r < rows_; r++)
                eraseCells(r, 0, cols_);
        } else if (mode == 1) {
            eraseCells(cursorRow_, 0, cursorCol_ + 1);
            for (int r = 0; r < cursorRow_; r++)
                eraseCells(r, 0, cols_);
        } else if (mode == 2) {
            for (int r = 0; r < rows_; r++)
                eraseCells(r, 0, cols_);
        } else if (mode == 3) {
            scrollback_.clear(); // clear, as `clear` sends it
            version_++;
        }
        break;
    }
    case 'K': { // EL
        const int mode = param(0, 0);
        if (mode == 0)
            eraseCells(cursorRow_, cursorCol_, cols_);
        else if (mode == 1)
            eraseCells(cursorRow_, 0, cursorCol_ + 1);
        else if (mode == 2)
            eraseCells(cursorRow_, 0, cols_);
        break;
    }
    case 'L': // IL
        if (cursorRow_ >= top_ && cursorRow_ <= bottom_) {
            scrollDown(cursorRow_, bottom_, count(0));
            cursorCol_ = 0;
            wrapPending_ = false;
        }
        break;
    case 'M': // DL
        if (cursorRow_ >= top_ && cursorRow_ <= bottom_) {
            // lines deleted inside the screen never go to the scrollback
            vector<Line> &s = screen();
            const int n = min(count(0), bottom_ - cursorRow_ + 1);
            for (int i = 0; i < n; i++) {
                s.erase(s.begin() + cursorRow_);
                s.insert(s.begin() + bottom_, Line(cols_, blank()));
            }
            cursorCol_ = 0;
            wrapPending_ = false;
            touchAll();
        }
        break;
    case 'P': { // DCH
        Line &l = screen()[cursorRow_];
        const int n = min(count(0), cols_ - cursorCol_);
        l.erase(l.begin() + cursorCol_, l.begin() + cursorCol_ + n);
        l.resize(cols_, blank());
        wrapPending_ = false;
        touch(cursorRow_);
        break;
    }
    case 'S': // SU
        if (private_ == 0)
            scrollUp(top_, bottom_, count(0));
        break;
    case 'T': // SD
        if (private_ == 0 && params_.size() <= 1)
            scrollDown(top_, bottom_, count(0));
        break;
    case 'X': // ECH
        eraseCells(cursorRow_, cursorCol_, cursorCol_ + count(0));
        wrapPending_ = false;
        break;
    case 'b': // REP: the last character again
        for (int i = min(count(0), 65535); i > 0; i--)
            print(lastPrinted_);
        break;
    case 'c': // DA
        if (private_ == 0)
            reply("\x1b[?62;22c"); // a VT220 with ANSI colour
        break;
    case 'd': // VPA
        moveTo(originTop + count(0) - 1, cursorCol_);
        break;
    case 'g': // TBC
        if (param(0, 0) == 0)
            tabs_[cursorCol_] = false;
        else if (param(0, 0) == 3)
            fill(tabs_.begin(), tabs_.end(), false);
        break;
    case 'h':
    case 'l':
        // the ANSI modes: IRM (4) and LNM (20)
        for (size_t i = 0; i < max<size_t>(1, params_.size()); i++) {
            const int mode = param(i, 0);
            if (mode == 4)
                insertMode_ = final == 'h';
            else if (mode == 20)
                newLineMode_ = final == 'h';
        }
        break;
    case 'm':
        sgr();
        break;
    case 'n': // DSR
        if (param(0, 0) == 5) {
            reply("\x1b[0n");
        } else if (param(0, 0) == 6) {
            char buf[32];
            snprintf(buf, sizeof(buf), "\x1b[%s%d;%dR", private_ == '?' ? "?" : "", cursorRow_ - originTop + 1,
                     cursorCol_ + 1);
            reply(buf);
        }
        break;
    case 'r': { // DECSTBM
        const int t = count(0) - 1;
        const int b = param(1, rows_) <= 0 ? rows_ - 1 : min(rows_, param(1, rows_)) - 1;
        if (t < b) {
            top_ = t;
            bottom_ = b;
            moveTo(originMode_ ? top_ : 0, 0);
        }
        break;
    }
    case 's': // SCOSC
        if (private_ == 0)
            saveCursor();
        break;
    case 'u': // SCORC
        if (private_ == 0)
            restoreCursor();
        break;
    default:
        break; // t (window operations) and anything else
    }
}

//*******************************
// VtScreen::setMode
//*******************************
// the DEC private modes, CSI ? n h / l
void VtScreen::setMode(bool on) {
    for (size_t i = 0; i < max<size_t>(1, params_.size()); i++) {
        switch (param(i, 0)) {
        case 1:
            appCursorKeys_ = on;
            break;
        case 3: // DECCOLM: the column count is ours to decide; the screen is cleared as a VT100 does
            for (int r = 0; r < rows_; r++)
                eraseCells(r, 0, cols_);
            top_ = 0;
            bottom_ = rows_ - 1;
            moveTo(0, 0);
            break;
        case 5:
            reverseVideo_ = on;
            touchAll();
            break;
        case 6:
            originMode_ = on;
            moveTo(on ? top_ : 0, 0);
            break;
        case 7:
            autoWrap_ = on;
            if (!on)
                wrapPending_ = false;
            break;
        case 25:
            cursorVisible_ = on;
            touch(cursorRow_);
            break;
        case 47:
        case 1047:
            switchScreen(on, on && param(i, 0) == 1047);
            break;
        case 1048:
            if (on)
                saveCursor();
            else
                restoreCursor();
            break;
        case 1049:
            if (on) {
                saveCursor();
                switchScreen(true, true);
                savedAlt_ = saved_;
            } else {
                switchScreen(false, false);
                restoreCursor();
            }
            break;
        case 2004:
            bracketedPaste_ = on;
            break;
        default:
            break; // the mouse modes, focus events, blink: nothing to report, nothing to do
        }
    }
}

//*******************************
// VtScreen::sgr
//*******************************
void VtScreen::sgr() {
    if (params_.empty()) {
        pen_ = Cell();
        return;
    }
    // 38/48: ;5;n or ;2;r;g;b, and the colon forms 38:5:n and 38:2:[space]:r:g:b
    auto extended = [this](size_t &i, Color &out) {
        const bool colons = i + 1 < params_.size() && paramIsSub_[i + 1];
        if (i + 1 >= params_.size())
            return;
        const int kind = param(i + 1, 0);
        if (colons) {
            size_t n = i + 1;
            while (n + 1 < params_.size() && paramIsSub_[n + 1])
                n++;
            const size_t subs = n - (i + 1); // after the kind
            if (kind == 5 && subs >= 1)
                out = Color::indexed(max(0, min(255, param(i + 2, 0))));
            else if (kind == 2 && subs >= 3) {
                const size_t first = subs >= 4 ? i + 3 : i + 2; // a colour space id before r:g:b
                out = Color::rgb(param(first, 0) & 255, param(first + 1, 0) & 255, param(first + 2, 0) & 255);
            }
            i = n;
        } else if (kind == 5) {
            if (i + 2 < params_.size())
                out = Color::indexed(max(0, min(255, param(i + 2, 0))));
            i += 2;
        } else if (kind == 2) {
            if (i + 4 < params_.size())
                out = Color::rgb(param(i + 2, 0) & 255, param(i + 3, 0) & 255, param(i + 4, 0) & 255);
            i += 4;
        } else {
            i += 1;
        }
    };
    for (size_t i = 0; i < params_.size(); i++) {
        if (paramIsSub_[i])
            continue; // a sub-parameter of something not understood (4:3, a curly underline, is underline)
        const int p = param(i, 0);
        switch (p) {
        case 0:
            pen_ = Cell();
            break;
        case 1:
            pen_.attr |= Attr::Bold;
            break;
        case 2:
            pen_.attr |= Attr::Dim;
            break;
        case 3:
            pen_.attr |= Attr::Italic;
            break;
        case 4:
            if (i + 1 < params_.size() && paramIsSub_[i + 1] && param(i + 1, 0) == 0)
                pen_.attr &= ~Attr::Underline;
            else
                pen_.attr |= Attr::Underline;
            break;
        case 5:
        case 6:
            pen_.attr |= Attr::Blink;
            break;
        case 7:
            pen_.attr |= Attr::Reverse;
            break;
        case 8:
            pen_.attr |= Attr::Invisible;
            break;
        case 9:
            pen_.attr |= Attr::Strike;
            break;
        case 21:
            pen_.attr |= Attr::Underline;
            break;
        case 22:
            pen_.attr &= ~(Attr::Bold | Attr::Dim);
            break;
        case 23:
            pen_.attr &= ~Attr::Italic;
            break;
        case 24:
            pen_.attr &= ~Attr::Underline;
            break;
        case 25:
            pen_.attr &= ~Attr::Blink;
            break;
        case 27:
            pen_.attr &= ~Attr::Reverse;
            break;
        case 28:
            pen_.attr &= ~Attr::Invisible;
            break;
        case 29:
            pen_.attr &= ~Attr::Strike;
            break;
        case 38:
            extended(i, pen_.fg);
            break;
        case 39:
            pen_.fg = Color();
            break;
        case 48:
            extended(i, pen_.bg);
            break;
        case 49:
            pen_.bg = Color();
            break;
        default:
            if (p >= 30 && p <= 37)
                pen_.fg = Color::indexed(p - 30);
            else if (p >= 40 && p <= 47)
                pen_.bg = Color::indexed(p - 40);
            else if (p >= 90 && p <= 97)
                pen_.fg = Color::indexed(p - 90 + 8);
            else if (p >= 100 && p <= 107)
                pen_.bg = Color::indexed(p - 100 + 8);
            break;
        }
    }
}

//*******************************
// VtScreen::osc
//*******************************
// OSC 0 / 2 set the title; the rest (colours, clipboard, hyperlinks) is dropped
void VtScreen::osc() {
    const size_t semi = osc_.find(';');
    if (semi == string::npos)
        return;
    const string code = osc_.substr(0, semi);
    if (code == "0" || code == "2") {
        title_ = osc_.substr(semi + 1);
        version_++;
    }
}

} // namespace term

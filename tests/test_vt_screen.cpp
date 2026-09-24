//
// VtScreen: bytes in, cells out.
//
#include "core/vt_screen.h"

#include <doctest/doctest.h>

using namespace std;
using namespace term;

namespace {
string row(const VtScreen &vt, int r) {
    string s;
    for (const Cell &c : vt.line(r))
        s += c.ch < 0x80 ? static_cast<char>(c.ch) : '?';
    s.erase(s.find_last_not_of(' ') + 1);
    return s;
}
} // namespace

TEST_CASE("text, carriage return and line feed") {
    VtScreen vt(20, 5);
    vt.write("hello\r\nworld");
    CHECK(row(vt, 0) == "hello");
    CHECK(row(vt, 1) == "world");
    CHECK(vt.cursorRow() == 1);
    CHECK(vt.cursorCol() == 5);
}

TEST_CASE("the line wraps at the last column only when the next character comes") {
    VtScreen vt(5, 3);
    vt.write("abcde");
    CHECK(vt.cursorRow() == 0);
    CHECK(vt.cursorCol() == 4);
    vt.write("f");
    CHECK(row(vt, 0) == "abcde");
    CHECK(row(vt, 1) == "f");
    // a CR at the pending wrap stays on the line, as readline expects
    VtScreen vt2(5, 3);
    vt2.write("abcde\rX");
    CHECK(row(vt2, 0) == "Xbcde");
}

TEST_CASE("scrolling pushes lines into the scrollback") {
    VtScreen vt(10, 3, 100);
    vt.write("1\r\n2\r\n3\r\n4\r\n5");
    CHECK(row(vt, 0) == "3");
    CHECK(row(vt, 2) == "5");
    REQUIRE(vt.scrollbackSize() == 2);
    CHECK(vt.scrollbackLine(0)[0].ch == '1');
    CHECK(vt.viewLine(0, 2)[0].ch == '1');
    CHECK(vt.viewLine(2, 2)[0].ch == '3');
}

TEST_CASE("cursor movement and erasing") {
    VtScreen vt(10, 4);
    vt.write("0123456789\x1b[2;3HX");
    CHECK(row(vt, 1) == "  X");
    vt.write("\x1b[1;5H\x1b[K");
    CHECK(row(vt, 0) == "0123");
    vt.write("\x1b[1;3H\x1b[1K");
    CHECK(row(vt, 0) == "   3");
    vt.write("\x1b[2J");
    CHECK(vt.text() == "\n\n\n");
    vt.write("\x1b[H\x1b[3B\x1b[4C*");
    CHECK(row(vt, 3) == "    *");
    vt.write("\x1b[10A\x1b[99D+");
    CHECK(row(vt, 0) == "+");
}

TEST_CASE("insert and delete characters and lines") {
    VtScreen vt(8, 4);
    vt.write("abcdef\x1b[1;3H\x1b[2@");
    CHECK(row(vt, 0) == "ab  cdef");
    vt.write("\x1b[3P");
    CHECK(row(vt, 0) == "abdef");
    vt.write("\x1b[H1\r\n2\r\n3\r\n4\x1b[2;1H\x1b[L");
    CHECK(row(vt, 1) == "");
    CHECK(row(vt, 2) == "2");
    CHECK(row(vt, 3) == "3");
    vt.write("\x1b[M");
    CHECK(row(vt, 1) == "2");
    CHECK(row(vt, 3) == "");
    CHECK(vt.scrollbackSize() == 0); // lines deleted inside the screen never go to the scrollback
}

TEST_CASE("the scrolling region") {
    VtScreen vt(6, 5);
    vt.write("a\r\nb\r\nc\r\nd\r\ne");
    vt.write("\x1b[2;4r"); // rows 2..4
    CHECK(vt.cursorRow() == 0);
    vt.write("\x1b[4;1H\n");
    CHECK(row(vt, 0) == "a");
    CHECK(row(vt, 1) == "c");
    CHECK(row(vt, 2) == "d");
    CHECK(row(vt, 3) == "");
    CHECK(row(vt, 4) == "e");
    vt.write("\x1b[2;1H\x1bM"); // reverse index at the top of the region
    CHECK(row(vt, 1) == "");
    CHECK(row(vt, 2) == "c");
    CHECK(vt.scrollbackSize() == 0);
}

TEST_CASE("SGR colours and attributes") {
    VtScreen vt(20, 2);
    vt.write("\x1b[1;31mR\x1b[0m\x1b[38;5;208mO\x1b[38;2;1;2;3mT\x1b[48:2::9:8:7mB\x1b[7;4mV\x1b[mN");
    const Line &l = vt.line(0);
    CHECK((l[0].attr & Attr::Bold) != 0);
    CHECK(l[0].fg == Color::indexed(1));
    CHECK(l[1].fg == Color::indexed(208));
    CHECK(l[1].attr == 0);
    CHECK(l[2].fg == Color::rgb(1, 2, 3));
    CHECK(l[3].bg == Color::rgb(9, 8, 7));
    CHECK((l[4].attr & Attr::Reverse) != 0);
    CHECK((l[4].attr & Attr::Underline) != 0);
    CHECK(l[5].attr == 0);
    CHECK(l[5].fg.kind == Color::Default);
    vt.write("\x1b[92;104mx");
    CHECK(vt.line(0)[6].fg == Color::indexed(10));
    CHECK(vt.line(0)[6].bg == Color::indexed(12));
}

TEST_CASE("an erase takes the current background") {
    VtScreen vt(4, 2);
    vt.write("\x1b[44m\x1b[2J");
    CHECK(vt.cell(1, 3).bg == Color::indexed(4));
}

TEST_CASE("UTF-8, also split between writes") {
    VtScreen vt(10, 2);
    vt.write("\xc5\xbc\xc3");
    vt.write("\xb3\xe2\x94\x80");
    CHECK(vt.cell(0, 0).ch == 0x17c);
    CHECK(vt.cell(0, 1).ch == 0xf3);
    CHECK(vt.cell(0, 2).ch == 0x2500);
    vt.write("\xff");
    CHECK(vt.cell(0, 3).ch == 0xfffd);
}

TEST_CASE("the DEC line-drawing set") {
    VtScreen vt(10, 2);
    vt.write("\x1b(0lqk\x1b(Bq");
    CHECK(vt.cell(0, 0).ch == 0x250c);
    CHECK(vt.cell(0, 1).ch == 0x2500);
    CHECK(vt.cell(0, 2).ch == 0x2510);
    CHECK(vt.cell(0, 3).ch == 'q');
    vt.write("\x1b)0\x0eqx\x0fq"); // G1 through SO / SI
    CHECK(vt.cell(0, 4).ch == 0x2500);
    CHECK(vt.cell(0, 5).ch == 0x2502);
    CHECK(vt.cell(0, 6).ch == 'q');
}

TEST_CASE("the alternate screen keeps the main one") {
    VtScreen vt(10, 3, 100);
    vt.write("shell$ ls");
    vt.write("\x1b[?1049h");
    CHECK(vt.altScreen());
    CHECK(vt.text() == "\n\n");
    vt.write("\x1b[HFULL SCREEN\r\n1\r\n2\r\n3\r\n4");
    CHECK(vt.scrollbackSize() == 0); // the alternate screen has none
    vt.write("\x1b[?1049l");
    CHECK_FALSE(vt.altScreen());
    CHECK(row(vt, 0) == "shell$ ls");
    CHECK(vt.cursorCol() == 9);
}

TEST_CASE("modes: cursor keys, keypad, cursor visibility, bracketed paste") {
    VtScreen vt(10, 3);
    vt.write("\x1b[?1h\x1b=\x1b[?25l\x1b[?2004h");
    CHECK(vt.appCursorKeys());
    CHECK(vt.appKeypad());
    CHECK_FALSE(vt.cursorVisible());
    CHECK(vt.bracketedPaste());
    vt.write("\x1b[?1l\x1b>\x1b[?25h\x1b[?2004l");
    CHECK_FALSE(vt.appCursorKeys());
    CHECK_FALSE(vt.appKeypad());
    CHECK(vt.cursorVisible());
    CHECK_FALSE(vt.bracketedPaste());
}

TEST_CASE("the answers: cursor position and device attributes") {
    VtScreen vt(10, 5);
    string answer;
    vt.onReply = [&](const string &s) { answer += s; };
    vt.write("\x1b[3;7H\x1b[6n");
    CHECK(answer == "\x1b[3;7R");
    answer.clear();
    vt.write("\x1b[c");
    CHECK(answer.compare(0, 3, "\x1b[?") == 0);
    answer.clear();
    vt.write("\x1b[5n");
    CHECK(answer == "\x1b[0n");
}

TEST_CASE("OSC: the title is kept, anything else dropped, with either terminator") {
    VtScreen vt(10, 2);
    vt.write("\x1b]0;my title\x07x");
    CHECK(vt.title() == "my title");
    vt.write("\x1b]2;other\x1b\\y");
    CHECK(vt.title() == "other");
    vt.write("\x1b]52;c;aGVsbG8=\x07z");
    CHECK(vt.title() == "other");
    CHECK(row(vt, 0) == "xyz");
}

TEST_CASE("DCS and APC strings are skipped") {
    VtScreen vt(10, 2);
    vt.write("a\x1bPq#0;2;0;0;0\x1b\\b\x1b_hidden\x1b\\c");
    CHECK(row(vt, 0) == "abc");
}

TEST_CASE("save and restore the cursor with its attributes") {
    VtScreen vt(10, 3);
    vt.write("\x1b[2;4H\x1b[31m\x1b"
             "7\x1b[H\x1b[0mA\x1b"
             "8B");
    CHECK(vt.cell(1, 3).ch == 'B');
    CHECK(vt.cell(1, 3).fg == Color::indexed(1));
}

TEST_CASE("tabs") {
    VtScreen vt(30, 2);
    vt.write("a\tb\tc");
    CHECK(vt.cell(0, 8).ch == 'b');
    CHECK(vt.cell(0, 16).ch == 'c');
    vt.write("\x1b[Zd"); // back to the previous stop
    CHECK(vt.cell(0, 16).ch == 'd');
}

TEST_CASE("REP and ECH") {
    VtScreen vt(10, 2);
    vt.write("-\x1b[4b");
    CHECK(row(vt, 0) == "-----");
    vt.write("\x1b[1;2H\x1b[2X");
    CHECK(row(vt, 0) == "-  --");
}

TEST_CASE("resize: rows above the cursor go to the scrollback and come back") {
    VtScreen vt(10, 5, 100);
    vt.write("1\r\n2\r\n3\r\n4\r\n5");
    vt.resize(10, 3);
    CHECK(row(vt, 0) == "3");
    CHECK(row(vt, 2) == "5");
    CHECK(vt.cursorRow() == 2);
    CHECK(vt.scrollbackSize() == 2);
    vt.resize(10, 5);
    CHECK(row(vt, 0) == "1");
    CHECK(row(vt, 4) == "5");
    CHECK(vt.cursorRow() == 4);
    CHECK(vt.scrollbackSize() == 0);
}

TEST_CASE("resize: blank rows under the cursor go first, columns are cut and padded") {
    VtScreen vt(10, 5, 100);
    vt.write("abcdefghij\r\nxy");
    vt.resize(4, 3);
    CHECK(vt.scrollbackSize() == 0);
    CHECK(row(vt, 0) == "abcd");
    CHECK(row(vt, 1) == "xy");
    CHECK(vt.cursorRow() == 1);
    CHECK(vt.cursorCol() == 2);
    vt.resize(12, 3);
    CHECK(vt.line(0).size() == 12);
}

TEST_CASE("the dirty rows and the version") {
    VtScreen vt(10, 3);
    vt.clearDirty();
    const unsigned long v = vt.version();
    vt.write("\x1b[2;1Hx");
    CHECK(vt.version() != v);
    CHECK(vt.rowDirty(1));
    CHECK_FALSE(vt.rowDirty(2));
}

TEST_CASE("a flood of random bytes never throws or leaves the grid") {
    VtScreen vt(13, 7, 50);
    unsigned int seed = 12345;
    string junk;
    for (int i = 0; i < 200000; i++) {
        seed = seed * 1103515245 + 12345;
        const unsigned char c = static_cast<unsigned char>(seed >> 16);
        // biased towards what the parser branches on
        const char pick[] = "\x1b[;?0123456789mHJKrABCDhlPLM@X\n\r\t\x07\x0e\x0f]\\(0";
        junk += (seed & 3) ? pick[(seed >> 8) % (sizeof(pick) - 1)] : static_cast<char>(c);
    }
    vt.write(junk);
    CHECK(vt.cursorRow() >= 0);
    CHECK(vt.cursorRow() < vt.rows());
    CHECK(vt.cursorCol() >= 0);
    CHECK(vt.cursorCol() < vt.cols());
    for (int r = 0; r < vt.rows(); r++)
        CHECK(vt.line(r).size() == static_cast<size_t>(vt.cols()));
}

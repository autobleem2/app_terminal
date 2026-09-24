//
// PtyProcess: a real program on a real pseudo-terminal, its output through the VtScreen.
//
#include "core/pty_process.h"
#include "core/vt_screen.h"

#include <doctest/doctest.h>

#include <chrono>
#include <thread>

using namespace std;
using namespace term;

namespace {

// everything the program writes until it has gone (or `seconds` pass), into `vt`
bool drain(PtyProcess &pty, VtScreen &vt, int seconds = 10) {
    const auto end = chrono::steady_clock::now() + chrono::seconds(seconds);
    char buffer[4096];
    while (chrono::steady_clock::now() < end) {
        const int n = pty.read(buffer, sizeof(buffer));
        if (n < 0)
            return true;
        if (n > 0)
            vt.write(buffer, static_cast<size_t>(n));
        else
            this_thread::sleep_for(chrono::milliseconds(5));
    }
    return false;
}

// waits for `text` to show on the screen
bool waitFor(PtyProcess &pty, VtScreen &vt, const string &text, int seconds = 10) {
    const auto end = chrono::steady_clock::now() + chrono::seconds(seconds);
    char buffer[4096];
    while (chrono::steady_clock::now() < end) {
        if (vt.text().find(text) != string::npos)
            return true;
        const int n = pty.read(buffer, sizeof(buffer));
        if (n < 0)
            return vt.text().find(text) != string::npos;
        if (n > 0)
            vt.write(buffer, static_cast<size_t>(n));
        else
            this_thread::sleep_for(chrono::milliseconds(5));
    }
    return false;
}

const vector<string> Env = PtyProcess::environment({"TERM=xterm-256color"});

} // namespace

TEST_CASE("environment: set, replace and remove") {
    const vector<string> env = PtyProcess::environment({"AB_TEST_ONE=1", "AB_TEST_ONE=2", "PATH"});
    int ones = 0;
    for (const string &e : env) {
        if (e.compare(0, 12, "AB_TEST_ONE=") == 0) {
            ones++;
            CHECK(e == "AB_TEST_ONE=2");
        }
        CHECK(e.compare(0, 5, "PATH=") != 0);
    }
    CHECK(ones == 1);
}

#ifndef _WIN32

TEST_CASE("a program's output, colours and exit status") {
    PtyProcess pty;
    VtScreen vt(40, 5);
    REQUIRE(pty.start({"/bin/sh", "-c", "printf 'hello \\033[31mred\\033[0m'; exit 3"}, Env, "", 40, 5));
    REQUIRE(drain(pty, vt));
    CHECK(vt.text().find("hello red") != string::npos);
    CHECK(vt.cell(0, 6).fg == term::Color::indexed(1));
    CHECK_FALSE(pty.running());
    CHECK(pty.exitStatus() == 3);
}

TEST_CASE("the program sees a terminal of our size, and the new one after a resize") {
    PtyProcess pty;
    VtScreen vt(40, 5);
    REQUIRE(pty.start({"/bin/sh", "-c", "stty size; read x; stty size"}, Env, "", 33, 7));
    CHECK(waitFor(pty, vt, "7 33"));
    pty.resize(50, 9);
    pty.write("\r");
    CHECK(waitFor(pty, vt, "9 50"));
}

TEST_CASE("typed input reaches the program, Ctrl+C interrupts it") {
    PtyProcess pty;
    VtScreen vt(40, 6);
    REQUIRE(pty.start({"/bin/sh", "-c", "read x; echo \"got:$x\"; sleep 30"}, Env, "", 40, 6));
    pty.write("hi\r");
    CHECK(waitFor(pty, vt, "got:hi"));
    pty.write("\x03");
    CHECK(drain(pty, vt));
    CHECK_FALSE(pty.running());
    CHECK(pty.exitStatus() == 128 + 2); // SIGINT
}

TEST_CASE("a program that cannot be started") {
    PtyProcess pty;
    VtScreen vt(40, 5);
    REQUIRE(pty.start({"/no/such/shell"}, Env, "", 40, 5));
    CHECK(drain(pty, vt));
    CHECK(pty.exitStatus() == 127);
}

TEST_CASE("terminate hangs up on the program") {
    PtyProcess pty;
    REQUIRE(pty.start({"/bin/sh", "-c", "sleep 30"}, Env, "", 40, 5));
    CHECK(pty.running());
    pty.terminate();
    CHECK_FALSE(pty.running());
}

#else

TEST_CASE("a program's output through the pseudo console") {
    PtyProcess pty;
    VtScreen vt(60, 5);
    REQUIRE(pty.start({"cmd.exe", "/c", "echo hello-conpty & exit 3"}, Env, "", 60, 5));
    CHECK(waitFor(pty, vt, "hello-conpty"));
    CHECK(drain(pty, vt));
    CHECK(pty.exitStatus() == 3);
}

#endif

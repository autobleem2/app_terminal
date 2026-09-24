//
// ShellLaunch: which shell, with what, where.
//
#include "core/shell_launch.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <set>

using namespace std;
using namespace term;

namespace {
ShellFacts machine(const set<string> &files, const set<string> &dirs) {
    ShellFacts f;
    f.appDir = "/media/Apps/terminal";
    f.root = "/media";
    f.isFile = [files](const string &p) { return files.count(p) > 0; };
    f.isDir = [dirs](const string &p) { return dirs.count(p) > 0; };
    return f;
}

bool has(const vector<string> &v, const string &s) {
    return find(v.begin(), v.end(), s) != v.end();
}
} // namespace

TEST_CASE("bash with the App's bashrc, on an xterm-256color, in the home on the stick") {
    ShellFacts f = machine({"/bin/bash", "/bin/sh", "/media/Apps/terminal/bashrc"},
                           {"/media/Home", "/media/Apps/terminal/terminfo"});
    f.home = "/media/Home";
    const ShellLaunch l = ShellLaunch::plan(f);
    CHECK(l.argv == vector<string>{"/bin/bash", "--rcfile", "/media/Apps/terminal/bashrc", "-i"});
    CHECK(l.cwd == "/media/Home");
    CHECK(has(l.envChanges, "TERM=xterm-256color"));
    CHECK(has(l.envChanges, "LD_PRELOAD")); // removed: the pad preload is the App's, not the shell's
    CHECK(has(l.envChanges, "SHELL=/bin/bash"));
    CHECK(has(l.envChanges, "TERMINFO_DIRS=:/media/Apps/terminal/terminfo"));
}

TEST_CASE("no bash: $SHELL, then /bin/sh") {
    ShellFacts f = machine({"/bin/sh", "/bin/ash"}, {});
    f.envShell = "/bin/ash";
    ShellLaunch l = ShellLaunch::plan(f);
    CHECK(l.argv == vector<string>{"/bin/ash", "-i"});
    f.envShell = "";
    l = ShellLaunch::plan(f);
    CHECK(l.argv == vector<string>{"/bin/sh", "-i"});
    CHECK_FALSE(has(l.envChanges, "TERMINFO_DIRS=:/media/Apps/terminal/terminfo"));
}

TEST_CASE("no HOME: the stick's Home is made the home") {
    ShellFacts f = machine({"/bin/bash"}, {});
    const ShellLaunch l = ShellLaunch::plan(f);
    CHECK(has(l.envChanges, "HOME=/media/Home"));
    CHECK(l.cwd == "/media"); // Home does not exist yet
    CHECK(l.argv == vector<string>{"/bin/bash", "-i"});
}

TEST_CASE("the shell asked for wins") {
    ShellFacts f = machine({"/bin/bash"}, {});
    f.shellOverride = "/usr/bin/zsh";
    CHECK(ShellLaunch::plan(f).argv == vector<string>{"/usr/bin/zsh"});
}

TEST_CASE("Windows: MSYS2's bash as a login shell, else cmd.exe") {
    ShellFacts f = machine({"C:/msys64/usr/bin/bash.exe"}, {});
    f.windows = true;
    ShellLaunch l = ShellLaunch::plan(f);
    CHECK(l.argv == vector<string>{"C:/msys64/usr/bin/bash.exe", "--login", "-i"});
    CHECK(has(l.envChanges, "MSYSTEM=UCRT64"));
    f = machine({}, {});
    f.windows = true;
    l = ShellLaunch::plan(f);
    CHECK(l.argv == vector<string>{"cmd.exe"});
}

//
// ShellLaunch: which shell the terminal runs, with what arguments, environment and starting folder - decided
// from facts about the machine so the tests can hand in any machine they like. Linux: bash when there is
// one (with the App's bashrc: a coloured prompt and `ls --color`, then the user's own ~/.bashrc), else
// $SHELL, else /bin/sh. Windows (the dev host): MSYS2's bash when it is installed, else cmd.exe. Either way
// the program is told it is on an xterm-256color, and the App's own compiled terminfo is offered behind
// the system's, for a machine (the console) that has none for it.
//
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace term {

struct ShellFacts {
    std::string appDir;        // the App's folder (bashrc, terminfo/ are in it)
    std::string root;          // the data root
    std::string shellOverride; // --shell / AB_TERMINAL_SHELL; "" = choose
    std::string home;          // $HOME; "" = unset (then <root>/Home)
    std::string envShell;      // $SHELL
    bool windows = false;
    std::function<bool(const std::string &)> isFile; // an existing file
    std::function<bool(const std::string &)> isDir;  // an existing folder
};

struct ShellLaunch {
    std::vector<std::string> argv;
    std::vector<std::string> envChanges; // NAME=value to set, a bare NAME to remove (PtyProcess::environment)
    std::string cwd;

    static ShellLaunch plan(const ShellFacts &facts);
};

} // namespace term

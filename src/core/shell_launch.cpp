//
// ShellLaunch: the shell and its environment.
//
#include "shell_launch.h"

using namespace std;

namespace term {

namespace {

string baseName(const string &path) {
    const size_t slash = path.find_last_of("/\\");
    string name = slash == string::npos ? path : path.substr(slash + 1);
    const size_t dot = name.rfind(".exe");
    if (dot != string::npos && dot + 4 == name.size())
        name.erase(dot);
    return name;
}

} // namespace

//*******************************
// ShellLaunch::plan
//*******************************
ShellLaunch ShellLaunch::plan(const ShellFacts &f) {
    ShellLaunch l;
    const string home = f.home.empty() ? f.root + "/Home" : f.home;
    l.cwd = f.isDir(home) ? home : f.root;

    // what a terminal says about itself; what the App's own environment put there for its SDL, not
    // (the virtual pad's preload would load into every program the shell starts)
    l.envChanges = {
        "TERM=xterm-256color", "COLORTERM=truecolor",           "LD_PRELOAD", "AB_PAD_PROFILE", "AB_PAD_DEFAULTS",
        "AB_PAD_LOG",          "SDL_GAMECONTROLLERCONFIG_FILE",
    };
    if (f.home.empty())
        l.envChanges.push_back("HOME=" + home);

    string shell = f.shellOverride;
    if (f.windows) {
        const string msysBash = "C:/msys64/usr/bin/bash.exe";
        if (shell.empty() && f.isFile(msysBash))
            shell = msysBash;
        if (shell.empty())
            shell = "cmd.exe";
        if (baseName(shell) == "bash") {
            // a login shell of MSYS2's UCRT64 environment, in the folder it was started in
            l.argv = {shell, "--login", "-i"};
            l.envChanges.push_back("MSYSTEM=UCRT64");
            l.envChanges.push_back("CHERE_INVOKING=1");
        } else {
            l.argv = {shell};
        }
        return l;
    }

    if (shell.empty()) {
        for (const string &candidate : {string("/bin/bash"), f.envShell, string("/bin/sh")}) {
            if (!candidate.empty() && f.isFile(candidate)) {
                shell = candidate;
                break;
            }
        }
    }
    if (shell.empty())
        shell = "/bin/sh";
    const string name = baseName(shell);
    const string rc = f.appDir + "/bashrc";
    if (name == "bash" && f.isFile(rc))
        l.argv = {shell, "--rcfile", rc, "-i"};
    else if (name == "bash" || name == "sh" || name == "ash" || name == "dash")
        l.argv = {shell, "-i"};
    else
        l.argv = {shell};
    l.envChanges.push_back("SHELL=" + shell);

    // the system's terminfo first (an empty entry is ncurses' built-in list), ours behind it
    const string terminfo = f.appDir + "/terminfo";
    if (f.isDir(terminfo))
        l.envChanges.push_back("TERMINFO_DIRS=:" + terminfo);
    return l;
}

} // namespace term

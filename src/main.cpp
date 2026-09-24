//
// Terminal: a terminal emulator for AutoBleem - bash (or the machine's shell) on a pseudo-terminal, drawn
// with the main GUI's theme, typed on with a USB keyboard or the pad. A multi-platform App
// (Apps/terminal/, bin/<platform key>/terminal): the launcher starts it with AB_APP_DIR and AB_ROOT in the
// environment. By hand: `terminal [--shell <program>] [<data root>]`.
//
#include "core/main.h"
#include "core/services/environment.h"
#include "core/services/environment_setup.h"
#include "core/shell_launch.h"
#include "core/version.h"
#include "terminal_app.h"

#include <ableem/engine/log.h>
#include <ableem/ui/platform.h>

#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <climits>
#include <unistd.h>
#endif

using namespace std;

namespace {

string envOr(const char *name, const string &fallback) {
    const char *v = getenv(name);
    return v && *v ? string(v) : fallback;
}

string parentOf(const string &path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == string::npos ? string(".") : path.substr(0, slash);
}

string nameOf(const string &path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == string::npos ? path : path.substr(slash + 1);
}

// the folder the program is in
string programDir() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    string path;
    for (DWORD i = 0; i < n; i++)
        path += buffer[i] < 128 ? static_cast<char>(buffer[i] == L'\\' ? L'/' : buffer[i]) : '?';
    return parentOf(path);
#else
    char buffer[PATH_MAX];
    const ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (n <= 0)
        return ".";
    return parentOf(string(buffer, static_cast<size_t>(n)));
#endif
}

// the App's folder: what the launcher said, else the folder above bin/<key>/ the program is in
string findAppDir() {
    const string given = envOr("AB_APP_DIR", "");
    if (!given.empty())
        return given;
    const string dir = programDir();
    if (nameOf(parentOf(dir)) == "bin")
        return parentOf(parentOf(dir));
    return dir;
}

bool isFile(const string &path) {
    return DirEntry::exists(path) && !DirEntry::isDirectory(path);
}

//*******************************
// runTerminal
//*******************************
int runTerminal(int argc, char *argv[]) {
    cout.setf(ios::unitbuf);
    cerr.setf(ios::unitbuf);
    ableem::Log::initConsoleOnly();
    atexit(ableem::Platform::shutdownSDL);

    string shell = envOr("AB_TERMINAL_SHELL", "");
    string root;
    for (int i = 1; i < argc; i++) {
        const string a = argv[i];
        if (a == "--shell" && i + 1 < argc) {
            shell = argv[++i];
        } else if (!a.empty() && a[0] != '-' && root.empty()) {
            root = a;
        } else {
            PLOG_INFO << "USAGE: terminal [--shell <program>] [<data root>]";
            return EXIT_FAILURE;
        }
    }
    const string appDir = findAppDir();
    if (root.empty())
        root = envOr("AB_ROOT", "");
    if (root.empty()) {
#ifdef AB_ROOT_RELATIVE_LAYOUT
        root = parentOf(parentOf(appDir)); // <root>/Apps/terminal
#else
        root = "/media";
#endif
    }
    Env::setAppDir(appDir);
    EnvironmentSetup::fromRoot(root);
    DirEntry::createDir(Env::getPathToLogsDir());
    ableem::Log::addFile(Env::getPathToLogsDir() + sep + "terminal.log");
    PLOG_INFO << "Terminal " << Version::FULL_VERSION << ", built " << Version::BUILD_TIMESTAMP << " UTC, "
              << Env::platformName() << ", app dir " << appDir << ", root " << root;

    term::ShellFacts facts;
    facts.appDir = appDir;
    facts.root = root;
    facts.shellOverride = shell;
    facts.home = envOr("HOME", "");
    facts.envShell = envOr("SHELL", "");
#ifdef _WIN32
    facts.windows = true;
    facts.home = envOr("AB_TERMINAL_HOME", ""); // not the Windows profile: the data tree's Home, as on a stick
#endif
    facts.isFile = isFile;
    facts.isDir = [](const string &p) { return DirEntry::isDirectory(p); };
    TerminalApp app(term::ShellLaunch::plan(facts), appDir);
    return app.run();
}

} // namespace

//*******************************
// main
//*******************************
int main(int argc, char *argv[]) {
    try {
        return runTerminal(argc, argv);
    } catch (const std::exception &e) {
        PLOG_ERROR << "FATAL: unhandled exception: " << e.what();
    } catch (...) {
        PLOG_ERROR << "FATAL: unhandled exception of unknown type";
    }
    return EXIT_FAILURE;
}

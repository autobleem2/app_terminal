//
// TerminalSettings: terminal.ini.
//
#include "terminal_settings.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/ini_file.h>

#include <algorithm>
#include <cstdlib>

using namespace std;
using ableem::DirEntry;

const int TerminalSettings::MinFontSize;
const int TerminalSettings::MaxFontSize;

//*******************************
// TerminalSettings::defaultPath
//*******************************
string TerminalSettings::defaultPath(const string &root) {
    string config;
    if (const char *xdg = getenv("XDG_CONFIG_HOME"))
        config = xdg;
    if (config.empty()) {
        const char *home = getenv("HOME");
        config = (home && *home ? string(home) : root + "/Home") + "/.config";
    }
    return config + "/autobleem-terminal/terminal.ini";
}

//*******************************
// TerminalSettings::load
//*******************************
void TerminalSettings::load() {
    if (path.empty() || !DirEntry::exists(path))
        return;
    ableem::IniFile ini;
    ini.load(path);
    auto number = [&](const char *key, int fallback) {
        auto it = ini.values.find(key);
        return it == ini.values.end() || it->second.empty() ? fallback : atoi(it->second.c_str());
    };
    auto flag = [&](const char *key, bool fallback) {
        auto it = ini.values.find(key);
        return it == ini.values.end() || it->second.empty() ? fallback : it->second == "true";
    };
    fontSize = max(MinFontSize, min(MaxFontSize, number("fontsize", fontSize)));
    fullscreen = flag("fullscreen", fullscreen);
    keyboard = flag("keyboard", keyboard);
    scrollback = max(100, min(20000, number("scrollback", scrollback)));
}

//*******************************
// TerminalSettings::save
//*******************************
void TerminalSettings::save() const {
    if (path.empty())
        return;
    const size_t slash = path.find_last_of('/');
    if (slash != string::npos)
        DirEntry::createDirs(path.substr(0, slash));
    ableem::IniFile ini;
    ini.section = "Terminal";
    ini.values["fontsize"] = to_string(fontSize);
    ini.values["fullscreen"] = fullscreen ? "true" : "false";
    ini.values["keyboard"] = keyboard ? "true" : "false";
    ini.values["scrollback"] = to_string(scrollback);
    ini.save(path);
}

//
// TerminalSettings: what the terminal remembers between runs - the text size, full screen, whether the pad
// keyboard shows - in <XDG config>/autobleem-terminal/terminal.ini, under the App's home on the stick (an
// update or a removal of the App never loses it).
//
#pragma once

#include <string>

struct TerminalSettings {
    static const int MinFontSize = 12;
    static const int MaxFontSize = 36;
    int fontSize = 18;
    bool fullscreen = false;
    bool keyboard = true;
    int scrollback = 2000;

    std::string path;
    // the file for the environment's XDG_CONFIG_HOME (else HOME/.config, else <root>/Home/.config)
    static std::string defaultPath(const std::string &root);
    void load();
    void save() const;
};

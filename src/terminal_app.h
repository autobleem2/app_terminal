//
// TerminalApp: the program - AppBase (the main GUI's config.ini, theme and language, the Gui) plus the
// terminal's settings and the shell it runs; run() shows the terminal until it is closed.
//
#pragma once

#include "app_base.h"
#include "core/shell_launch.h"
#include "gui/terminal_settings.h"

class TerminalApp : public AppBase {
public:
    TerminalApp(term::ShellLaunch launch, std::string appDir);
    int run();

private:
    term::ShellLaunch launch_;
    std::string appDir_;
    TerminalSettings settings_;
};

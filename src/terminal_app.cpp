//
// TerminalApp: the program.
//
#include "terminal_app.h"

#include "core/services/environment.h"
#include "gui/gui_terminal.h"

#include <ableem/engine/log.h>

using namespace std;

//*******************************
// TerminalApp::TerminalApp
//*******************************
TerminalApp::TerminalApp(term::ShellLaunch launch, string appDir)
    : AppBase("Terminal"), launch_(std::move(launch)), appDir_(std::move(appDir)) {
    // the App's own translations over the main GUI's, in the language its config.ini names
    lang_.loadMore(Env::getPathToAppLangDir());
    settings_.path = TerminalSettings::defaultPath(Env::getPathToUSBRoot());
    settings_.load();
}

//*******************************
// TerminalApp::run
//*******************************
int TerminalApp::run() {
    gui_->loadAssets();
    audio_->freeMusic(); // a terminal is no place for the theme's music
    gui_->hideMouseCursor();
    {
        GuiTerminal terminal(*gui_, settings_, launch_, appDir_ + "/fonts");
        terminal.show();
    }
    settings_.save();
    gui_->finish();
    return 0;
}

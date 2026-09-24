//
// GuiTerminal: the terminal screen.
//
#include "gui_terminal.h"

#include "core/main.h"
#include "core/services/environment.h"
#include "gui/gui.h"
#include "gui/panel_style.h"
#include "gui/screens/gui_action_menu.h"

#include <ableem/engine/log.h>

#include <algorithm>

using namespace std;
using ableem::Rect;
using term::TermKey;
using term::TermMods;

namespace {
const int ReadBudget = 256 * 1024; // bytes of output taken per frame, so a flood still lets the pad in
const unsigned int RepeatDelay = 380, RepeatInterval = 70;
const int FullScreenMargin = 6;
} // namespace

//*******************************
// GuiTerminal::GuiTerminal
//*******************************
GuiTerminal::GuiTerminal(ableem::GuiBase &gui, TerminalSettings &settings, term::ShellLaunch launch, string fontDir)
    : GuiScreen(gui), settings_(settings), launch_(std::move(launch)), fontDir_(std::move(fontDir)),
      vt_(80, 24, settings.scrollback) {}

//*******************************
// GuiTerminal::loadFonts
//*******************************
void GuiTerminal::loadFonts() {
    const string regular = fontDir_ + "/DejaVuSansMono.ttf";
    const string bold = fontDir_ + "/DejaVuSansMono-Bold.ttf";
    if (!view_.loadFonts(renderer, regular, bold, settings_.fontSize)) {
        // without its own face the terminal still works in the theme's font - not monospaced, but readable
        const string fallback = Env::getPathToFontsDir() + sep + "OpenSans-Medium.ttf";
        PLOG_ERROR << "Cannot load " << regular << ", using " << fallback;
        view_.loadFonts(renderer, fallback, fallback, settings_.fontSize);
    }
    keyFont_ = ableem::Font::load(renderer, regular, 18);
    if (!keyFont_.valid())
        keyFont_ = ableem::Font::load(renderer, Env::getPathToFontsDir() + sep + "OpenSans-Medium.ttf", 18);
}

//*******************************
// GuiTerminal::layout
//*******************************
// where the grid and the keyboard go, and so how many rows and columns the terminal has
void GuiTerminal::layout() {
    Rect area;
    if (settings_.fullscreen) {
        area = Rect(FullScreenMargin, FullScreenMargin, renderer.width() - 2 * FullScreenMargin,
                    renderer.height() - 2 * FullScreenMargin);
    } else {
        const Rect content = gui->classicContent();
        area = Rect(content.x + 14, content.y + 6, content.w - 28, content.h - 10);
    }
    if (settings_.keyboard) {
        const int width = min(area.w, 940);
        const int height = keyboard_.heightFor(width);
        keyboardArea_ = Rect(area.x + (area.w - width) / 2, area.y + area.h - height, width, height);
        area.h -= height + 6;
    }
    termArea_ = area;
    const int cols = view_.colsFor(area.w), rows = view_.rowsFor(area.h);
    if (cols != vt_.cols() || rows != vt_.rows()) {
        vt_.resize(cols, rows);
        pty_.resize(cols, rows);
    }
    const int gridW = static_cast<int>(cols * view_.cellWidth());
    gridX_ = area.x + (area.w - gridW) / 2;
    gridY_ = area.y + (area.h - rows * view_.cellHeight()) / 2;
    scrollOffset_ = min(scrollOffset_, vt_.scrollbackSize());
}

//*******************************
// GuiTerminal::init
//*******************************
void GuiTerminal::init() {
    // the whole keyboard is the program's: Esc included, no keyboard-as-pad on a dev host
    gui->input().setRawKeyboard(true);
    gui->input().setKeyboardAsPad(false);
    loadFonts();
    layout();
    vt_.onReply = [this](const string &bytes) { pty_.write(bytes); };

    const vector<string> env = term::PtyProcess::environment(launch_.envChanges);
    string command;
    for (const string &a : launch_.argv)
        command += (command.empty() ? "" : " ") + a;
    PLOG_INFO << "Starting " << command << " in " << launch_.cwd << " on " << vt_.cols() << "x" << vt_.rows();
    started_ = pty_.start(launch_.argv, env, launch_.cwd, vt_.cols(), vt_.rows());
    if (!started_) {
        PLOG_ERROR << "Cannot start the shell: " << pty_.error();
        message(_("Cannot start the shell:") + " " + pty_.error());
        ended_ = true;
    }
}

//*******************************
// GuiTerminal::message
//*******************************
void GuiTerminal::message(const string &text) {
    vt_.write("\r\n\x1b[0;1;33m" + text + "\x1b[0m\r\n");
}

//*******************************
// GuiTerminal::send
//*******************************
void GuiTerminal::send(const string &bytes) {
    if (bytes.empty() || ended_)
        return;
    pty_.write(bytes);
    scrollOffset_ = 0; // typing brings the view back to the live screen, as every terminal does
}

void GuiTerminal::sendKey(TermKey key, TermMods mods) {
    send(term::KeyEncoder::key(key, mods, vt_.appCursorKeys()));
}

//*******************************
// GuiTerminal::pumpOutput
//*******************************
bool GuiTerminal::pumpOutput() {
    if (!started_ || ended_)
        return false;
    char buffer[16384];
    int taken = 0;
    bool any = false;
    while (taken < ReadBudget) {
        const int n = pty_.read(buffer, sizeof(buffer));
        if (n > 0) {
            vt_.write(buffer, static_cast<size_t>(n));
            taken += n;
            any = true;
        } else {
            if (n < 0)
                shellEnded();
            break;
        }
    }
    if (any && scrollOffset_ > 0)
        scrollOffset_ = min(scrollOffset_, vt_.scrollbackSize());
    return any || ended_;
}

//*******************************
// GuiTerminal::shellEnded
//*******************************
// `exit` closes the terminal; a shell that failed stays on screen until a press, so its last words can be read
void GuiTerminal::shellEnded() {
    pty_.running();
    const int status = pty_.exitStatus();
    PLOG_INFO << "The shell has ended, status " << status;
    ended_ = true;
    if (status == 0 || gui->input().quitRequested()) {
        menuVisible = false;
        return;
    }
    message(_("The shell has ended.") + " (" + to_string(status) + ") " + _("Press any button to close."));
    scrollOffset_ = 0;
}

//*******************************
// GuiTerminal::scroll
//*******************************
void GuiTerminal::scroll(int lines) {
    const int before = scrollOffset_;
    scrollOffset_ = max(0, min(vt_.scrollbackSize(), scrollOffset_ + lines));
    if (scrollOffset_ != before)
        app.audio().cursor.play();
}

//*******************************
// GuiTerminal::hints
//*******************************
string GuiTerminal::hints() const {
    string status;
    if (scrollOffset_ > 0)
        status = _("Lines back:") + " " + to_string(scrollOffset_) + "  ";
    if (ended_)
        return status + "|@X| " + _("Close");
    if (settings_.keyboard)
        return status + "|@X| " + _("Type") + "  |@O| " + _("Delete") + "  |@S| " + _("Space") + "  |@T| " +
               _("Hide keyboard") + "  |@Start| " + _("Enter") + "  |@Select| " + _("Menu") + "  |@L1|/|@R1| " +
               _("Shift/Ctrl") + "  |@L2|/|@R2| " + _("Scroll");
    return status + "|@X| " + _("Enter") + "  |@O| Esc  |@S| Tab  |@T| " + _("Keyboard") + "  |@Select| " + _("Menu") +
           "  |@L1|/|@R1| " + _("Page Up/Down") + "  |@L2|/|@R2| " + _("Scroll");
}

//*******************************
// GuiTerminal::render
//*******************************
void GuiTerminal::render() {
    renderer.clear();
    if (settings_.fullscreen) {
        renderer.setDrawColor(ableem::Color(0, 0, 0, 255));
        renderer.fillRect();
    } else {
        gui->renderBackground();
        gui->renderTextBar();
        string title = _("Terminal");
        if (!vt_.title().empty())
            title += " - " + vt_.title().substr(0, 60);
        gui->renderHeader(title);
    }
    renderer.setDrawColor(TerminalView::defaultBackground());
    renderer.setBlendMode(ableem::BlendMode::None);
    renderer.fillRect(termArea_);
    renderer.setBlendMode(ableem::BlendMode::Blend);
    view_.render(renderer, vt_, gridX_, gridY_, scrollOffset_, !ended_);
    if (settings_.keyboard && keyFont_.valid())
        keyboard_.render(*gui, renderer, keyboardArea_, keyFont_);
    if (!settings_.fullscreen)
        gui->renderStatus(hints());
    renderer.present();
}

//*******************************
// GuiTerminal::onKey
//*******************************
// a USB keyboard: the keys that are not characters become their sequences, a character with Ctrl or Alt its
// control code (the character alone arrives as TextInput)
void GuiTerminal::onKey(const Event &e) {
    TermMods mods;
    mods.shift = (e.mods & ableem::KeyMod::Shift) != 0;
    mods.ctrl = (e.mods & ableem::KeyMod::Ctrl) != 0;
    mods.alt = (e.mods & ableem::KeyMod::Alt) != 0;
    if (mods.shift && (e.key == Key::PageUp || e.key == Key::PageDown)) {
        scroll(e.key == Key::PageUp ? vt_.rows() - 1 : -(vt_.rows() - 1));
        return;
    }
    static const struct {
        Key key;
        TermKey term;
    } map[] = {{Key::Escape, TermKey::Escape}, {Key::Return, TermKey::Return},
               {Key::Up, TermKey::Up},         {Key::Down, TermKey::Down},
               {Key::Left, TermKey::Left},     {Key::Right, TermKey::Right},
               {Key::PageUp, TermKey::PageUp}, {Key::PageDown, TermKey::PageDown},
               {Key::Home, TermKey::Home},     {Key::End, TermKey::End},
               {Key::Tab, TermKey::Tab},       {Key::Backspace, TermKey::Backspace},
               {Key::Delete, TermKey::Delete}, {Key::Insert, TermKey::Insert},
               {Key::F1, TermKey::F1},         {Key::F2, TermKey::F2},
               {Key::F3, TermKey::F3},         {Key::F4, TermKey::F4},
               {Key::F5, TermKey::F5},         {Key::F6, TermKey::F6},
               {Key::F7, TermKey::F7},         {Key::F8, TermKey::F8},
               {Key::F9, TermKey::F9},         {Key::F10, TermKey::F10},
               {Key::F11, TermKey::F11},       {Key::F12, TermKey::F12}};
    for (const auto &m : map) {
        if (m.key == e.key) {
            sendKey(m.term, mods);
            return;
        }
    }
    if (e.code != 0 && (mods.ctrl || mods.alt)) {
        send(term::KeyEncoder::character(e.code, mods));
        swallowText_ = true; // Alt+x may come as text too: that "x" was just sent with its ESC
    }
}

//*******************************
// GuiTerminal::onDpad
//*******************************
void GuiTerminal::onDpad(Button direction) {
    if (settings_.keyboard) {
        keyboard_.move(direction == Button::DpadLeft ? -1 : (direction == Button::DpadRight ? 1 : 0),
                       direction == Button::DpadUp ? -1 : (direction == Button::DpadDown ? 1 : 0));
        app.audio().cursor.play();
        return;
    }
    switch (direction) {
    case Button::DpadUp:
        sendKey(TermKey::Up);
        break;
    case Button::DpadDown:
        sendKey(TermKey::Down);
        break;
    case Button::DpadLeft:
        sendKey(TermKey::Left);
        break;
    case Button::DpadRight:
        sendKey(TermKey::Right);
        break;
    default:
        break;
    }
}

//*******************************
// GuiTerminal::repeatDpad
//*******************************
// a held d-pad goes on moving (or sending arrows), after a pause, as a held key does
void GuiTerminal::repeatDpad() {
    if (heldDpad_ == Button::None)
        return;
    ableem::Input &in = gui->input();
    const bool held =
        (heldDpad_ == Button::DpadUp && in.dpadUp()) || (heldDpad_ == Button::DpadDown && in.dpadDown()) ||
        (heldDpad_ == Button::DpadLeft && in.dpadLeft()) || (heldDpad_ == Button::DpadRight && in.dpadRight());
    if (!held) {
        heldDpad_ = Button::None;
        return;
    }
    const unsigned int now = gui->platform().ticks();
    if (now >= nextRepeat_) {
        onDpad(heldDpad_);
        nextRepeat_ = now + RepeatInterval;
    }
}

//*******************************
// GuiTerminal::onButton
//*******************************
void GuiTerminal::onButton(Button button) {
    if (ended_) {
        menuVisible = false;
        return;
    }
    if (button == Button::Select) {
        openMenu();
        return;
    }
    if (button == Button::L2 || button == Button::R2) {
        scroll(button == Button::L2 ? vt_.rows() - 1 : -(vt_.rows() - 1));
        return;
    }
    if (button == Button::Triangle) {
        settings_.keyboard = !settings_.keyboard;
        settings_.save();
        layout();
        app.audio().cursor.play();
        return;
    }
    if (button == Button::Start) {
        sendKey(TermKey::Return);
        keyboard_.releaseLatches();
        return;
    }
    if (!settings_.keyboard) {
        // the pad alone: what a full-screen program is driven with
        switch (button) {
        case Button::Cross:
            sendKey(TermKey::Return);
            break;
        case Button::Circle:
            sendKey(TermKey::Escape);
            break;
        case Button::Square:
            sendKey(TermKey::Tab);
            break;
        case Button::L1:
            sendKey(TermKey::PageUp);
            break;
        case Button::R1:
            sendKey(TermKey::PageDown);
            break;
        default:
            break;
        }
        return;
    }
    switch (button) {
    case Button::Cross: {
        const PadKeyboard::Press p = keyboard_.press();
        if (p.kind == PadKeyboard::Press::Text)
            send(p.text);
        else if (p.kind == PadKeyboard::Press::Key)
            sendKey(p.key, p.mods);
        else if (p.kind == PadKeyboard::Press::Character)
            send(term::KeyEncoder::character(p.code, p.mods));
        if (p.kind != PadKeyboard::Press::None)
            keyboard_.releaseLatches();
        break;
    }
    case Button::Circle:
        sendKey(TermKey::Backspace);
        break;
    case Button::Square:
        send(" ");
        keyboard_.releaseLatches();
        break;
    case Button::L1:
        keyboard_.toggle(PadKeyboard::Modifier::Shift);
        app.audio().cursor.play();
        break;
    case Button::R1:
        keyboard_.toggle(PadKeyboard::Modifier::Ctrl);
        app.audio().cursor.play();
        break;
    default:
        break;
    }
}

//*******************************
// GuiTerminal::onEvent
//*******************************
void GuiTerminal::onEvent(const Event &e) {
    switch (e.type) {
    case Event::Type::Quit:
        menuVisible = false;
        break;
    case Event::Type::ButtonDown:
        onButton(e.button);
        break;
    case Event::Type::DpadDown:
        if (ended_) {
            menuVisible = false;
        } else if (e.button != Button::None) {
            onDpad(e.button);
            heldDpad_ = e.button;
            nextRepeat_ = gui->platform().ticks() + RepeatDelay;
        }
        break;
    case Event::Type::KeyDown:
        if (ended_) {
            if (e.key != Key::Other || e.code != 0)
                menuVisible = false;
        } else {
            onKey(e);
        }
        break;
    case Event::Type::TextInput:
        if (!swallowText_)
            send(e.text);
        break;
    case Event::Type::RenderReset:
        view_.invalidate();
        break;
    default:
        break;
    }
}

//*******************************
// GuiTerminal::openMenu
//*******************************
void GuiTerminal::openMenu() {
    enum { Keyboard, Larger, Smaller, FullScreen, CtrlC, CtrlD, CtrlZ, Close };
    GuiActionMenu menu(*gui);
    menu.title = _("Terminal");
    menu.items = {
        {settings_.keyboard ? _("Hide keyboard") : _("Show keyboard"), _("The on-screen keyboard for the pad")},
        {_("Larger text"), _("Fewer, bigger characters")},
        {_("Smaller text"), _("More rows and columns")},
        {settings_.fullscreen ? _("Leave full screen") : _("Full screen"), _("The terminal over the whole screen")},
        {_("Send Ctrl+C"), _("Interrupt the running program")},
        {_("Send Ctrl+D"), _("End of input - at an empty prompt, ends the shell")},
        {_("Send Ctrl+Z"), _("Suspend the running program")},
        {_("Close the terminal"), _("Ends the shell and every program started from it")},
    };
    menu.show();
    gui->input().setRawKeyboard(true);
    gui->input().setKeyboardAsPad(false);
    TermMods ctrl;
    ctrl.ctrl = true;
    switch (menu.result) {
    case Keyboard:
        settings_.keyboard = !settings_.keyboard;
        break;
    case Larger:
    case Smaller:
        settings_.fontSize =
            max(TerminalSettings::MinFontSize,
                min(TerminalSettings::MaxFontSize, settings_.fontSize + (menu.result == Larger ? 2 : -2)));
        loadFonts();
        break;
    case FullScreen:
        settings_.fullscreen = !settings_.fullscreen;
        break;
    case CtrlC:
        send(term::KeyEncoder::character('c', ctrl));
        break;
    case CtrlD:
        send(term::KeyEncoder::character('d', ctrl));
        break;
    case CtrlZ:
        send(term::KeyEncoder::character('z', ctrl));
        break;
    case Close:
        menuVisible = false;
        break;
    default:
        break;
    }
    settings_.save();
    layout();
    view_.invalidate();
}

//*******************************
// GuiTerminal::loop
//*******************************
void GuiTerminal::loop() {
    menuVisible = true;
    bool dirty = true;
    while (menuVisible) {
        dirty |= pumpOutput();
        Event e;
        swallowText_ = false;
        while (menuVisible && gui->input().poll(e)) {
            if (e.type == Event::Type::None)
                continue; // a mouse move and the like: nothing to draw again for
            onEvent(e);
            dirty = true;
        }
        if (!menuVisible)
            break;
        if (heldDpad_ != Button::None) {
            repeatDpad();
            dirty = true;
        }
        // nothing changed, nothing drawn: the cursor does not blink, so an idle terminal costs no frames -
        // the console's CPU is the shell's
        if (dirty) {
            render();
            dirty = false;
        } else {
            gui->platform().delay(8);
        }
    }
    if (pty_.running())
        pty_.terminate();
    gui->input().setRawKeyboard(false);
}

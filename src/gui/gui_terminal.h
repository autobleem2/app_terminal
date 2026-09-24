//
// GuiTerminal: the terminal screen - the shell's output in a classic panel (or the whole screen), the pad
// keyboard under it, and every input a terminal can get: a USB keyboard as it is, the pad through the
// on-screen keyboard or, with that hidden, as arrows and the keys a full-screen program wants (Enter, Esc,
// Tab, Page Up/Down). Select opens the menu; L2/R2 page through what scrolled away.
//
#pragma once

#include "core/key_encoder.h"
#include "core/pty_process.h"
#include "core/shell_launch.h"
#include "core/vt_screen.h"
#include "gui/gui_screen.h"
#include "pad_keyboard.h"
#include "terminal_settings.h"
#include "terminal_view.h"

#include <string>

class GuiTerminal : public GuiScreen {
public:
    GuiTerminal(ableem::GuiBase &gui, TerminalSettings &settings, term::ShellLaunch launch, std::string fontDir);

    void init() override;
    void render() override;
    void loop() override;

private:
    TerminalSettings &settings_;
    term::ShellLaunch launch_;
    std::string fontDir_;
    term::VtScreen vt_;
    term::PtyProcess pty_;
    TerminalView view_;
    PadKeyboard keyboard_;
    ableem::Font keyFont_;
    ableem::Rect termArea_, keyboardArea_;
    int gridX_ = 0, gridY_ = 0;
    int scrollOffset_ = 0;
    bool started_ = false;
    bool ended_ = false; // the shell has gone; the screen waits for a press to close
    bool swallowText_ = false;
    Button heldDpad_ = Button::None;
    unsigned int nextRepeat_ = 0;

    void loadFonts();
    void layout();
    void send(const std::string &bytes);
    void sendKey(term::TermKey key, term::TermMods mods = term::TermMods());
    bool pumpOutput();
    void shellEnded();
    void message(const std::string &text); // a line of ours in the terminal, in the terminal's colours
    void scroll(int lines);

    void onEvent(const Event &e);
    void onButton(Button button);
    void onDpad(Button direction);
    void onKey(const Event &e);
    void repeatDpad();
    void openMenu();
    std::string hints() const;
};

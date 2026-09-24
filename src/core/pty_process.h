//
// PtyProcess: a program - the shell - running on a pseudo-terminal, so that it believes it is talking to a
// real terminal: line editing, job control, Ctrl+C as a signal, programs that ask for the window size. On
// Linux it is a POSIX pty (posix_openpt, the child made the session leader with the slave as its
// controlling terminal); on Windows, where only the dev host runs this, a ConPTY pseudo console, which
// speaks the same escape sequences. Reading never blocks: the screen polls it once a frame.
//
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace term {

class PtyProcess {
public:
    PtyProcess();
    ~PtyProcess(); // ends the program (SIGHUP, as a closed terminal does) if it is still running
    PtyProcess(const PtyProcess &) = delete;
    PtyProcess &operator=(const PtyProcess &) = delete;

    // starts argv[0] (searched on PATH) with the environment `env` (the whole of it, NAME=value) in `cwd`
    // ("" = where we are), on a terminal of cols x rows. false, with error() saying why, when it cannot
    bool start(const std::vector<std::string> &argv, const std::vector<std::string> &env, const std::string &cwd,
               int cols, int rows);
    // what the program wrote since the last call, up to `size` bytes; 0 = nothing now, -1 = the program has
    // gone and everything it wrote was read
    int read(char *buffer, int size);
    // the keys typed, to the program
    void write(const std::string &bytes);
    // the terminal's new size (the program gets SIGWINCH)
    void resize(int cols, int rows);
    bool running();
    // the exit status once it is not running (-1 before, or when unknown)
    int exitStatus() const { return exitStatus_; }
    const std::string &error() const { return error_; }
    // SIGHUP, as when a terminal is closed, then SIGKILL if it takes longer than `graceMs`
    void terminate(int graceMs = 500);

    // the current environment as NAME=value lines, `set` applied over it (NAME=value replaces or adds,
    // a bare NAME removes)
    static std::vector<std::string> environment(const std::vector<std::string> &set);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    int exitStatus_ = -1;
    std::string error_;
};

} // namespace term

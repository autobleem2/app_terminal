//
// PtyProcess: the shell on a pseudo-terminal - a POSIX pty on Linux, a ConPTY pseudo console on Windows.
//
#include "pty_process.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
extern char **environ;
#endif

using namespace std;

namespace term {

namespace {

// NAME of NAME=value; on Windows names are compared without case, as the system does
string envName(const string &entry) {
    return entry.substr(0, entry.find('='));
}

bool sameName(const string &a, const string &b) {
#ifdef _WIN32
    return a.size() == b.size() &&
           equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return toupper(x) == toupper(y); });
#else
    return a == b;
#endif
}

} // namespace

//*******************************
// PtyProcess::environment
//*******************************
vector<string> PtyProcess::environment(const vector<string> &set) {
    vector<string> env;
#ifdef _WIN32
    wchar_t *block = GetEnvironmentStringsW();
    for (const wchar_t *p = block; p && *p; p += wcslen(p) + 1) {
        if (*p == L'=')
            continue; // the hidden per-drive directories ("=C:=C:\...")
        const int n = WideCharToMultiByte(CP_UTF8, 0, p, -1, nullptr, 0, nullptr, nullptr);
        string s(n > 0 ? n - 1 : 0, '\0');
        if (n > 1)
            WideCharToMultiByte(CP_UTF8, 0, p, -1, &s[0], n, nullptr, nullptr);
        env.push_back(s);
    }
    if (block)
        FreeEnvironmentStringsW(block);
#else
    for (char **e = environ; e && *e; e++)
        env.emplace_back(*e);
#endif
    for (const string &s : set) {
        const string name = envName(s);
        env.erase(remove_if(env.begin(), env.end(), [&](const string &e) { return sameName(envName(e), name); }),
                  env.end());
        if (s.find('=') != string::npos)
            env.push_back(s);
    }
    return env;
}

#ifndef _WIN32

//******************
// PtyProcess::Impl - POSIX
//******************
struct PtyProcess::Impl {
    int master = -1;
    pid_t pid = -1;
};

static void sleepMs(int ms) {
    this_thread::sleep_for(chrono::milliseconds(ms));
}

PtyProcess::PtyProcess() : impl_(new Impl) {}

PtyProcess::~PtyProcess() {
    terminate();
}

//*******************************
// PtyProcess::start
//*******************************
bool PtyProcess::start(const vector<string> &argv, const vector<string> &env, const string &cwd, int cols, int rows) {
    if (argv.empty()) {
        error_ = "nothing to run";
        return false;
    }
    const int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
        error_ = string("no pseudo-terminal: ") + strerror(errno);
        if (master >= 0)
            close(master);
        return false;
    }
    const char *name = ptsname(master);
    if (!name) {
        error_ = string("no pseudo-terminal name: ") + strerror(errno);
        close(master);
        return false;
    }
    const string slaveName = name;
    winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ioctl(master, TIOCSWINSZ, &ws);

    // everything the child needs, made before fork(): nothing may be allocated after it
    vector<char *> args, envp;
    for (const string &a : argv)
        args.push_back(const_cast<char *>(a.c_str()));
    args.push_back(nullptr);
    for (const string &e : env)
        envp.push_back(const_cast<char *>(e.c_str()));
    envp.push_back(nullptr);
    const long maxFd = sysconf(_SC_OPEN_MAX) > 0 ? sysconf(_SC_OPEN_MAX) : 1024;

    const pid_t pid = fork();
    if (pid < 0) {
        error_ = string("fork: ") + strerror(errno);
        close(master);
        return false;
    }
    if (pid == 0) {
        // the child: its own session, the pty's slave as its controlling terminal and its stdio
        setsid();
        const int slave = open(slaveName.c_str(), O_RDWR);
        if (slave < 0)
            _exit(126);
        ioctl(slave, TIOCSCTTY, 0);
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        // nothing of ours goes along: the window's and the pads' devices, the log, the master
        for (long fd = 3; fd < maxFd; fd++)
            close(static_cast<int>(fd));
        // the signals as a fresh program expects them (SDL and the launcher may have changed some)
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, nullptr);
        for (int sig = 1; sig < NSIG; sig++)
            signal(sig, SIG_DFL);
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) {
            // stays where it is
        }
        environ = envp.data();
        execvp(args[0], args.data());
        _exit(127);
    }
    fcntl(master, F_SETFL, fcntl(master, F_GETFL) | O_NONBLOCK);
    fcntl(master, F_SETFD, FD_CLOEXEC);
    impl_->master = master;
    impl_->pid = pid;
    exitStatus_ = -1;
    return true;
}

//*******************************
// PtyProcess::read
//*******************************
int PtyProcess::read(char *buffer, int size) {
    if (impl_->master < 0)
        return -1;
    const ssize_t n = ::read(impl_->master, buffer, static_cast<size_t>(size));
    if (n > 0)
        return static_cast<int>(n);
    if (n < 0 && (errno == EAGAIN || errno == EINTR)) {
        // the shell may be gone while something it started in the background keeps the terminal open
        return running() ? 0 : -1;
    }
    // EIO: every holder of the slave has closed it - the shell is exiting; its status a moment later
    for (int t = 0; t < 1000 && running(); t += 5)
        sleepMs(5);
    return -1;
}

//*******************************
// PtyProcess::write
//*******************************
void PtyProcess::write(const string &bytes) {
    size_t done = 0;
    int waited = 0;
    while (impl_->master >= 0 && done < bytes.size()) {
        const ssize_t n = ::write(impl_->master, bytes.data() + done, bytes.size() - done);
        if (n > 0) {
            done += static_cast<size_t>(n);
        } else if (n < 0 && (errno == EAGAIN || errno == EINTR) && waited < 1000) {
            sleepMs(5); // the program is not reading its input; give it a moment
            waited += 5;
        } else {
            break;
        }
    }
}

//*******************************
// PtyProcess::resize
//*******************************
void PtyProcess::resize(int cols, int rows) {
    if (impl_->master < 0)
        return;
    winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ioctl(impl_->master, TIOCSWINSZ, &ws);
}

//*******************************
// PtyProcess::running
//*******************************
bool PtyProcess::running() {
    if (impl_->pid <= 0)
        return false;
    int status = 0;
    const pid_t r = waitpid(impl_->pid, &status, WNOHANG);
    if (r == 0)
        return true;
    if (r == impl_->pid)
        exitStatus_ = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    impl_->pid = -1;
    return false;
}

//*******************************
// PtyProcess::terminate
//*******************************
void PtyProcess::terminate(int graceMs) {
    if (impl_->pid > 0) {
        kill(-impl_->pid, SIGHUP); // the shell's process group, as a terminal hanging up
        kill(impl_->pid, SIGHUP);
        for (int t = 0; t < graceMs && running(); t += 10)
            sleepMs(10);
        if (impl_->pid > 0) {
            kill(-impl_->pid, SIGKILL);
            kill(impl_->pid, SIGKILL);
            int status = 0;
            waitpid(impl_->pid, &status, 0);
            impl_->pid = -1;
        }
    }
    if (impl_->master >= 0) {
        close(impl_->master);
        impl_->master = -1;
    }
}

#else // _WIN32

//******************
// PtyProcess::Impl - ConPTY
//******************
// CreatePseudoConsole is Windows 10 1809 and newer; it is looked up at run time, so an older MinGW's
// headers and an older Windows both do (the second saying so in error())
namespace {
typedef void *Hpcon;
typedef HRESULT(WINAPI *CreatePseudoConsoleFn)(COORD, HANDLE, HANDLE, DWORD, Hpcon *);
typedef HRESULT(WINAPI *ResizePseudoConsoleFn)(Hpcon, COORD);
typedef void(WINAPI *ClosePseudoConsoleFn)(Hpcon);
const DWORD_PTR AttributePseudoConsole = 0x00020016; // PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE

// GetProcAddress as a plain function pointer, which casts to any other without a warning
void (*proc(HMODULE module, const char *name))() {
    return reinterpret_cast<void (*)()>(GetProcAddress(module, name));
}

wstring widen(const string &s) {
    if (s.empty())
        return wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

// one argument as CommandLineToArgvW reads it back
string quoteArg(const string &a) {
    if (!a.empty() && a.find_first_of(" \t\"") == string::npos)
        return a;
    string out = "\"";
    int backslashes = 0;
    for (char c : a) {
        if (c == '\\') {
            backslashes++;
        } else if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out += c;
            backslashes = 0;
        } else {
            out.append(backslashes, '\\');
            out += c;
            backslashes = 0;
        }
    }
    out.append(backslashes * 2, '\\');
    return out + "\"";
}
} // namespace

struct PtyProcess::Impl {
    HANDLE inWrite = INVALID_HANDLE_VALUE, outRead = INVALID_HANDLE_VALUE;
    Hpcon pc = nullptr;
    PROCESS_INFORMATION pi = {};
    ResizePseudoConsoleFn resizePc = nullptr;
    ClosePseudoConsoleFn closePc = nullptr;

    // ClosePseudoConsole waits, on some Windows 10 builds, for its output to be read: off the main thread
    void closePseudoConsole() {
        if (!pc)
            return;
        Hpcon p = pc;
        ClosePseudoConsoleFn f = closePc;
        pc = nullptr;
        thread([p, f]() { f(p); }).detach();
    }
};

PtyProcess::PtyProcess() : impl_(new Impl) {}

PtyProcess::~PtyProcess() {
    terminate();
}

bool PtyProcess::start(const vector<string> &argv, const vector<string> &env, const string &cwd, int cols, int rows) {
    if (argv.empty()) {
        error_ = "nothing to run";
        return false;
    }
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    auto createPc = reinterpret_cast<CreatePseudoConsoleFn>(proc(kernel, "CreatePseudoConsole"));
    impl_->resizePc = reinterpret_cast<ResizePseudoConsoleFn>(proc(kernel, "ResizePseudoConsole"));
    impl_->closePc = reinterpret_cast<ClosePseudoConsoleFn>(proc(kernel, "ClosePseudoConsole"));
    if (!createPc || !impl_->resizePc || !impl_->closePc) {
        error_ = "this Windows has no pseudo console (Windows 10 1809 or newer is needed)";
        return false;
    }
    HANDLE inRead, outWrite;
    if (!CreatePipe(&inRead, &impl_->inWrite, nullptr, 0) || !CreatePipe(&impl_->outRead, &outWrite, nullptr, 0)) {
        error_ = "CreatePipe failed";
        return false;
    }
    COORD size = {static_cast<SHORT>(cols), static_cast<SHORT>(rows)};
    const HRESULT hr = createPc(size, inRead, outWrite, 0, &impl_->pc);
    CloseHandle(inRead); // the pseudo console holds its own
    CloseHandle(outWrite);
    if (FAILED(hr)) {
        error_ = "CreatePseudoConsole failed";
        return false;
    }

    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    vector<char> attrs(attrSize);
    STARTUPINFOEXW si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    // no std handles of ours: the child's console is the pseudo console alone
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrs.data());
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrSize) ||
        !UpdateProcThreadAttribute(si.lpAttributeList, 0, AttributePseudoConsole, impl_->pc, sizeof(impl_->pc), nullptr,
                                   nullptr)) {
        error_ = "UpdateProcThreadAttribute failed";
        return false;
    }

    string commandLine;
    for (const string &a : argv)
        commandLine += (commandLine.empty() ? "" : " ") + quoteArg(a);
    wstring cmd = widen(commandLine);
    wstring block;
    for (const string &e : env) {
        block += widen(e);
        block += L'\0';
    }
    block += L'\0';
    const wstring dir = widen(cwd);
    const BOOL ok = CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                                   EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, &block[0],
                                   dir.empty() ? nullptr : dir.c_str(), &si.StartupInfo, &impl_->pi);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    if (!ok) {
        error_ = "cannot start " + argv[0] + " (error " + to_string(GetLastError()) + ")";
        impl_->closePseudoConsole();
        return false;
    }
    exitStatus_ = -1;
    return true;
}

int PtyProcess::read(char *buffer, int size) {
    if (impl_->outRead == INVALID_HANDLE_VALUE)
        return -1;
    DWORD avail = 0;
    if (!PeekNamedPipe(impl_->outRead, nullptr, 0, nullptr, &avail, nullptr))
        return -1; // broken: the pseudo console is closed and drained
    if (avail == 0) {
        // the program has gone: closing the pseudo console flushes what it still holds and breaks the pipe
        if (!running())
            impl_->closePseudoConsole();
        return 0;
    }
    DWORD got = 0;
    if (!ReadFile(impl_->outRead, buffer, min<DWORD>(avail, static_cast<DWORD>(size)), &got, nullptr))
        return -1;
    return static_cast<int>(got);
}

void PtyProcess::write(const string &bytes) {
    if (impl_->inWrite == INVALID_HANDLE_VALUE || bytes.empty())
        return;
    DWORD written = 0;
    WriteFile(impl_->inWrite, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
}

void PtyProcess::resize(int cols, int rows) {
    if (impl_->pc)
        impl_->resizePc(impl_->pc, COORD{static_cast<SHORT>(cols), static_cast<SHORT>(rows)});
}

bool PtyProcess::running() {
    if (!impl_->pi.hProcess)
        return false;
    if (WaitForSingleObject(impl_->pi.hProcess, 0) == WAIT_TIMEOUT)
        return true;
    DWORD code = 0;
    if (GetExitCodeProcess(impl_->pi.hProcess, &code))
        exitStatus_ = static_cast<int>(code);
    CloseHandle(impl_->pi.hProcess);
    CloseHandle(impl_->pi.hThread);
    impl_->pi = PROCESS_INFORMATION();
    return false;
}

void PtyProcess::terminate(int graceMs) {
    impl_->closePseudoConsole(); // the program gets CTRL_CLOSE_EVENT, as a closed console window sends
    if (impl_->pi.hProcess) {
        if (WaitForSingleObject(impl_->pi.hProcess, static_cast<DWORD>(graceMs)) == WAIT_TIMEOUT)
            TerminateProcess(impl_->pi.hProcess, 1);
        running();
    }
    if (impl_->inWrite != INVALID_HANDLE_VALUE) {
        CloseHandle(impl_->inWrite);
        impl_->inWrite = INVALID_HANDLE_VALUE;
    }
    if (impl_->outRead != INVALID_HANDLE_VALUE) {
        CloseHandle(impl_->outRead);
        impl_->outRead = INVALID_HANDLE_VALUE;
    }
}

#endif

} // namespace term

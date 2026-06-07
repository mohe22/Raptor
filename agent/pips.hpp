#pragma once
#include <cstring>
#include <format>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdexcept>

struct Pty {
    pid_t pid   {-1};
    int   master{-1};

    static Pty create() {
        Pty shell;
        struct winsize ws{};
        ws.ws_row = 40;
        ws.ws_col = 120;

        shell.pid = forkpty(&shell.master, nullptr, nullptr, &ws);
        if (shell.pid == -1)
            throw std::runtime_error(std::format("forkpty failed: {}", strerror(errno)));
        if (shell.pid == 0) {
            ::setenv("TERM",        "xterm-256color", 1);
            ::setenv("COLORTERM",   "truecolor",      1);

            // Colored prompt:
            // green user@host
            // blue cwd
            // red # for root, $ otherwise
            ::setenv(
                "PS1",
                "\\[\\e[1;32m\\]\\u@\\h"
                "\\[\\e[0m\\]:"
                "\\[\\e[1;34m\\]\\w"
                "\\[\\e[0m\\]"
                "\\[\\e[1;31m\\]\\$ "
                "\\[\\e[0m\\]",
                1
            );

            ::setenv("PS2",         "\\[\\e[33m\\]> \\[\\e[0m\\]", 1);

            ::setenv("HISTCONTROL", "ignoredups:erasedups", 1);
            ::setenv("HISTSIZE",    "10000",                1);
            ::setenv("HISTFILESIZE","20000",                1);

            ::execl(
                "/bin/bash",
                "bash",
                "--norc",
                "--noprofile",
                "-i",
                nullptr
            );

            ::_exit(1);
        }

        /* suppress local echo on the master side */
        struct termios tios{};
        if (::tcgetattr(shell.master, &tios) == 0) {
            tios.c_lflag &= ~static_cast<tcflag_t>(ECHO | ECHOE | ECHOK | ECHONL);
            ::tcsetattr(shell.master, TCSANOW, &tios);
        }
        return shell;
    }

    void close() {
        if (master != -1) { ::close(master); master = -1; }
        if (pid    >   0) {
            ::kill(pid, SIGTERM);
            ::waitpid(pid, nullptr, 0);
            pid = -1;
        }
    }
};

struct Pip {
    int   in  {-1};   /* write end  → shell stdin  */
    int   out {-1};   /* read  end  ← shell stdout */
    int   err {-1};   /* read  end  ← shell stderr */
    pid_t pid {-1};

    static Pip create() {
        Pip fds;
        int stdinPipe[2], stdoutPipe[2], stderrPipe[2];

        if (::pipe(stdinPipe)  < 0 ||
            ::pipe(stdoutPipe) < 0 ||
            ::pipe(stderrPipe) < 0)
            throw std::runtime_error(
                std::format("pipe() failed: {}", strerror(errno)));

        pid_t pid = ::fork();
        if (pid == -1)
            throw std::runtime_error(
                std::format("fork failed: {}", strerror(errno)));

        if (pid == 0) {
            ::dup2(stdinPipe[0],  STDIN_FILENO);
            ::dup2(stdoutPipe[1], STDOUT_FILENO);
            ::dup2(stderrPipe[1], STDERR_FILENO);
            ::close(stdinPipe[1]);
            ::close(stdoutPipe[0]);
            ::close(stderrPipe[0]);
            char* const env[] = {
                (char*)"PATH=/usr/bin:/bin",
                (char*)"TERM=dumb",
                (char*)"PS1=",
                (char*)"PS2=",
                (char*)"BASH_ENV=",
                (char*)"ENV=",
                (char*)"HISTFILE=/dev/null",
                nullptr
            };

            char* const argv[] = {
                (char*)"bash",
                (char*)"--norc",
                (char*)"--noprofile",
                (char*)"--noediting",
                nullptr
            };

            execve("/bin/bash", argv, env);

            perror("execve failed");
            _exit(127);

            ::_exit(1);
        }

        ::close(stdinPipe[0]);
        ::close(stdoutPipe[1]);
        ::close(stderrPipe[1]);

        fds.in  = stdinPipe[1];
        fds.out = stdoutPipe[0];
        fds.err = stderrPipe[0];
        fds.pid = pid;
        return fds;
    }

    void close() {
        if (in  != -1) { ::close(in);  in  = -1; }
        if (out != -1) { ::close(out); out = -1; }
        if (err != -1) { ::close(err); err = -1; }
        if (pid >   0) {
            ::kill(pid, SIGTERM);
            ::waitpid(pid, nullptr, 0);
            pid = -1;
        }
    }
};

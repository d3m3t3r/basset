#include <stdio.h>
#include <unistd.h>

#include <sys/ptrace.h>
#include <sys/wait.h>

#include <linux/ptrace.h>

#undef PTRACE_CONT
#undef PTRACE_SETOPTIONS
#undef PTRACE_TRACEME

#include <csignal>
#include <iostream>
#include <string>

using std::cerr;
using std::string;

int main(int argc, char *argv[]) {
  argv++;

  if (auto pid = fork()) {
    if (pid == -1) {
      perror("cannot fork()");
      return -1;
    }

    int wstatus;

    if (wait(&wstatus) == -1) {
      perror("cannot wait()");
      return -1;
    }

    if (!WIFSTOPPED(wstatus) || WSTOPSIG(wstatus) != SIGSTOP) {
      cerr << "unexpected state of child\n";
      return -1;
    }

    if (ptrace(PTRACE_SETOPTIONS, pid, nullptr,
               PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
                   PTRACE_O_TRACEEXEC) == -1) {
      perror("cannot ptrace(PTRACE_SETOPTIONS)");
      return -1;
    }

    if (ptrace(PTRACE_CONT, pid, nullptr, nullptr) == -1) {
      perror("cannot ptrace(PTRACE_CONT)");
      return -1;
    }

    while (auto pid = wait(&wstatus)) {
      if (pid == -1) {
        perror("cannot wait()");
        return -1;
      }

      if (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) {
        cerr << pid << " exited/terminated by signal\n";
      } else if (WIFSTOPPED(wstatus)) {
        cerr << pid << " stopped\n";

        if (WSTOPSIG(wstatus) == SIGTRAP) {
          switch (wstatus >> 16) {
          case PTRACE_EVENT_CLONE:
          case PTRACE_EVENT_EXEC:
          case PTRACE_EVENT_FORK:
          case PTRACE_EVENT_VFORK:
            break;
          default:
            cerr << "unknown stop event, signal: " << (wstatus >> 16) << '\n';
            return -1;
          }
        } else {
          cerr << "got signal: " << WSTOPSIG(wstatus) << '\n';
        }

        if (ptrace(PTRACE_CONT, pid, nullptr, nullptr) == -1) {
          perror("cannot ptrace(PTRACE_CONT)");
          return -1;
        }
      } else if (WIFCONTINUED(wstatus)) {
        cerr << pid << " continued\n";
      } else {
        cerr << "unexpected wait status: " << wstatus << '\n';
        return -1;
      }
    }

    return 0;
  }

  if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) == -1) {
    perror("cannot ptrace(PTRACE_TRACEME)");
    return -1;
  }

  // allow parent to ptrace(PTRACE_SETOPTIONS)
  if (raise(SIGSTOP) != 0) {
    perror("cannot raise(SIGSTOP)");
    return -1;
  }

  execvp(*argv, argv);
  // on success, execve() does not return
  perror("cannot execve()");
  return -1;
}

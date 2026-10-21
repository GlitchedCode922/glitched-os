#pragma once
#include <stdint.h>
#include <sys/types.h>
#include <uapi/signals.h> // IWYU pragma: export

typedef int sig_atomic_t;

int kill(pid_t pid, int sig);
int raise(int sig);
int signal(int sig, void (*handler)(int));
int sigaction(int sig, const sigaction_t *new, sigaction_t *old);

typedef uint32_t sigset_t;

int sigemptyset(sigset_t* set);
int sigfillset(sigset_t* set);
int sigaddset(sigset_t* set, int sig);
int sigdelset(sigset_t* set, int sig);
int sigismember(sigset_t* set, int sig);
int sigprocmask(int how, const sigset_t* set, sigset_t* old_set);

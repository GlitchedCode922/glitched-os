#pragma once
#include <sys/types.h>
#include <uapi/signals.h> // IWYU pragma: export

int kill(pid_t pid, int sig);
int raise(int sig);
int sigaction(int sig, const sigaction_t *new, sigaction_t *old);

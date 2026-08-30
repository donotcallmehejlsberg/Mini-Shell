#ifndef MINI_SHELL_PROCESS_H_
#define MINI_SHELL_PROCESS_H_

#include <sys/types.h>

#include "job.h"

int waitForChild(pid_t pid, JobState *job_state);

#endif

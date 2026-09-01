/*
 * Waits for foreground children and reaps completed background processes.
 */

#ifndef MINI_SHELL_PROCESS_H_
#define MINI_SHELL_PROCESS_H_

#include <sys/types.h>

#include "job.h"

// Waits for the specified child and returns its exit or signal status.
int waitForChild(pid_t pid, JobState *job_state);

// Reaps completed background children to prevent zombie processes.
void reapBackgroundChildren(void);

#endif

/*
 * Dispatches and executes built-in commands, external programs,
 * redirections, and pipelines.
 */

#ifndef MINI_SHELL_EXECUTOR_H_
#define MINI_SHELL_EXECUTOR_H_

#include <stdbool.h>
#include <sys/types.h>

// Executes the given command and returns its exit status.
int executeCommand(char **command_argv, bool is_background, pid_t shell_pgid,
                   const char *command_text);

#endif

#ifndef MINI_SHELL_EXECUTOR_H_
#define MINI_SHELL_EXECUTOR_H_

#include <stdbool.h>
#include <sys/types.h>

int executeCommand(char **command_argv, bool is_background, pid_t shell_pgid,
                   const char *command_text);

#endif

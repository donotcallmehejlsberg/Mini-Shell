/*
 * Detects and executes commands implemented directly by Mini-Shell.
 */

#ifndef BUILD_IN_COMMAND_H_
#define BUILD_IN_COMMAND_H_

#include <stdbool.h>
#include <sys/types.h>

// Checks whether the given command is a Mini-Shell built-in command.
bool isBuiltinCommand(const char *command);

// Executes the requested built-in command and returns its status.
int executeBuiltinCommand(char **command_argv, pid_t shell_pgid);

#endif

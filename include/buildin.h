#ifndef BUILD_IN_COMMAND
#define BUILD_IN_COMMAND

#include <stdbool.h>
#include <sys/types.h>

bool isBuiltinCommand(const char *command);
int executeBuiltinCommand(char **command_argv, pid_t shell_pgid);

#endif

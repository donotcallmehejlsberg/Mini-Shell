#ifndef MINI_SHELL_PARSER_H_
#define MINI_SHELL_PARSER_H_

#include <stdbool.h>

#define MAX_ARGUMENTS 32

int tokenizeCommand(char *buffer, char **command_argv);
bool isBackground(char **command_argv, int argument_count);
int findRedirectionIndex(char **command_argv);
int countPipes(char **command_argv);
void expandEnvironment(char **command_argv);

#endif

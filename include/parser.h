/*
 * Parses command input, expands environment variables,
 * and detects shell operators such as &, redirections, and pipes.
 */

#ifndef MINI_SHELL_PARSER_H_
#define MINI_SHELL_PARSER_H_

#include <stdbool.h>

#define MAX_ARGUMENTS 32

// Splits the input buffer into a NULL-terminated argument vector.
int tokenizeCommand(char *buffer, char **command_argv);

// Replaces standalone "$NAME" arguments with environment values.
void expandEnvironment(char **command_argv);

// Checks whether the last argument is the "&" background operator.
bool isBackground(char **command_argv, int argument_count);

// Returns the index of the first <, >, or >> operator, or -1.
int findRedirectionIndex(char **command_argv);

// Returns the number of "|" operators in the argument vector.
int countPipes(char **command_argv);

#endif

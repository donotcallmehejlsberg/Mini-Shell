/*
 * Manages the shell input buffer, reads user input,
 * and displays the interactive prompt.
 */

#ifndef MINI_SHELL_INPUT_H_
#define MINI_SHELL_INPUT_H_

#include <stdbool.h>

#define BUFFER_SIZE 256

// Allocates a zero-initialized input buffer. The caller must free it.
char *allocateBuffer(void);

// Reads one command line into the provided buffer.
bool readLine(char *buffer);

// Displays the interactive shell prompt.
void printPrompt(void);

#endif

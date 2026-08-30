#ifndef MINI_SHELL_INPUT_H_
#define MINI_SHELL_INPUT_H_

#include <stdbool.h>

#define BUFFER_SIZE 256

char *allocateBuffer(void);
bool readLine(char *buffer);
void printPrompt(void);

#endif

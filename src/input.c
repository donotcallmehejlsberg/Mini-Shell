#include "input.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "signal_handler.h"

// Input and parsing
char *allocateBuffer(void) {
  char *buffer = calloc(BUFFER_SIZE, sizeof(char));
  if (buffer == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  return buffer;
}

bool readLine(char *buffer) {
  if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
    if (errno == EINTR && consumeInterruptSignal()) {
      clearerr(stdin);
      buffer[0] = '\0';
      printf("\n");
      return true;
    }
  }
  
  return true;
}

void printPrompt(void) {
  printf("my shell > ");
  fflush(stdout);
}

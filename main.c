#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 32

int main(void) {
  char *buffer = calloc(BUFFER_SIZE, sizeof(char));

  while (1) {
    printf("my shell > ");
    fflush(stdout);
    if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
      break;
    }

    if (strcmp(buffer, "quit\n") == 0) {
      break;
    }

    if (strcmp(buffer, "\n") == 0) {
      continue;
    }

    printf("You entered: %s", buffer);
  }

  free(buffer);
  buffer = NULL;

  return 0;
}

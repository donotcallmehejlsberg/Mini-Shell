#include <stdlib.h>

#include "shell.h"
#include "input.h"

int main(void) {
  char *buffer = allocateBuffer();
  if (buffer == NULL) {
    return EXIT_FAILURE;
  }
  runShell(buffer);

  free(buffer);
  buffer = NULL;
  return EXIT_SUCCESS;
}

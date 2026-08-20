#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 256
#define MAX_ARGUMENTS 32

int main(void) {
  const char delimiters[] = " \t\n";
  char *token;
  char *buffer = NULL;
  buffer = calloc(BUFFER_SIZE, sizeof(char));
  if (buffer == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return EXIT_FAILURE;
  }

  char *command_argv[MAX_ARGUMENTS + 1];
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

    int argument_count = 0;
    token = strtok(buffer, delimiters);
    if (token == NULL) {
      continue;
    }

    while (token != NULL && argument_count < MAX_ARGUMENTS) {
      command_argv[argument_count] = token;
      argument_count++;

      token = strtok(NULL, delimiters);
    }

    if (token != NULL) {
      fprintf(stderr, "Too many arguments\n");
      continue;
    }

    command_argv[argument_count] = NULL;
    for (int i = 0; i < argument_count; i++) {
      printf("command_argv[%d] = \"%s\"\n", i, command_argv[i]);
    }

    printf("command_argv[%d] = NULL\n", argument_count);
  }

  free(buffer);
  buffer = NULL;

  return EXIT_SUCCESS;
}

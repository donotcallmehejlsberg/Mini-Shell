#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 256
#define MAX_ARGUMENTS 32

void printPrompt(void) {
  printf("my shell > ");
  fflush(stdout);
}

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
    return false;
  }

  if (strcmp(buffer, "quit\n") == 0) {
    return false;
  }

  return true;
}

void runShell(char *buffer) {
  char *command_argv[MAX_ARGUMENTS + 1];
  const char delimiters[] = " \t\n";
  char *token;

  while (1) {
    printPrompt();
    bool command = readLine(buffer);
    if (command == false) {
      break;
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
}

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

#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tokenizeCommand(char *buffer, char **command_argv) {
  int argument_count = 0;
  const char delimiters[] = " \t\n";

  char *token = strtok(buffer, delimiters);
  if (token == NULL) {
    command_argv[0] = NULL;
    return 0;
  }

  while (token != NULL && argument_count < MAX_ARGUMENTS) {
    command_argv[argument_count] = token;
    argument_count++;

    token = strtok(NULL, delimiters);
  }

  if (token != NULL) {
    fprintf(stderr, "Too many arguments\n");
    return -1;
  }

  command_argv[argument_count] = NULL;
  return argument_count;
}

void expandEnvironment(char **command_argv) {
  for (int i = 1; command_argv[i] != NULL; i++) {
    if (command_argv[i][0] != '$') {
      continue;
    }
    char *value = getenv(command_argv[i] + 1);

    if (value != NULL) {
      command_argv[i] = value;
    } else {
      command_argv[i] = "";
    }
  }
}

bool isBackground(char **command_argv, int argument_count) {
  if (argument_count == 0) {
    return false;
  }
  return strcmp(command_argv[argument_count - 1], "&") == 0;
}

int findRedirectionIndex(char **command_argv) {
  for (int i = 1; command_argv[i] != NULL; i++) {
    if (strcmp(command_argv[i], ">") == 0) {
      return i;
    }
    if (strcmp(command_argv[i], "<") == 0) {
      return i;
    }
    if (strcmp(command_argv[i], ">>") == 0) {
      return i;
    }
  }
  return -1;
}

int countPipes(char **command_argv) {
  int count = 0;
  for (int i = 1; command_argv[i] != NULL; i++) {
    if (strcmp(command_argv[i], "|") == 0) {
      count++;
    }
  }
  return count;
}

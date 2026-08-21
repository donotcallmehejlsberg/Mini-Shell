#include "shell.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 256
#define MAX_ARGUMENTS 32

static void printPrompt(void) {
  printf("my shell > ");
  fflush(stdout);
}

static bool readLine(char *buffer) {
  if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
    return false;
  }

  if (strcmp(buffer, "quit\n") == 0) {
    return false;
  }

  return true;
}

static int executeCommand(char **command_argv) {
  if (strcmp(command_argv[0], "cd") == 0) {
    if (command_argv[1] == NULL) {
      fprintf(stderr, "cd: missing directory\n");
      return EXIT_FAILURE;
    }

    if (chdir(command_argv[1]) != 0) {
      perror("cd");
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  if (strcmp(command_argv[0], "exit") == 0) {
    return EXIT_SUCCESS;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork failed");
    return EXIT_FAILURE;
  }

  if (pid == 0) {
    execvp(command_argv[0], command_argv);
    perror("execvp failed");
    _exit(EXIT_FAILURE);
  }

  int status;
  pid_t wait_result = waitpid(pid, &status, 0);
  if (wait_result == -1) {
    perror("waitpid failed");
    return EXIT_FAILURE;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  if (WIFSIGNALED(status)) {
    // Shells represent signal termination as 128 plus the signal number.
    return 128 + WTERMSIG(status);
  }

  return EXIT_FAILURE;
}

char *allocateBuffer(void) {
  char *buffer = calloc(BUFFER_SIZE, sizeof(char));
  if (buffer == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  return buffer;
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

    int command_status = executeCommand(command_argv);
    printf("Command status: %d\n", command_status);
  }
}

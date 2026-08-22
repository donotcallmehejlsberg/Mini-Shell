#include "shell.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static int findRedirectionIndex(char **command_argv) {
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

static int changeDirectory(char **command_argv) {
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

static int setupRedirection(char **command_argv, int redirection_index) {
  if (redirection_index == -1) {
    return EXIT_SUCCESS;
  }

  if (command_argv[redirection_index + 1] == NULL) {
    fprintf(stderr, "missing redirection file\n");
    return EXIT_FAILURE;
  }

  const char *redirection_operator = command_argv[redirection_index];
  const char *filename = command_argv[redirection_index + 1];

  int fd;
  int target_fd;

  if (strcmp(redirection_operator, ">") == 0) {
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    target_fd = STDOUT_FILENO;
  } else if (strcmp(redirection_operator, "<") == 0) {
    fd = open(filename, O_RDONLY);
    target_fd = STDIN_FILENO;
  } else if (strcmp(redirection_operator, ">>") == 0) {
    fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    target_fd = STDOUT_FILENO;
  } else {
    fprintf(stderr, "unsupported redirection operator\n");
    return EXIT_FAILURE;
  }

  if (fd == -1) {
    perror("open");
    return EXIT_FAILURE;
  }

  if (dup2(fd, target_fd) == -1) {
    perror("dup2");
    close(fd);
    return EXIT_FAILURE;
  }

  close(fd);
  command_argv[redirection_index] = NULL;

  return EXIT_SUCCESS;
}

static int waitForChild(pid_t pid) {
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
    return 128 + WTERMSIG(status);
  }

  return EXIT_FAILURE;
}

static void executeChild(char **command_argv, int redirection_index) {
  if (setupRedirection(command_argv, redirection_index) != EXIT_SUCCESS) {
    _exit(EXIT_FAILURE);
  }

  execvp(command_argv[0], command_argv);

  perror("execvp failed");
  _exit(EXIT_FAILURE);
}

static int executeCommand(char **command_argv) {
  if (strcmp(command_argv[0], "cd") == 0) {
    return changeDirectory(command_argv);
  }

  int redirection_index = findRedirectionIndex(command_argv);

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork failed");
    return EXIT_FAILURE;
  }

  if (pid == 0) {
    executeChild(command_argv, redirection_index);
  }

  return waitForChild(pid);
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

    if (strcmp(command_argv[0], "exit") == 0) {
      break;
    }

    int command_status = executeCommand(command_argv);
    printf("Command status: %d\n", command_status);
  }
}

#include "shell.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "executor.h"
#include "input.h"
#include "parser.h"
#include "process.h"
#include "signal_handler.h"

// Shell lifecycle
void runShell(char *buffer) {
  char *command_argv[MAX_ARGUMENTS + 1];

  // Remember which process group must own the terminal while showing a prompt
  pid_t shell_pgid = getpgrp();

  if (setupSignalHandlers() != EXIT_SUCCESS) {
    return;
  }

  while (1) {
    reapBackgroundChildren();
    printPrompt();
    bool command = readLine(buffer);
    if (command == false) {
      break;
    }

    char command_copy[BUFFER_SIZE];
    snprintf(command_copy, sizeof(command_copy), "%s", buffer);

    int argument_count = tokenizeCommand(buffer, command_argv);
    if (argument_count < 0) {
      continue;
    }

    if (argument_count == 0) {
      continue;
    }

    bool is_background = isBackground(command_argv, argument_count);
    if (is_background) {
      command_argv[argument_count - 1] = NULL;
      argument_count--;
    }

    if (strcmp(command_argv[0], "exit") == 0) {
      break;
    }

    int command_status =
        executeCommand(command_argv, is_background, shell_pgid, command_copy);
    printf("\nCommand status: %d\n", command_status);
  }
}

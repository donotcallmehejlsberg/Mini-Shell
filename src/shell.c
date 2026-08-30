#include "shell.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "buildin.h"
#include "input.h"
#include "job.h"
#include "parser.h"
#include "pipeline.h"
#include "process.h"
#include "signal_handler.h"

// Redirection
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

static void executeChild(char **command_argv, int redirection_index) {
  if (setupRedirection(command_argv, redirection_index) != EXIT_SUCCESS) {
    _exit(EXIT_FAILURE);
  }

  execvp(command_argv[0], command_argv);

  perror("execvp failed");
  _exit(EXIT_FAILURE);
}

/*
kill(pid, SIGTERM);  // Request process termination
kill(pid, SIGSTOP);  // Stop the process
kill(pid, SIGCONT);  // Continue a stopped process
*/

// Command dispatch and execution
static int executeCommand(char **command_argv, bool is_background,
                          pid_t shell_pgid, const char *command_text) {
  if (isBuiltinCommand(command_argv[0])) {
    return executeBuiltinCommand(command_argv, shell_pgid);
  }

  int pipe_count = countPipes(command_argv);
  if (pipe_count > 0) {
    return executePipeline(command_argv, pipe_count, is_background, shell_pgid);
  }

  int redirection_index = findRedirectionIndex(command_argv);

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork failed");
    return EXIT_FAILURE;
  }

  // For one command, the child is the leader of its own process group
  pid_t group_leader = pid;
  if (pid == 0) {
    if (restoreChildSignalHandlers() != EXIT_SUCCESS) {
      _exit(EXIT_FAILURE);
    }

    // In the child both zero arguments mean "use this process and its PID."
    if (setpgid(pid, group_leader) == -1) {
      perror("setpgid");
      _exit(EXIT_FAILURE);
    }
    executeChild(command_argv, redirection_index);
  }

  // Parent repeats setpgid() so scheduling order cannot cause a race
  if (setpgid(pid, group_leader) == -1) {
    perror("setpgid");
    return EXIT_FAILURE;
  }
  printf("Child PID: %d, PGID: %d\n", (int)pid, (int)getpgid(pid));

  // A background job must not take terminal control or block the shell
  if (is_background) {
    int job_id = addJob(group_leader, command_text, RUNNING);
    printf("[background] PID %d %d\n", (int)pid, job_id);
    return EXIT_SUCCESS;
  }

  // Keyboard input, Ctrl+C, and Ctrl+Z now target the foreground job group
  if (tcsetpgrp(STDIN_FILENO, group_leader) == -1) {
    perror("tcsetpgrp");
    return EXIT_FAILURE;
  }

  // Save the status because the shell must reclaim the terminal before return
  JobState job_state;
  int command_status = waitForChild(pid, &job_state);
  if (job_state == STOPPED) {
    addJob(group_leader, command_text, STOPPED);
  }

  // Give keyboard input and terminal signals back to the Mini-Shell group
  if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
    perror("tcsetpgrp");
    return EXIT_FAILURE;
  }

  return command_status;
}

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

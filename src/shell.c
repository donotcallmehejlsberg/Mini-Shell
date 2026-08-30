#include "shell.h"
#include "job.h"
#include "parser.h"
#include "process.h"
#include "signal_handler.h"
#include "buildin.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 256
#define PIPE_END_COUNT 2

// Input and parsing
char *allocateBuffer(void) {
  char *buffer = calloc(BUFFER_SIZE, sizeof(char));
  if (buffer == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  return buffer;
}

static void printPrompt(void) {
  printf("my shell > ");
  fflush(stdout);
}

static bool readLine(char *buffer) {
  if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
    if (errno == EINTR && consumeInterruptSignal()) {
      clearerr(stdin);
      buffer[0] = '\0';
      printf("\n");
      return true;
    }
  }

  if (strcmp(buffer, "quit\n") == 0) {
    return false;
  }

  return true;
}

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

// Process lifecycle
static void reapBackgroundChildren(void) {
  int status;
  pid_t finished_pid;

  while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    Job *job = findJobByPgid(finished_pid);

    if (WIFEXITED(status)) {
      printf("[background] PID %d finished with status %d\n", (int)finished_pid,
             WEXITSTATUS(status));

      if (job != NULL) {
        updateJobState(job, DONE);
        printf("[%d] Done (status %d): %s", job->job_id, WEXITSTATUS(status),
               job->command);
        removeJob(job);
      }
    } else if (WIFSIGNALED(status)) {
      printf("[background] PID %d terminated by signal %d\n", (int)finished_pid,
             WTERMSIG(status));

      if (job != NULL) {
        updateJobState(job, DONE);
        printf("[%d] Done: %s", job->job_id, job->command);
        removeJob(job);
      }
    }
  }
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

// Pipeline execution
static int executePipeline(char **command_argv, int pipe_count,
                           bool is_background, pid_t shell_pgid) {
  int pipefds[pipe_count][PIPE_END_COUNT];
  for (int i = 0; i < pipe_count; i++) {
    if (pipe(pipefds[i]) == -1) {
      perror("pipe");
      return EXIT_FAILURE;
    }
  }

  char **commands[MAX_ARGUMENTS];
  int command_count = 1;
  commands[0] = command_argv;

  for (int i = 1; command_argv[i] != NULL; i++) {
    if (strcmp(command_argv[i], "|") == 0) {
      command_argv[i] = NULL;
      commands[command_count] = &command_argv[i + 1];
      command_count++;
    }
  }

  // The first child becomes the process-group leader for the whole pipeline
  pid_t group_leader = 0;
  pid_t pids[command_count];
  for (int i = 0; i < command_count; i++) {
    pids[i] = fork();
    if (pids[i] < 0) {
      perror("fork failed");
      return EXIT_FAILURE;
    }

    if (i == 0) {
      // Parent sees the first child's real PID; the first child sees zero
      group_leader = pids[i];
    }

    if (pids[i] == 0) {
      if (restoreChildSignalHandlers() != EXIT_SUCCESS) {
        _exit(EXIT_FAILURE);
      }

      // Join this child to the pipeline group before replacing it with
      // execvp()
      if (setpgid(pids[i], group_leader) == -1) {
        perror("setpgid");
        _exit(EXIT_FAILURE);
      }

      if (i > 0) {
        dup2(pipefds[i - 1][0], STDIN_FILENO);
      }

      if (i < command_count - 1) {
        dup2(pipefds[i][1], STDOUT_FILENO);
      }

      for (int j = 0; j < pipe_count; j++) {
        close(pipefds[j][0]);
        close(pipefds[j][1]);
      }

      execvp(commands[i][0], commands[i]);
      perror("execvp failed");
      _exit(EXIT_FAILURE);
    }

    // Parent repeats setpgid() so scheduling order cannot cause a race
    if (setpgid(pids[i], group_leader) == -1) {
      perror("setpgid");
      return EXIT_FAILURE;
    }
    printf("Child PID: %d, PGID: %d\n", (int)pids[i], (int)getpgid(pids[i]));
  }

  for (int i = 0; i < pipe_count; i++) {
    close(pipefds[i][0]);
    close(pipefds[i][1]);
  }

  // A background pipeline keeps running without terminal control or waiting
  if (is_background) {
    printf("[background] PGID %d\n", (int)group_leader);
    return EXIT_SUCCESS;
  }

  // Give terminal input and signals to the entire foreground pipeline group
  if (tcsetpgrp(STDIN_FILENO, group_leader) == -1) {
    perror("tcsetpgrp");
    return EXIT_FAILURE;
  }

  JobState job_state;
  for (int i = 0; i < command_count - 1; i++) {
    waitForChild(pids[i], &job_state);
  }

  int pipeline_status = waitForChild(pids[command_count - 1], &job_state);

  // Reclaim the terminal after every process in the pipeline has been handled
  if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
    perror("tcsetpgrp");
    return EXIT_FAILURE;
  }

  return pipeline_status;
}

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

#include "pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "job.h"
#include "parser.h"
#include "process.h"
#include "signal_handler.h"

#define PIPE_END_COUNT 2

int executePipeline(char **command_argv, int pipe_count, bool is_background,
                    pid_t shell_pgid) {
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

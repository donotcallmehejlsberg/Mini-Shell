#include "buildin.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "job.h"
#include "process.h"

// Built-in commands

/******************************************************
  kill(pid, SIGTERM);  // Request process termination
  kill(pid, SIGSTOP);  // Stop the process
  kill(pid, SIGCONT);  // Continue a stopped process
*******************************************************/

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

static int backgroundJob(Job *job) {
  if (job == NULL) {
    fprintf(stderr, "bg: job not found\n");
    return EXIT_FAILURE;
  }

  if (kill(-job->pgid, SIGCONT) == -1) {
    perror("kill");
    return EXIT_FAILURE;
  }

  job->state = RUNNING;
  return EXIT_SUCCESS;
}

static int foregroundJob(Job *job, pid_t shell_pgid) {
  if (job == NULL) {
    fprintf(stderr, "fg: job not found\n");
    return EXIT_FAILURE;
  }

  if (tcsetpgrp(STDIN_FILENO, job->pgid) == -1) {
    perror("tcsetpgrp");
    return EXIT_FAILURE;
  }

  if (kill(-job->pgid, SIGCONT) == -1) {
    perror("kill");
    tcsetpgrp(STDIN_FILENO, shell_pgid);
    return EXIT_FAILURE;
  }

  job->state = RUNNING;

  JobState job_state = RUNNING;
  int command_status = waitForChild(job->pgid, &job_state);

  if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
    perror("tcsetpgrp");
    return EXIT_FAILURE;
  }

  if (job_state == DONE) {
    removeJob(job);
  } else if (job_state == STOPPED) {
    job->state = STOPPED;
  }

  return command_status;
}

bool isBuiltinCommand(const char *command) {
  if (command == NULL) {
    return false;
  }

  return strcmp(command, "cd") == 0 || strcmp(command, "jobs") == 0 ||
         strcmp(command, "fg") == 0 || strcmp(command, "bg") == 0;
}

int executeBuiltinCommand(char **command_argv, pid_t shell_pgid) {
  if (command_argv[0] == NULL) {
    return EXIT_FAILURE;
  }

  if (!isBuiltinCommand(command_argv[0])) {
    return EXIT_FAILURE;
  }

  if (strcmp(command_argv[0], "cd") == 0) {
    return changeDirectory(command_argv);
  }

  if (strcmp(command_argv[0], "jobs") == 0) {
    printJobs();
    return EXIT_SUCCESS;
  }

  if (strcmp(command_argv[0], "fg") == 0) {
    if (command_argv[1] == NULL) {
      fprintf(stderr, "fg: missing job ID\n");
      return EXIT_FAILURE;
    }

    int job_id = atoi(command_argv[1]);
    Job *job = findJobById(job_id);

    return foregroundJob(job, shell_pgid);
  }

  if (strcmp(command_argv[0], "bg") == 0) {
    if (command_argv[1] == NULL) {
      fprintf(stderr, "bg: missing job ID\n");
      return EXIT_FAILURE;
    }

    int job_id = atoi(command_argv[1]);
    Job *job = findJobById(job_id);

    return backgroundJob(job);
  }

  return EXIT_FAILURE;
}

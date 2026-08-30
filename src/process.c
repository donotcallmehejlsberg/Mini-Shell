#include "process.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

int waitForChild(pid_t pid, JobState *job_state) {
  int status;
  pid_t wait_result;

  do {
    wait_result = waitpid(pid, &status, WUNTRACED);
  } while (wait_result == -1 && errno == EINTR);

  if (wait_result == -1) {
    perror("waitpid failed");
    return EXIT_FAILURE;
  }

  if (WIFEXITED(status)) {
    *job_state = DONE;
    return WEXITSTATUS(status);
  }

  if (WIFSIGNALED(status)) {
    *job_state = DONE;
    return 128 + WTERMSIG(status);
  }

  if (WIFSTOPPED(status)) {
    *job_state = STOPPED;
    int signal_number = WSTOPSIG(status);
    printf("\n[stopped] PID %d by signal %d\n", (int)pid, signal_number);
    return 128 + signal_number;
  }

  return EXIT_FAILURE;
}

void reapBackgroundChildren(void) {
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

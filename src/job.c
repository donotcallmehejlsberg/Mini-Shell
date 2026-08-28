#include "job.h"

#include <stdio.h>
#include <string.h>

#define MAX_JOBS 5

static Job jobs[MAX_JOBS] = {0};
static int next_job_id = 1;

static const char *jobStateToString(JobState state) {
  switch (state) {
    case RUNNING:
      return "Running";
    case STOPPED:
      return "Stopped";
    case DONE:
      return "Done";
  }
  return "Unknown";
}

int addJob(pid_t pgid, const char *command, JobState state) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (jobs[i].job_id == 0) {
      jobs[i].job_id = next_job_id;
      jobs[i].pgid = pgid;
      snprintf(jobs[i].command, sizeof(jobs[i].command), "%s", command);
      jobs[i].state = state;

      next_job_id++;
      return jobs[i].job_id;
    }
  }
  return -1;
}

Job *findJobById(int job_id) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (jobs[i].job_id == job_id) {
      return &jobs[i];
    }
  }
  return NULL;
}

Job *findJobByPgid(pid_t pgid) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (jobs[i].pgid == pgid) {
      return &jobs[i];
    }
  }
  return NULL;
}

void updateJobState(Job *job, JobState state) {
  if (job == NULL) {
    return;
  }
  job->state = state;
}

void removeJob(Job *job) {
  if (job == NULL) {
    return;
  }
  memset(job, 0, sizeof(*job));
}

void printJobs(void) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (jobs[i].job_id == 0) {
      continue;
    }

    printf("[%d] %-7s %s", jobs[i].job_id, jobStateToString(jobs[i].state),
           jobs[i].command);

    size_t command_length = strlen(jobs[i].command);
    if (command_length == 0 || jobs[i].command[command_length - 1] != '\n') {
      printf("\n");
    }
  }
}

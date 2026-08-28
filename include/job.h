#ifndef MINI_SHELL_JOB_H_
#define MINI_SHELL_JOB_H_

#include <sys/types.h>

#define JOB_COMMAND_SIZE 256

typedef enum { RUNNING, STOPPED, DONE } JobState;

typedef struct {
  int job_id;
  pid_t pgid;
  char command[JOB_COMMAND_SIZE];
  JobState state;
} Job;

int addJob(pid_t pgid, const char *command, JobState state);
Job *findJobById(int job_id);
Job *findJobByPgid(pid_t pgid);
void updateJobState(Job *job, JobState state);
void removeJob(Job *job);
void printJobs(void);

#endif

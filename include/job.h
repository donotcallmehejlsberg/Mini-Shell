/*
 * Stores and manages shell jobs and their running, stopped, or completed
 * states.
 */

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

// Adds a new job and returns its shell-generated Job ID.
int addJob(pid_t pgid, const char *command, JobState state);

// Returns the job with the given Job ID, or NULL if it is not found.
Job *findJobById(int job_id);

// Returns the job with the given Process Group ID, or NULL if it is not found.
Job *findJobByPgid(pid_t pgid);

// Changes the current state of the given job.
void updateJobState(Job *job, JobState state);

// Removes the given job from the job table.
void removeJob(Job *job);

// Prints all currently stored jobs and their states.
void printJobs(void);

#endif

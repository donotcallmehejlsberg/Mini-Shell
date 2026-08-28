#include "shell.h"

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
#define MAX_ARGUMENTS 32
#define PIPE_END_COUNT 2
#define MAX_JOBS 5

typedef enum { RUNNING, STOPPED, DONE } JobState;

typedef struct {
  int job_id;
  pid_t pgid;
  char command[BUFFER_SIZE];
  JobState state;
} Job;

static Job jobs[MAX_JOBS] = {0};
static int next_job_id = 1;

static Job *findJobByPgid(pid_t pgid);
static void updateJobState(Job *job, JobState state);
static void removeJob(Job *job);

// Signal handling
static volatile sig_atomic_t received_signal = 0;

static void handleSignal(int signal_number) { received_signal = signal_number; }

static int setupSignalHandlers(void) {
  struct sigaction action = {0};

  action.sa_handler = handleSignal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  if (sigaction(SIGINT, &action, NULL) == -1) {
    perror("sigaction");
    return EXIT_FAILURE;
  }

  action.sa_handler = SIG_IGN;
  if (sigaction(SIGTSTP, &action, NULL) == -1) {
    perror("sigaction");
    return EXIT_FAILURE;
  }

  // Let the shell reclaim the terminal while its process group is background
  if (sigaction(SIGTTOU, &action, NULL) == -1) {
    perror("sigaction SIGTTOU");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

static int restoreChildSignalHandlers(void) {
  struct sigaction action = {0};

  action.sa_handler = SIG_DFL;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  // External commands should use the normal terminal-output signal behavior
  if (sigaction(SIGTTOU, &action, NULL) == -1) {
    perror("sigaction SIGTTOU");
    return EXIT_FAILURE;
  }

  if (sigaction(SIGINT, &action, NULL) == -1) {
    perror("sigaction");
    return EXIT_FAILURE;
  }

  if (sigaction(SIGTSTP, &action, NULL) == -1) {
    perror("sigaction");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

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
    if (errno == EINTR && received_signal == SIGINT) {
      clearerr(stdin);
      received_signal = 0;
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

static int tokenizeCommand(char *buffer, char **command_argv) {
  int argument_count = 0;
  const char delimiters[] = " \t\n";

  char *token = strtok(buffer, delimiters);
  if (token == NULL) {
    command_argv[0] = NULL;
    return 0;
  }

  while (token != NULL && argument_count < MAX_ARGUMENTS) {
    command_argv[argument_count] = token;
    argument_count++;

    token = strtok(NULL, delimiters);
  }

  if (token != NULL) {
    fprintf(stderr, "Too many arguments\n");
    return -1;
  }

  command_argv[argument_count] = NULL;
  return argument_count;
}

static bool isBackground(char **command_argv, int argument_count) {
  if (argument_count == 0) {
    return false;
  }
  return strcmp(command_argv[argument_count - 1], "&") == 0;
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

static int countPipes(char **command_argv) {
  int count = 0;
  for (int i = 1; command_argv[i] != NULL; i++) {
    if (strcmp(command_argv[i], "|") == 0) {
      count++;
    }
  }
  return count;
}

// Built-in commands and redirection
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

static int waitForChild(pid_t pid, JobState *job_state) {
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
    return 128 + WSTOPSIG(status);
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

static int addJob(pid_t pgid, const char *command, JobState state) {
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

static Job *findJobById(int job_id) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (jobs[i].job_id == job_id) {
      return &jobs[i];
    }
  }
  return NULL;
}

/*
kill(pid, SIGTERM);  // Request process termination
kill(pid, SIGSTOP);  // Stop the process
kill(pid, SIGCONT);  // Continue a stopped process
*/

static int bg(Job *job) {
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
static int fg(Job *job, pid_t shell_pgid) {
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

static Job *findJobByPgid(pid_t pgid) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (jobs[i].pgid == pgid) {
      return &jobs[i];
    }
  }
  return NULL;
}

static void updateJobState(Job *job, JobState state) {
  if (job == NULL) {
    return;
  }
  job->state = state;
}

static void removeJob(Job *job) {
  if (job == NULL) {
    return;
  }
  memset(job, 0, sizeof(*job));
}

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

static void printJobs(void) {
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

    return fg(job, shell_pgid);
  }

  if (strcmp(command_argv[0], "bg") == 0) {
    if (command_argv[1] == NULL) {
      fprintf(stderr, "bg: missing job ID\n");
      return EXIT_FAILURE;
    }

    int job_id = atoi(command_argv[1]);
    Job *job = findJobById(job_id);

    return bg(job);
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

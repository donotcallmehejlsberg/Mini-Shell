/*
 * Creates and executes pipelines by connecting child processes with Unix pipes.
 */

#ifndef MINI_SHELL_PIPELINE_H_
#define MINI_SHELL_PIPELINE_H_

#include <stdbool.h>
#include <sys/types.h>

// Executes commands as a pipeline and returns the pipeline's exit status.
int executePipeline(char **command_argv, int pipe_count, bool is_background,
                    pid_t shell_pgid);

#endif
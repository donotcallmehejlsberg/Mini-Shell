#ifndef MINI_SHELL_PIPELINE_H_
#define MINI_SHELL_PIPELINE_H_

#include <stdbool.h>
#include <sys/types.h>

int executePipeline(char **command_argv, int pipe_count, bool is_background,
                    pid_t shell_pgid);

#endif
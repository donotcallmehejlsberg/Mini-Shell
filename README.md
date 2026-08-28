# Mini Shell

A small Unix-like shell written in C17 as a system programming learning
project. It reads commands, creates child processes, executes programs, and
connects commands using Unix pipes and file descriptor redirection. The
project also implements basic process-group, signal, terminal, and job
management.

## Features

- Execute external programs with `fork()` and `execvp()`
- Change directories with the built-in `cd` command
- Exit the shell with `exit` or `quit`
- Redirect input with `<`
- Redirect output with `>`
- Append output with `>>`
- Connect multiple commands with pipelines
- Run individual external commands in the background with `&`
- Place individual commands and pipelines in separate process groups
- Give terminal control to the foreground process group
- Keep the shell alive when `Ctrl+C` interrupts a foreground process group
- Detect and store individual foreground processes stopped with `Ctrl+Z`
- List stored jobs with the built-in `jobs` command
- Continue a stopped job in the background with `bg JOB_ID`
- Continue a stopped job in the foreground with `fg JOB_ID`
- Wait for child processes and report command exit status
- Reap completed background processes without blocking the shell

## Requirements

- A Unix-like operating system such as Linux or macOS
- CMake 3.20 or newer
- A C17 compiler
- POSIX.1-2008 process, signal, pipe, and terminal APIs

The project is compiled as strict C17. CMake defines
`_POSIX_C_SOURCE=200809L` so POSIX declarations such as `sigaction()`,
`waitpid()`, `setpgid()`, and `tcsetpgrp()` are available on Linux.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/minishell
```

## Examples

```bash
ls -la
cd /tmp
echo hello > output.txt
cat < output.txt
cat file.txt | grep hello | sort
sleep 5 &
sleep 20
# Press Ctrl+Z while sleep is running.
jobs
bg 1
fg 1
```

Job IDs are shell-generated numbers. The current `fg` and `bg` commands use a
plain numeric argument such as `fg 1`, without the `%1` syntax supported by
larger shells.

## Project Structure

```text
include/
  job.h             Job types and job-table interface
  parser.h          Command parsing interface
  shell.h           Public shell interface
  signal_handler.h  Signal-handling interface

src/
  job.c             Job storage and state management
  main.c            Program entry point
  parser.c          Tokenization and operator detection
  shell.c           Shell loop, execution, pipelines, and terminal control
  signal_handler.c  Shell and child signal configuration
```

## Current Architecture

The project is divided by responsibility:

- `main.c` allocates the input buffer, starts the shell, and releases the
  buffer when the shell exits.
- `shell.c` owns the interactive loop and coordinates built-in commands,
  external-command execution, redirection, pipelines, process groups, and
  terminal control.
- `parser.c` converts the input buffer into an argument vector and detects
  background execution, redirection operators, and pipes.
- `job.c` owns the private job table, generates Job IDs, and provides
  operations for finding, updating, printing, and removing jobs.
- `signal_handler.c` configures shell signal behavior, restores default signal
  behavior in children, and records an interrupted input operation.

The main command flow is:

```text
main.c
  |
  v
runShell() in shell.c
  |
  +--> read input
  |
  +--> parser.c
  |      tokenize command
  |      detect &, redirection, and pipes
  |
  +--> built-in command --------------------> cd / jobs / fg / bg
  |
  +--> single external command -------------> fork / redirection / execvp
  |
  +--> pipeline -----------------------------> pipes / process group / execvp
  |
  +--> job.c --------------------------------> store and update jobs
  |
  +--> signal_handler.c ---------------------> shell and child signal behavior
```

`shell.c` currently remains the largest module because process creation,
pipeline execution, built-in dispatch, and terminal control still live there.
These responsibilities can be separated further as the project grows.

## Current Limitations

- Commands are parsed as whitespace-separated tokens.
- Quoting and escaping are not supported.
- Environment-variable expansion such as `echo $HOME` is not implemented.
- Job tracking and `fg`/`bg` currently support single-process jobs; complete
  pipeline job tracking is not implemented yet.
- Completed background children are reaped when the shell loop runs again;
  immediate `SIGCHLD`-driven notification is not implemented.

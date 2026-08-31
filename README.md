# Mini Shell

A small Unix-like shell written in C17 as a system programming learning
project. It reads commands, creates child processes, executes programs, and
connects commands using Unix pipes and file descriptor redirection. The
project also implements basic process-group, signal, terminal, and job
management.

## Features

- Execute external programs with `fork()` and `execvp()`
- Change directories with the built-in `cd` command
- Exit the shell with the built-in `exit` command
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
- Expand standalone environment-variable arguments such as `$HOME` and `$USER`
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
echo hello $USER $HOME
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
  buildin.h         Built-in command dispatch interface
  executor.h        Command execution interface
  input.h           Input buffer and prompt interface
  job.h             Job types and job-table interface
  parser.h          Command parsing interface
  pipeline.h        Pipeline execution interface
  process.h         Child-process waiting interface
  shell.h           Public shell interface
  signal_handler.h  Signal-handling interface

src/
  buildin.c         Built-in cd, jobs, fg, and bg commands
  executor.c        Command dispatch, redirection, and external execution
  input.c           Input buffer, line reading, and prompt output
  job.c             Job storage and state management
  main.c            Program entry point
  parser.c          Tokenization and operator detection
  pipeline.c        Pipe creation and pipeline execution
  process.c         Child waiting and process-status conversion
  shell.c           Interactive shell loop and high-level coordination
  signal_handler.c  Shell and child signal configuration
```

## Current Architecture

The project is divided by responsibility:

- `main.c` allocates the input buffer, starts the shell, and releases the
  buffer when the shell exits.
- `shell.c` owns the interactive loop, parses each input line, and delegates
  command execution.
- `buildin.c` detects and executes the built-in `cd`, `jobs`, `fg`, and `bg`
  commands.
- `executor.c` dispatches built-ins and pipelines, and executes single external
  commands with redirection, process groups, and terminal control.
- `input.c` allocates the input buffer, reads command lines, and prints the
  interactive prompt.
- `parser.c` converts the input buffer into an argument vector and detects
  background execution, redirection operators, and pipes.
- `pipeline.c` creates pipes, forks pipeline processes, connects their file
  descriptors, manages their process group, and waits for foreground pipelines.
- `process.c` waits for a child and converts its wait status into a command
  status and job state.
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
  +--> executor.c
  |      |
  |      +--> buildin.c ---------------------> cd / jobs / fg / bg
  |      |
  |      +--> single external command -------> fork / redirection / execvp
  |      |
  |      +--> pipeline.c --------------------> pipes / process group / execvp
  |      |
  |      +--> process.c ---------------------> wait for child / report state
  |      |
  |      +--> job.c -------------------------> store and update jobs
  |
  +--> signal_handler.c ---------------------> shell and child signal behavior
```

`shell.c` now focuses on the interactive lifecycle. Execution details are
owned by `executor.c` and `pipeline.c`.

## Current Limitations

- Commands are parsed as whitespace-separated tokens.
- Quoting and escaping are not supported.
- Environment-variable expansion supports standalone arguments such as
  `$HOME`; embedded and braced forms such as `user-$USER` and `${HOME}` are not
  supported.
- Job tracking and `fg`/`bg` currently support single-process jobs; complete
  pipeline job tracking is not implemented yet.
- Completed background children are reaped when the shell loop runs again;
  immediate `SIGCHLD`-driven notification is not implemented.

# Mini Shell

A small Unix-like shell written in C17 as a system programming learning
project. It reads commands, creates child processes, executes programs, and
connects commands using Unix pipes and file descriptor redirection.

## Features

- Execute external programs with `fork()` and `execvp()`
- Change directories with the built-in `cd` command
- Exit the shell with `exit` or `quit`
- Redirect input with `<`
- Redirect output with `>`
- Append output with `>>`
- Connect multiple commands with pipelines
- Run individual external commands in the background with `&`
- Keep the shell alive when `Ctrl+C` interrupts a foreground command
- Detect foreground processes stopped with `Ctrl+Z`
- Wait for child processes and report command exit status
- Reap completed background processes without blocking the shell

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
```

Commands are currently parsed as whitespace-separated tokens. Quoting,
escaping, and job control commands such as `jobs`, `fg`, and `bg` are not
supported. Process groups, terminal foreground control, and immediate
`SIGCHLD`-driven background-process cleanup are not implemented yet.

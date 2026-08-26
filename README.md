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
- Wait for child processes and report command exit status

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
```

Commands are currently parsed as whitespace-separated tokens. Quoting,
escaping, background execution, and job control are not supported.

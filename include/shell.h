#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "FCIT> "
#define CONT_PROMPT "> "

// Function prototypes
char** tokenize(char* cmdline);

int execute(char** arglist);
int handle_builtin(char** arglist);

void print_history(void);

char** shell_completion(const char* text, int start, int end);

// Execute one input line (may contain ; separated commands).
// Returns 2 if the shell must exit.
int execute_line(char* line);

// Reap completed background processes.
void reap_background_jobs(void);

// Return the exit status of the most recently executed foreground command.
int get_last_exit_status(void);

// ---- Feature 7: if-then-else-fi ----

// Return 1 if the line starts with the keyword "if".
int is_if_line(const char* line);

// Return 1 if the line consists only of the keyword "fi".
int is_fi_line(const char* line);

// Execute an array of lines. Lines may contain complete
// if-then-else-fi blocks (nesting is supported).
// Returns 2 if the shell must exit.
int execute_block(char** lines, int count);

#endif
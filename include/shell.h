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

// Function prototypes
char** tokenize(char* cmdline);

int execute(char** arglist);
int handle_builtin(char** arglist);

void print_history(void);

char** shell_completion(const char* text, int start, int end);

int execute_line(char* line);
#endif

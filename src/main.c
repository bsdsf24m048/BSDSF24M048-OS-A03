#include "shell.h"

#include <ctype.h>
#include <limits.h>


// Expand !n into a previous command
int expand_history(char** cmdline) {

    if ((*cmdline)[0] != '!') {
        return 1;
    }

    char* number_text = *cmdline + 1;

    if (!isdigit((unsigned char)number_text[0])) {
        fprintf(stderr, "history: invalid command\n");
        return 0;
    }

    errno = 0;

    char* end;

    unsigned long number = strtoul(number_text, &end, 10);

    while (isspace((unsigned char)*end)) {
        end++;
    }

    if (errno == ERANGE ||
        *end != '\0' ||
        number == 0 ||
        number > INT_MAX) {

        fprintf(stderr, "history: invalid command number\n");
        return 0;
    }

    HIST_ENTRY* previous = history_get((int)number);

    if (previous == NULL) {
        fprintf(stderr, "history: command not found\n");
        return 0;
    }

    char* replacement = strdup(previous->line);

    if (replacement == NULL) {
        perror("strdup");
        return 0;
    }

    free(*cmdline);
    *cmdline = replacement;

    return 1;
}


int main(void) {

    char* cmdline;
    int builtin_status;

    // Enable command and filename completion
    rl_attempted_completion_function = shell_completion;

    while (1) {

        // Reap completed background processes before each prompt
        reap_background_jobs();

        // Display prompt and read next command
        cmdline = readline(PROMPT);

        if (cmdline == NULL) {
            break;
        }

        builtin_status = 0;

        // Expand !n before adding to history
        if (!expand_history(&cmdline)) {
            free(cmdline);
            continue;
        }

        // Add non-empty commands to Readline history
        if (cmdline[0] != '\0') {
            add_history(cmdline);
        }

        // Parse and execute the full command line
        builtin_status = execute_line(cmdline);

        free(cmdline);

        if (builtin_status == 2) {
            break;
        }
    }

    printf("\nShell exited.\n");

    return 0;
}

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

    // Retrieve command from GNU Readline history
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



int main() {

    char* cmdline;
    char** arglist;

    int builtin_status;


    // Enable command and filename completion
    rl_attempted_completion_function = shell_completion;


    while ((cmdline = readline(PROMPT)) != NULL) {

        builtin_status = 0;


        // Expand !n before adding the command to history
        if (!expand_history(&cmdline)) {

            free(cmdline);

            continue;
        }


        // Store non-empty commands in Readline history
        if (cmdline[0] != '\0') {
            add_history(cmdline);
        }


        // Tokenize command
        arglist = tokenize(cmdline);


        if (arglist != NULL) {

            // Check built-in commands
            builtin_status = handle_builtin(arglist);


            // Execute external commands
            if (builtin_status == 0) {
                execute(arglist);
            }


            // Free tokenized arguments
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }

            free(arglist);
        }


        free(cmdline);


        if (builtin_status == 2) {
            break;
        }
    }


    printf("\nShell exited.\n");

    return 0;
}

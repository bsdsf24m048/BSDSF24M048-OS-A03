#include "shell.h"
#include <ctype.h>


// Expand !n into the previously stored command
int expand_history(char** cmdline) {

    // Only process commands starting with !
    if ((*cmdline)[0] != '!') {
        return 1;
    }

    char* number_text = *cmdline + 1;

    // Check that a number follows !
    if (!isdigit((unsigned char)number_text[0])) {

        fprintf(stderr, "history: invalid command\n");

        return 0;
    }

    errno = 0;

    char* end;

    unsigned long number = strtoul(number_text, &end, 10);

    // Allow trailing spaces, but no other characters
    while (isspace((unsigned char)*end)) {
        end++;
    }

    if (errno == ERANGE || *end != '\0' || number == 0) {

        fprintf(stderr, "history: invalid command number\n");

        return 0;
    }

    // Retrieve the requested command
    const char* previous = get_history_command(number);

    if (previous == NULL) {

        fprintf(stderr, "history: command not found\n");

        return 0;
    }

    // Create a copy before replacing the original command line
    char* replacement = malloc(strlen(previous) + 1);

    if (replacement == NULL) {
        perror("malloc");
        return 0;
    }

    strcpy(replacement, previous);

    free(*cmdline);

    *cmdline = replacement;

    return 1;
}



int main() {

    char* cmdline;
    char** arglist;

    int builtin_status;


    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {

        builtin_status = 0;


        // STEP 1: Expand !n before storing or tokenizing
        if (!expand_history(&cmdline)) {

            free(cmdline);

            continue;
        }


        // STEP 2: Store the actual command in history
        add_history(cmdline);


        // STEP 3: Tokenize the command
        arglist = tokenize(cmdline);


        if (arglist != NULL) {

            // STEP 4: Check built-in commands
            builtin_status = handle_builtin(arglist);


            // STEP 5: Execute external commands
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


        // STEP 6: Exit the shell if exit was entered
        if (builtin_status == 2) {

            break;
        }
    }


    printf("\nShell exited.\n");

    return 0;
}

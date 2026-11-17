#include "shell.h"

int main() {

    char* cmdline;
    char** arglist;
    int builtin_status;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
        builtin_status = 0;
        arglist = tokenize(cmdline);

        if (arglist != NULL) {

            // Check built-in commands before executing external commands
            builtin_status = handle_builtin(arglist);

            // Execute only if the command is not a built-in
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

        // Exit the shell after cleaning up allocated memory
        if (builtin_status == 2) {
            break;
        }
    }

    printf("\nShell exited.\n");

    return 0;
}

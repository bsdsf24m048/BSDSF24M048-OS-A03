#include "shell.h"

#include "shell.h"

int handle_builtin(char** arglist) {

    // EXIT: terminate the shell
    if (strcmp(arglist[0], "exit") == 0) {
        return 2;
    }

    // CD: change the current working directory
    if (strcmp(arglist[0], "cd") == 0) {

        char* directory;

        if (arglist[1] == NULL) {
            directory = getenv("HOME");

            if (directory == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 1;
            }
        }
        else if (arglist[2] != NULL) {
            fprintf(stderr, "cd: too many arguments\n");
            return 1;
        }
        else {
            directory = arglist[1];
        }

        if (chdir(directory) != 0) {
            perror("cd");
        }

        return 1;
    }

    // HELP: display available built-in commands
    if (strcmp(arglist[0], "help") == 0) {
        printf("\nBuilt-in Commands:\n");
        printf("  exit              - Exit the shell\n");
        printf("  cd <directory>    - Change current directory\n");
        printf("  help              - Display this help message\n");
        printf("  jobs              - Display background jobs status\n\n");

        return 1;
    }

    // JOBS: placeholder for future job control
    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    // Not a built-in command
    return 0;
}


// Execute external commands using fork() and execvp()
int execute(char* arglist[]) {

    int status;
    int cpid = fork();

    switch (cpid) {

        case -1:
            perror("fork failed");
            return -1;

        case 0:
            execvp(arglist[0], arglist);
            perror("Command not found");
            exit(EXIT_FAILURE);

        default:
            waitpid(cpid, &status, 0);
            return 0;
    }
}

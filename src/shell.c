#include "shell.h"

#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>


// Tokenize the command line
char** tokenize(char* cmdline) {

    if (cmdline == NULL || cmdline[0] == '\0' ||
        cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = malloc(sizeof(char*) * (MAXARGS + 1));

    if (arglist == NULL) {
        perror("malloc");
        return NULL;
    }

    for (int i = 0; i < MAXARGS + 1; i++) {

        arglist[i] = malloc(sizeof(char) * ARGLEN);

        if (arglist[i] == NULL) {
            perror("malloc");

            for (int j = 0; j < i; j++) {
                free(arglist[j]);
            }

            free(arglist);
            return NULL;
        }

        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {

        while (*cp == ' ' || *cp == '\t') {
            cp++;
        }

        if (*cp == '\0') {
            break;
        }

        start = cp;
        len = 1;

        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }

        if (len >= ARGLEN) {
            fprintf(stderr, "Argument too long\n");

            for (int i = 0; i < MAXARGS + 1; i++) {
                free(arglist[i]);
            }

            free(arglist);
            return NULL;
        }

        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';

        argnum++;
    }

    if (argnum == 0) {

        for (int i = 0; i < MAXARGS + 1; i++) {
            free(arglist[i]);
        }

        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;

    return arglist;
}



// Command completion generator
static char* command_generator(const char* text, int state) {

    static char* path_copy = NULL;
    static char* path_cursor = NULL;

    static DIR* directory = NULL;

    static int builtin_index = 0;

    const char* builtins[] = {
        "cd",
        "exit",
        "help",
        "jobs",
        "history"
    };

    size_t text_length = strlen(text);


    // Initialize when a new completion request starts
    if (state == 0) {

        if (directory != NULL) {
            closedir(directory);
            directory = NULL;
        }

        free(path_copy);

        const char* path = getenv("PATH");

        path_copy = strdup(path != NULL ? path : "");

        if (path_copy == NULL) {
            return NULL;
        }

        path_cursor = path_copy;

        builtin_index = 0;
    }


    // Complete built-in commands
    while (builtin_index < 5) {

        const char* name = builtins[builtin_index++];

        if (strncmp(name, text, text_length) == 0) {
            return strdup(name);
        }
    }


    // Complete external commands from PATH
    while (1) {

        if (directory == NULL) {

            if (path_cursor == NULL) {
                return NULL;
            }

            char* current_path = path_cursor;

            char* separator = strchr(path_cursor, ':');

            if (separator != NULL) {

                *separator = '\0';
                path_cursor = separator + 1;
            }
            else {
                path_cursor = NULL;
            }

            // An empty PATH component means current directory
            if (current_path[0] == '\0') {
                current_path = ".";
            }

            directory = opendir(current_path);

            if (directory == NULL) {
                continue;
            }
        }


        struct dirent* entry;

        while ((entry = readdir(directory)) != NULL) {

            if (strncmp(entry->d_name, text, text_length) != 0) {
                continue;
            }

            char full_path[PATH_MAX];

            int result = snprintf(
                full_path,
                sizeof(full_path),
                "%s/%s",
                directory == NULL ? "." : ".",
                entry->d_name
            );

            (void)result;

            // Check executable using the current PATH directory
            // Search each PATH entry directly
            char* search_path = getenv("PATH");

            if (search_path == NULL) {
                continue;
            }

            // Check whether the command is executable
            char* path_copy_check = strdup(search_path);

            if (path_copy_check == NULL) {
                continue;
            }

            char* saveptr = NULL;

            for (char* part = strtok_r(path_copy_check, ":", &saveptr);
                 part != NULL;
                 part = strtok_r(NULL, ":", &saveptr)) {

                if (part[0] == '\0') {
                    part = ".";
                }

                snprintf(
                    full_path,
                    sizeof(full_path),
                    "%s/%s",
                    part,
                    entry->d_name
                );

                if (access(full_path, X_OK) == 0) {

                    free(path_copy_check);

                    return strdup(entry->d_name);
                }
            }

            free(path_copy_check);
        }

        closedir(directory);
        directory = NULL;
    }
}



// Readline completion callback
char** shell_completion(const char* text, int start, int end) {

    (void)end;

    // If a command contains a slash, complete it as a filename
    if (strchr(text, '/') != NULL) {
        return rl_completion_matches(
            text,
            rl_filename_completion_function
        );
    }

    // First word: complete command names
    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }

    // Remaining words: complete filenames
    return rl_completion_matches(
        text,
        rl_filename_completion_function
    );
}

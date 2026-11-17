
#include "shell.h"

#include <fcntl.h>
#include <ctype.h>

#define MAX_COMMANDS 10
#define MAX_TOKENS 256

// Built-in commands
int handle_builtin(char** arglist) {

    // EXIT
    if (strcmp(arglist[0], "exit") == 0) {
        return 2;
    }

    // CD
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

    // HELP
    if (strcmp(arglist[0], "help") == 0) {

        printf("\nBuilt-in Commands:\n");
        printf("  exit              - Exit the shell\n");
        printf("  cd <directory>    - Change current directory\n");
        printf("  help              - Display this help message\n");
        printf("  jobs              - Display background jobs status\n");
        printf("  history           - Display command history\n\n");

        return 1;
    }

    // JOBS
    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    // HISTORY
    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    return 0;
}


// Display GNU Readline history
void print_history(void) {

    HIST_ENTRY** entries = history_list();

    if (entries == NULL) {
        return;
    }

    for (int i = 0; entries[i] != NULL; i++) {
        printf("%d  %s\n", history_base + i, entries[i]->line);
    }
}


// Data structure for one command
typedef struct {
    char* args[MAXARGS + 1];
    char* input_file;
    char* output_file;
    int argc;
} Command;


// Split a line into words and operators.
// Supports single and double quoted arguments.
static int parse_tokens(char* line, char* tokens[]) {

    int count = 0;
    char* p = line;

    while (*p != '\0') {

        while (isspace((unsigned char)*p)) {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        if (count >= MAX_TOKENS) {
            fprintf(stderr, "Too many tokens\n");
            return -1;
        }

        // Operators are separate tokens, even without spaces
        if (*p == '|' || *p == '<' || *p == '>') {

            char operator_token[2] = {*p, '\0'};
            tokens[count] = strdup(operator_token);

            if (tokens[count] == NULL) {
                perror("strdup");
                return -1;
            }

            count++;
            p++;
            continue;
        }

        // Read a normal or quoted argument
        char buffer[MAX_LEN];
        int length = 0;
        char quote = '\0';

        if (*p == '\'' || *p == '"') {
            quote = *p;
            p++;
        }

        while (*p != '\0') {

            if (quote != '\0') {

                if (*p == quote) {
                    p++;
                    quote = '\0';
                    break;
                }
            }
            else {

                if (isspace((unsigned char)*p) ||
                    *p == '|' || *p == '<' || *p == '>') {
                    break;
                }
            }

            if (length >= MAX_LEN - 1) {
                fprintf(stderr, "Argument too long\n");
                return -1;
            }

            buffer[length++] = *p;
            p++;
        }

        if (quote != '\0') {
            fprintf(stderr, "Syntax error: unmatched quote\n");
            return -1;
        }

        buffer[length] = '\0';

        if (length == 0) {
            continue;
        }

        tokens[count] = strdup(buffer);

        if (tokens[count] == NULL) {
            perror("strdup");
            return -1;
        }

        count++;
    }

    return count;
}


// Free token memory
static void free_tokens(char* tokens[], int count) {

    for (int i = 0; i < count; i++) {
        free(tokens[i]);
    }
}


// Parse tokens into pipeline commands
static int parse_commands(
    char* tokens[],
    int token_count,
    Command commands[],
    int* command_count
) {

    int current = 0;

    *command_count = 1;

    memset(commands, 0, sizeof(Command) * MAX_COMMANDS);

    for (int i = 0; i < token_count; i++) {

        // Pipe operator
        if (strcmp(tokens[i], "|") == 0) {

            if (commands[current].argc == 0) {
                fprintf(stderr, "Syntax error near pipe\n");
                return -1;
            }

            if (current + 1 >= MAX_COMMANDS) {
                fprintf(stderr, "Too many commands in pipeline\n");
                return -1;
            }

            current++;
            (*command_count)++;
            continue;
        }

        // Input redirection
        if (strcmp(tokens[i], "<") == 0) {

            if (i + 1 >= token_count ||
                strcmp(tokens[i + 1], "|") == 0 ||
                strcmp(tokens[i + 1], "<") == 0 ||
                strcmp(tokens[i + 1], ">") == 0 ||
                commands[current].input_file != NULL) {

                fprintf(stderr, "Syntax error with input redirection\n");
                return -1;
            }

            commands[current].input_file = tokens[++i];
            continue;
        }

        // Output redirection
        if (strcmp(tokens[i], ">") == 0) {

            if (i + 1 >= token_count ||
                strcmp(tokens[i + 1], "|") == 0 ||
                strcmp(tokens[i + 1], "<") == 0 ||
                strcmp(tokens[i + 1], ">") == 0 ||
                commands[current].output_file != NULL) {

                fprintf(stderr, "Syntax error with output redirection\n");
                return -1;
            }

            commands[current].output_file = tokens[++i];
            continue;
        }

        // Normal command argument
        if (commands[current].argc >= MAXARGS) {
            fprintf(stderr, "Too many arguments\n");
            return -1;
        }

        commands[current].args[commands[current].argc++] = tokens[i];

        commands[current].args[commands[current].argc] = NULL;
    }

    if (commands[current].argc == 0) {
        fprintf(stderr, "Syntax error: missing command\n");
        return -1;
    }

    return 0;
}


// Execute a pipeline with optional redirection
static int execute_pipeline(Command commands[], int command_count) {

    int pipes[MAX_COMMANDS - 1][2];
    pid_t pids[MAX_COMMANDS];

    // Create pipes
    for (int i = 0; i < command_count - 1; i++) {

        if (pipe(pipes[i]) == -1) {
            perror("pipe");

            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            return -1;
        }
    }

    // Create one process per command
    for (int i = 0; i < command_count; i++) {

        pid_t pid = fork();

        if (pid == -1) {

            perror("fork");

            // Close all pipe ends
            for (int j = 0; j < command_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Wait for children already started
            for (int j = 0; j < i; j++) {
                waitpid(pids[j], NULL, 0);
            }

            return -1;
        }

        if (pid == 0) {

            // Connect input from previous pipe
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                    perror("dup2 input pipe");
                    _exit(EXIT_FAILURE);
                }
            }

            // Connect output to next pipe
            if (i < command_count - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2 output pipe");
                    _exit(EXIT_FAILURE);
                }
            }

            // Close all pipe descriptors in child
            for (int j = 0; j < command_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Input redirection overrides pipe input
            if (commands[i].input_file != NULL) {

                int fd = open(commands[i].input_file, O_RDONLY);

                if (fd == -1) {
                    perror(commands[i].input_file);
                    _exit(EXIT_FAILURE);
                }

                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("dup2 input");
                    close(fd);
                    _exit(EXIT_FAILURE);
                }

                close(fd);
            }

            // Output redirection overrides pipe output
            if (commands[i].output_file != NULL) {

                int fd = open(
                    commands[i].output_file,
                    O_WRONLY | O_CREAT | O_TRUNC,
                    0644
                );

                if (fd == -1) {
                    perror(commands[i].output_file);
                    _exit(EXIT_FAILURE);
                }

                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("dup2 output");
                    close(fd);
                    _exit(EXIT_FAILURE);
                }

                close(fd);
            }

            execvp(commands[i].args[0], commands[i].args);

            perror(commands[i].args[0]);
            _exit(EXIT_FAILURE);
        }

        pids[i] = pid;
    }

    // Parent closes all pipe descriptors
    for (int i = 0; i < command_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all child processes
    int status = 0;

    for (int i = 0; i < command_count; i++) {
        waitpid(pids[i], &status, 0);
    }

    return 0;
}


// Main entry point for command execution
int execute_line(char* line) {

    char* tokens[MAX_TOKENS];

    int token_count = parse_tokens(line, tokens);

    if (token_count <= 0) {
        return 0;
    }

    Command commands[MAX_COMMANDS];

    int command_count = 0;

    if (parse_commands(
            tokens,
            token_count,
            commands,
            &command_count
        ) != 0) {

        free_tokens(tokens, token_count);
        return 0;
    }

    int result = 0;

    // A built-in runs in the shell process only when it is a
    // standalone command without redirection.
    if (command_count == 1 &&
        commands[0].input_file == NULL &&
        commands[0].output_file == NULL) {

        result = handle_builtin(commands[0].args);

        if (result != 0) {
            free_tokens(tokens, token_count);
            return result;
        }
    }

    execute_pipeline(commands, command_count);

    free_tokens(tokens, token_count);

    return 0;
}

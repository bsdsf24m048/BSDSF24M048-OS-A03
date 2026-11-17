#include "shell.h"

#include <fcntl.h>
#include <ctype.h>

#define MAX_COMMANDS 10
#define MAX_TOKENS 256
#define MAX_JOBS 128


// Structure for one command in a pipeline
typedef struct {
    char* args[MAXARGS + 1];
    char* input_file;
    char* output_file;
    int argc;
} Command;


// Structure for one background job
typedef struct {
    pid_t pid;
    char command[MAX_LEN];
} BackgroundJob;


// List of active background jobs
static BackgroundJob background_jobs[MAX_JOBS];
static int background_job_count = 0;


// Remove a completed process from the active jobs list
static void remove_background_job(pid_t pid) {

    for (int i = 0; i < background_job_count; i++) {

        if (background_jobs[i].pid == pid) {

            for (int j = i; j < background_job_count - 1; j++) {
                background_jobs[j] = background_jobs[j + 1];
            }

            background_job_count--;
            return;
        }
    }
}


// Add a process to the active jobs list
static int add_background_job(pid_t pid, const char* command) {

    if (background_job_count >= MAX_JOBS) {
        fprintf(stderr, "Maximum background jobs reached.\n");
        return -1;
    }

    background_jobs[background_job_count].pid = pid;

    snprintf(
        background_jobs[background_job_count].command,
        MAX_LEN,
        "%s",
        command
    );

    background_job_count++;
    return 0;
}


// Display active background jobs
static void print_background_jobs(void) {

    if (background_job_count == 0) {
        printf("No background jobs.\n");
        return;
    }

    for (int i = 0; i < background_job_count; i++) {

        printf(
            "[%d] PID %ld %s\n",
            i + 1,
            (long)background_jobs[i].pid,
            background_jobs[i].command
        );
    }
}


// Reap completed background processes without blocking
void reap_background_jobs(void) {

    int status;
    pid_t pid;

    while (1) {

        pid = waitpid(-1, &status, WNOHANG);

        if (pid > 0) {

            remove_background_job(pid);

            printf(
                "\n[completed] PID %ld\n",
                (long)pid
            );

            continue;
        }

        if (pid == -1 && errno == EINTR) {
            continue;
        }

        break;
    }
}


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
        printf("  jobs              - Display active background jobs\n");
        printf("  history           - Display command history\n\n");

        return 1;
    }

    // JOBS
    if (strcmp(arglist[0], "jobs") == 0) {

        // Refresh the list before displaying it
        reap_background_jobs();
        print_background_jobs();

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
        if (*p == '|' || *p == '<' || *p == '>' || *p == '&') {

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
                    *p == '|' || *p == '<' || *p == '>' || *p == '&') {
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
                strcmp(tokens[i + 1], "&") == 0 ||
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
                strcmp(tokens[i + 1], "&") == 0 ||
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


// Execute a pipeline with optional redirection and background mode
static int execute_pipeline(
    Command commands[],
    int command_count,
    int background,
    const char* job_command
) {

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
                while (waitpid(pids[j], NULL, 0) == -1 && errno == EINTR) {
                    // Retry if interrupted
                }
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

    // Background execution: register children and return immediately
    if (background) {

        for (int i = 0; i < command_count; i++) {

            if (add_background_job(pids[i], job_command) == 0) {

                printf("[background] PID %ld\n", (long)pids[i]);
            }
            else {

                // If the job list is full, wait for this child
                while (waitpid(pids[i], NULL, 0) == -1 && errno == EINTR) {
                    // Retry if interrupted
                }
            }
        }

        return 0;
    }

    // Foreground execution: wait for every child
    for (int i = 0; i < command_count; i++) {

        while (waitpid(pids[i], NULL, 0) == -1) {

            if (errno == EINTR) {
                continue;
            }

            perror("waitpid");
            break;
        }
    }

    return 0;
}


// Execute one command, including its pipeline and redirection
static int execute_single_command(char* line) {

    char* tokens[MAX_TOKENS];

    int token_count = parse_tokens(line, tokens);

    if (token_count <= 0) {
        return 0;
    }

    // Detect background execution (&)
    int background = 0;

    for (int i = 0; i < token_count; i++) {

        if (strcmp(tokens[i], "&") == 0) {

            // Ampersand is allowed only at the end
            if (i != token_count - 1) {

                fprintf(stderr, "Syntax error: & must be at the end\n");
                free_tokens(tokens, token_count);
                return 0;
            }

            background = 1;

            free(tokens[i]);
            token_count--;

            break;
        }
    }

    if (token_count == 0) {
        fprintf(stderr, "Syntax error: missing command\n");
        return 0;
    }

    Command commands[MAX_COMMANDS];
    int command_count = 0;

    // Parse commands into pipeline segments
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

    // Built-ins execute in the parent shell when run in foreground
    if (!background &&
        command_count == 1 &&
        commands[0].input_file == NULL &&
        commands[0].output_file == NULL) {

        result = handle_builtin(commands[0].args);

        if (result != 0) {
            free_tokens(tokens, token_count);
            return result;
        }
    }

    // Execute foreground or background pipeline
    execute_pipeline(
        commands,
        command_count,
        background,
        line
    );

    free_tokens(tokens, token_count);
    return 0;
}


// Main entry point: split and execute semicolon-separated commands
int execute_line(char* line) {

    char* start = line;
    char quote = '\0';
    int result = 0;

    for (char* p = line; ; p++) {

        // A semicolon separates commands only outside quotes
        if (*p == '\0' || (*p == ';' && quote == '\0')) {

            char delimiter = *p;
            *p = '\0';

            // Remove leading whitespace
            while (isspace((unsigned char)*start)) {
                start++;
            }

            // Remove trailing whitespace
            char* end = start + strlen(start);

            while (end > start && isspace((unsigned char)end[-1])) {
                *--end = '\0';
            }

            // Execute non-empty command
            if (*start != '\0') {

                result = execute_single_command(start);

                // Stop processing the line if exit was entered
                if (result == 2) {
                    return 2;
                }
            }

            if (delimiter == '\0') {
                break;
            }

            start = p + 1;
            continue;
        }

        // Track quoted sections so semicolons inside quotes are ignored
        if (quote != '\0') {

            if (*p == quote) {
                quote = '\0';
            }
        }
        else if (*p == '\'' || *p == '"') {

            quote = *p;
        }
    }

    return 0;
}

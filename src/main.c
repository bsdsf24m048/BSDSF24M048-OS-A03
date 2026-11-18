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


// Free an array of lines
static void free_lines(char** lines, int count) {

    for (int i = 0; i < count; i++) {
        free(lines[i]);
    }

    free(lines);
}


// Append one line to a dynamic array of lines
static int push_line(char*** lines, int* count, int* capacity, const char* text) {

    if (*count >= *capacity) {

        int new_capacity = (*capacity == 0) ? 16 : *capacity * 2;
        char** resized = realloc(*lines, sizeof(char*) * new_capacity);

        if (resized == NULL) {
            perror("realloc");
            return 0;
        }

        *lines = resized;
        *capacity = new_capacity;
    }

    char* copy = strdup(text);

    if (copy == NULL) {
        perror("strdup");
        return 0;
    }

    (*lines)[(*count)++] = copy;
    return 1;
}


// Read a multi-line if ... fi block.
// The first line (starting with "if") has already been read.
// Continuation lines are read until the matching "fi" is found.
// Returns 1 on success, 0 on error or end of input.
static int read_if_block(const char* first_line, char*** lines, int* count) {

    int capacity = 0;
    int depth = 1;

    *lines = NULL;
    *count = 0;

    if (!push_line(lines, count, &capacity, first_line)) {
        free_lines(*lines, *count);
        return 0;
    }

    while (depth > 0) {

        char* next = readline(CONT_PROMPT);

        // Ctrl+D before the block was closed
        if (next == NULL) {
            fprintf(stderr, "\nSyntax error: unexpected end of input, expected 'fi'\n");
            free_lines(*lines, *count);
            return 0;
        }

        if (next[0] != '\0') {
            add_history(next);
        }

        if (!push_line(lines, count, &capacity, next)) {
            free(next);
            free_lines(*lines, *count);
            return 0;
        }

        // Track nested if blocks so the correct fi ends the block
        if (is_if_line(next)) {
            depth++;
        }
        else if (is_fi_line(next)) {
            depth--;
        }

        free(next);
    }

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

        if (is_if_line(cmdline)) {

            // Multi-line if-then-else-fi block
            char** lines = NULL;
            int line_count = 0;

            if (read_if_block(cmdline, &lines, &line_count)) {
                builtin_status = execute_block(lines, line_count);
                free_lines(lines, line_count);
            }
        }
        else {

            // Parse and execute the full command line
            // (execute_block also rejects stray then/else/fi keywords)
            builtin_status = execute_block(&cmdline, 1);
        }

        free(cmdline);

        if (builtin_status == 2) {
            break;
        }
    }

    printf("\nShell exited.\n");

    return 0;
}
#include "shell.h"

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace
        
        if (*cp == '\0') break; // Line was only whitespace

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for(int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}


// Command history storage
static char* command_history[HISTORY_SIZE] = {NULL};
static unsigned long history_count = 0;


// Add a command to history
void add_history(const char* command) {

    // Ignore empty or whitespace-only commands
    if (command == NULL || command[0] == '\0') {
        return;
    }

    int only_spaces = 1;

    for (int i = 0; command[i] != '\0'; i++) {
        if (command[i] != ' ' && command[i] != '\t') {
            only_spaces = 0;
            break;
        }
    }

    if (only_spaces) {
        return;
    }

    // Allocate memory for the command
    char* copy = malloc(strlen(command) + 1);

    if (copy == NULL) {
        perror("history");
        return;
    }

    strcpy(copy, command);

    // Calculate the circular array position
    unsigned long index = history_count % HISTORY_SIZE;

    // Free the oldest command if this position is occupied
    free(command_history[index]);

    command_history[index] = copy;

    history_count++;
}


// Display all stored commands
void print_history(void) {

    unsigned long first;

    if (history_count > HISTORY_SIZE) {
        first = history_count - HISTORY_SIZE + 1;
    }
    else {
        first = 1;
    }

    printf("\n");

    for (unsigned long i = first; i <= history_count; i++) {

        unsigned long index = (i - 1) % HISTORY_SIZE;

        printf("%lu  %s\n", i, command_history[index]);
    }

    printf("\n");
}


// Retrieve a command using its history number
const char* get_history_command(unsigned long number) {

    // Check whether the requested number exists
    if (number == 0 || number > history_count) {
        return NULL;
    }

    // Find the oldest command still available
    unsigned long first;

    if (history_count > HISTORY_SIZE) {
        first = history_count - HISTORY_SIZE + 1;
    }
    else {
        first = 1;
    }

    // Requested command has already been overwritten
    if (number < first) {
        return NULL;
    }

    unsigned long index = (number - 1) % HISTORY_SIZE;

    return command_history[index];
}

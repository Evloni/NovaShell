/*
 * NovaShell - GPLv3
 * Copyright (C) 2026 Evloni
 *
 * This file is part of NovaShell.
 * See LICENSE in the project root for full license information.
 */

#include "libs/utils.h"

void banner(void) {
    printf(NSH_ACCENT "nsh — Nova Shell\n" NSH_RESET);
    printf(NSH_INFO "nsh "
                    "v1.0.0\n" NSH_RESET);
    printf(NSH_INFO "Type `help` to show available commands!\n" NSH_RESET);

    printf("\n");
}

void completion(const char *buff, linenoiseCompletions *lc) {
    const char *commands[] = {"exit", "cd", "echo", "export", "clear", "help", "pwd", "dir"};
    int numCommands = sizeof(commands) / sizeof(commands[0]);

    const char *p = buff;
    while (*p == ' ') {
        p++;
    }

    const char *space = strchr(p, ' ');
    if (space == NULL) {
        for (int i = 0; i < numCommands; i++) {
            if (strncmp(p, commands[i], strlen(p)) == 0) {
                linenoiseAddCompletion(lc, commands[i]);
            }
        }
    }
}

// Parse command line into tokens (command + arguments)
// Returns number of tokens, or -1 on error
int parse_command(char *line, char **argv, int max_args) {
    int argc = 0;
    char *token;
    char *saveptr;

    // Skip leading whitespace
    while (*line == ' ' || *line == '\t') {
        line++;
    }

    // If empty line, return 0
    if (*line == '\0') {
        return 0;
    }

    // Tokenize the line
    token = strtok_r(line, " \t", &saveptr);
    while (token != NULL && argc < max_args - 1) {
        argv[argc++] = token;
        token = strtok_r(NULL, " \t", &saveptr);
    }
    argv[argc] = NULL; // Null-terminate the array

    return argc;
}
// Execute external program
void execute_external(char **argv) {
    pid_t pid = fork();

    if (pid == 0) {
        // Child process: execute the command
        execvp(argv[0], argv);
        // If execvp returns, there was an error
        perror(argv[0]);
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Fork failed
        perror("fork");
    } else {
        // Parent process: wait for child to complete
        int status;
        waitpid(pid, &status, 0);
    }
}

/*
 * NovaShell - GPLv3
 * Copyright (C) 2026 Evloni
 *
 * This file is part of NovaShell.
 * See LICENSE in the project root for full license information.
 */

#include "libs/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

// Execute scripts with bash
int execute_script(const char *script_path, char **args) {
    pid_t pid = fork();

    if (pid == 0) {
        int arg_count = 0;
        if (args) {
            while (args[arg_count]) {
                arg_count++;
            }
        }

        char *bash_args[arg_count + 3];
        bash_args[0] = "bash";
        bash_args[1] = (char *)script_path;

        for (int i = 0; i < arg_count; i++) {
            bash_args[i + 2] = args[i];
        }
        bash_args[arg_count + 2] = NULL;

        execvp("bash", bash_args);
        // If execvp fails
        execv("/bin/bash", bash_args);
        perror("exec failed");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork");
        return -1;
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
    }
    return -1;
}

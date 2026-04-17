/*
 * NovaShell - GPLv3
 * Copyright (C) 2026 Evloni
 *
 * This file is part of NovaShell.
 * See LICENSE in the project root for full license information.
 */

#include "libs/utils.h"
#include "libs/linenoise.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define NSH_COMP_LINE_MAX 4096

static void nsh_expand_tilde(const char *in, char *out, size_t outsz) {
    const char *home = getenv("HOME");
    if (home != NULL && in[0] == '~' && (in[1] == '/' || in[1] == '\0')) {
        if (in[1] == '\0') {
            snprintf(out, outsz, "%s", home);
        } else {
            snprintf(out, outsz, "%s%s", home, in + 1);
        }
    } else {
        snprintf(out, outsz, "%s", in);
    }
}

/* exp_partial: path fragment with tilde already expanded. */
static int nsh_filepath_split(const char *exp_partial, char *dirbuf, size_t dirsz, char *pfxbuf, size_t pfxsz) {
    const char *slash = strrchr(exp_partial, '/');

    if (slash == NULL) {
        snprintf(dirbuf, dirsz, ".");
        snprintf(pfxbuf, pfxsz, "%s", exp_partial);
        return 0;
    }
    if (slash == exp_partial) {
        snprintf(dirbuf, dirsz, "/");
        snprintf(pfxbuf, pfxsz, "%s", slash + 1);
        return 0;
    }
    size_t dlen = (size_t)(slash - exp_partial);
    if (dlen + 1 >= dirsz) {
        return -1;
    }
    memcpy(dirbuf, exp_partial, dlen);
    dirbuf[dlen] = '\0';
    snprintf(pfxbuf, pfxsz, "%s", slash + 1);
    return 0;
}

static void nsh_join_path(char *out, size_t outsz, const char *dir, const char *name) {
    if (strcmp(dir, "/") == 0) {
        snprintf(out, outsz, "/%s", name);
    } else {
        snprintf(out, outsz, "%s/%s", dir, name);
    }
}

static void nsh_add_path_completions(const char *buff, size_t prefix_len, const char *dir_scan, const char *namepfx,
                                     linenoiseCompletions *lc) {
    DIR *dp = opendir(dir_scan);
    if (dp == NULL) {
        return;
    }

    size_t pfxlen = strlen(namepfx);
    struct dirent *dent;

    while ((dent = readdir(dp)) != NULL) {
        if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
            continue;
        }
        if (pfxlen > 0) {
            if (strncmp(dent->d_name, namepfx, pfxlen) != 0) {
                continue;
            }
        } else if (dent->d_name[0] == '.') {
            continue;
        }

        char full[PATH_MAX];
        nsh_join_path(full, sizeof(full), dir_scan, dent->d_name);

        struct stat st;
        int is_dir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));

        /* +2 so "%s/" cannot truncate when full uses almost all of PATH_MAX */
        char tail[PATH_MAX + 2];
        if (is_dir) {
            snprintf(tail, sizeof(tail), "%s/", full);
        } else {
            snprintf(tail, sizeof(tail), "%s", full);
        }

        char line[NSH_COMP_LINE_MAX];
        int n = snprintf(line, sizeof(line), "%.*s%s", (int)prefix_len, buff, tail);
        if (n < 0 || (size_t)n >= sizeof(line)) {
            continue;
        }
        linenoiseAddCompletion(lc, line);
    }
    closedir(dp);
}

void banner(void) {
    printf(NSH_FG);
    fflush(stdout);
    printf("####################\n");
    printf("# nsh — Nova Shell #\n");
    printf("#     v1.1.1       #\n");
    printf("####################\n");
    printf("Type `help` to show available commands!\n");
    printf("\n");
}

void completion(const char *buff, linenoiseCompletions *lc) {
    /* Built-ins handled in main.c (keep in sync with strcmp argv[0] branches). */
    const char *commands[] = {
        "cd",
        "clear",
        "echo",
        "exit",
        "export",
        "help",
        "pwd",
        "ls",
    };
    int numCommands = (int)(sizeof(commands) / sizeof(commands[0]));

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
        return;
    }

    const char *last_sp = strrchr(buff, ' ');
    if (last_sp == NULL) {
        return;
    }

    const char *partial = last_sp + 1;
    while (*partial == ' ') {
        partial++;
    }

    size_t prefix_len = (size_t)(partial - buff);
    if (partial[0] == '\0') {
        nsh_add_path_completions(buff, prefix_len, ".", "", lc);
        return;
    }

    char expanded[PATH_MAX];
    nsh_expand_tilde(partial, expanded, sizeof(expanded));

    char dirbuf[PATH_MAX];
    char pfxbuf[PATH_MAX];
    if (nsh_filepath_split(expanded, dirbuf, sizeof(dirbuf), pfxbuf, sizeof(pfxbuf)) != 0) {
        return;
    }

    nsh_add_path_completions(buff, prefix_len, dirbuf, pfxbuf, lc);
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

char *expand_variable(const char *token) {
    if (token[0] == '$') {
        const char *varName = token + 1;
        char *value = getenv(varName);
        if (value != NULL) {
            return strdup(value);
        } else {
            return strdup("");
        }
    }
    return strdup(token);
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

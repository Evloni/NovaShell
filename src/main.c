/*
 * NovaShell - GPLv3
 * Copyright (C) 2026 Evloni
 *
 * This file is part of NovaShell.
 * See LICENSE in the project root for full license information.
 */

#include "libs/linenoise.h"
#include "libs/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

static void build_prompt(char *prompt_buff, size_t prompt_size, char *cwd_buff, size_t cwd_size) {
    const char *home = getenv("HOME");

    if (getcwd(cwd_buff, cwd_size) == NULL) {
        perror("getcwd");
        snprintf(prompt_buff, prompt_size, "nsh $ ");
        return;
    }

    if (home != NULL && home[0] != '\0') {
        size_t n = strlen(home);
        if (strncmp(cwd_buff, home, n) == 0 && (cwd_buff[n] == '\0' || cwd_buff[n] == '/')) {
            if (cwd_buff[n] == '\0') {
                snprintf(prompt_buff, prompt_size,
                         "\001" NSH_ACCENT "\002"
                         "~: nsh $"
                         "\001" NSH_RESET "\002"
                         " ");
            } else {
                snprintf(prompt_buff, prompt_size,
                         "\001" NSH_ACCENT "\002"
                         "~%s: nsh $"
                         "\001" NSH_RESET "\002"
                         " ",
                         cwd_buff + n);
            }
            return;
        }
    }

    snprintf(prompt_buff, prompt_size,
             "\001" NSH_ACCENT "\002"
             "%s: nsh $"
             "\001" NSH_RESET "\002"
             " ",
             cwd_buff);
}

/* Build path to persistent linenoise history (TMPDIR or /tmp). Returns 0 on success. */
static int build_history_path(char *buf, size_t buflen) {
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = "/tmp";
    }
    size_t n = strlen(tmpdir);
    const char *sep = (n > 0 && tmpdir[n - 1] == '/') ? "" : "/";
    if (snprintf(buf, buflen, "%s%shistory.txt", tmpdir, sep) >= (int)buflen) {
        return -1;
    }
    return 0;
}

static int file_starts_with_shebang(const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }
    char first_line[3];
    int ok = fgets(first_line, sizeof(first_line), file) != NULL && strncmp(first_line, "#!", 2) == 0;
    fclose(file);
    return ok;
}

/* True if path should be executed via execute_script (.sh or shebang file). */
static int path_looks_like_script(const char *path) {
    char *ext = strrchr(path, '.');
    if (ext != NULL && strcmp(ext, ".sh") == 0) {
        return 1;
    }
    if (access(path, F_OK) == 0 && file_starts_with_shebang(path)) {
        return 1;
    }
    return 0;
}

int main(int argc_main, char **argv_main) {
    char cwd_buff[PATH_MAX];
    char prompt_buff[PATH_MAX * 2];
    char history_path[PATH_MAX];
    char *argv[64];
    char *line;
    char *equals_pos;
    char *var_name;
    char *var_value;
    char *prompt = NULL;

    int argc;
    int handled;

    /* Non-interactive: first argv is program or script to run */
    if (argc_main > 1) {
        char *script_path = argv_main[1];
        char **script_args = (argc_main > 2) ? &argv_main[2] : NULL;

        if (path_looks_like_script(script_path)) {
            int exit_status = execute_script(script_path, script_args);
            exit(exit_status);
        } else {
            // Execute as external program with variable expansion
            int arg_count = argc_main - 1;
            char *expanded_main[arg_count + 1];
            for (int i = 0; i < arg_count; i++) {
                expanded_main[i] = expand_variable(argv_main[i + 1]);
            }
            expanded_main[arg_count] = NULL;

            execute_external(expanded_main);

            for (int i = 0; i < arg_count; i++) {
                free(expanded_main[i]);
            }
            exit(EXIT_SUCCESS);
        }
    }

    banner();

    if (build_history_path(history_path, sizeof(history_path)) == 0) {
        linenoiseHistoryLoad(history_path);
    }
    linenoiseSetCompletionCallback(completion);

    while (1) {
        build_prompt(prompt_buff, sizeof(prompt_buff), cwd_buff, sizeof(cwd_buff));
        prompt = prompt_buff;
        line = linenoise(prompt);
        if (line == NULL) {
            printf(NSH_RESET);
            break;
        }

        if (line[0] == '\0') {
            free(line);
            continue;
        }

        /* parse_command() uses strtok_r and mutates the buffer; keep a copy for
         * history so the full line (with args) is saved. */
        char *history_line = strdup(line);
        if (history_line == NULL) {
            perror("strdup");
            free(line);
            continue;
        }

        // Parse the command line
        argc = parse_command(line, argv, 64);
        if (argc == 0) {
            free(history_line);
            free(line);
            continue;
        }

        handled = 0;

        if (strcmp(argv[0], "exit") == 0) {
            if (history_line[0] != '\0') {
                linenoiseHistoryAdd(history_line);
            }
            if (build_history_path(history_path, sizeof(history_path)) == 0) {
                linenoiseHistorySave(history_path);
            }
            free(history_line);
            free(line);
            exit(EXIT_SUCCESS);
        } else if (strcmp(argv[0], "pwd") == 0) {
            printf(NSH_FG "%s\n" NSH_RESET, cwd_buff);
            fflush(stdout);
            handled = 1;
        } else if (strcmp(argv[0], "cd") == 0) {
            if (argc > 1) {
                if (chdir(argv[1]) != 0) {
                    perror(NSH_ERR "cd" NSH_RESET);
                } else {
                    printf(NSH_OK "Changed directory to: " NSH_FG "%s\n" NSH_RESET, argv[1]);
                    fflush(stdout);
                }
            } else {
                // cd with no arguments - go to home directory
                const char *home = getenv("HOME");
                if (home != NULL) {
                    chdir(home);
                    printf(NSH_OK "Changed directory to: " NSH_FG "%s\n" NSH_RESET, home);
                    fflush(stdout);
                } else {
                    fprintf(stderr, NSH_ERR "cd: HOME not set\n" NSH_RESET);
                }
            }
            handled = 1;
        } else if (strcmp(argv[0], "export") == 0) {
            // Handle export command
            if (argc == 1) {
                // List all environment variables
                for (char **env = environ; *env != NULL; env++) {
                    printf(NSH_FG "declare -x %s\n" NSH_RESET, *env);
                }
                fflush(stdout);
            } else {
                var_name = argv[1];
                // Find '=' to separate variable name and value
                equals_pos = strchr(var_name, '=');

                if (equals_pos != NULL) {
                    // export VAR=value
                    *equals_pos = '\0'; // Temporarily null-terminate at '='
                    var_value = equals_pos + 1;

                    if (setenv(var_name, var_value, 1) != 0) {
                        perror(NSH_ERR "export" NSH_RESET);
                    } else {
                        printf(NSH_OK "Exported: " NSH_ACCENT "%s" NSH_FG "=%s\n" NSH_RESET,
                               var_name, var_value);
                        fflush(stdout);
                    }

                    *equals_pos = '='; // Restore '=' for proper cleanup
                } else {
                    // export VAR (export existing variable)
                    // Check if variable exists
                    if (getenv(var_name) != NULL) {
                        // Variable exists, it's already in the environment
                        printf(NSH_OK "Exported: " NSH_ACCENT "%s\n" NSH_RESET, var_name);
                        fflush(stdout);
                    } else {
                        // Variable doesn't exist, set it to empty string
                        if (setenv(var_name, "", 1) != 0) {
                            perror(NSH_ERR "export" NSH_RESET);
                        } else {
                            printf(NSH_OK "Exported: " NSH_ACCENT "%s" NSH_FG "=\n" NSH_RESET,
                                   var_name);
                            fflush(stdout);
                        }
                    }
                }
            }
            handled = 1;
        } else if (strcmp(argv[0], "echo") == 0) {
            // Reset colors so echo output uses default terminal colors
            printf(NSH_RESET);
            fflush(stdout);
            // Handle echo command with variable expansion
            if (argc == 1) {
                // Just "echo" - print newline
                printf("\n");
            } else {
                // Echo all arguments (skip argv[0] which is "echo")
                for (int i = 1; i < argc; i++) {
                    char *current = argv[i];

                    // Process the string and expand variables
                    while (*current != '\0') {
                        if (*current == '$') {
                            char *var_start = current + 1;
                            char var_name_buf[256];
                            char *expansion;
                            char *var_end;

                            // Handle ${VAR} format
                            if (*var_start == '{') {
                                var_start++;
                                var_end = strchr(var_start, '}');
                                if (var_end == NULL) {
                                    // Malformed ${VAR, just print the $
                                    putchar('$');
                                    current++;
                                    continue;
                                }
                                size_t var_len = var_end - var_start;
                                if (var_len >= sizeof(var_name_buf)) {
                                    var_len = sizeof(var_name_buf) - 1;
                                }
                                strncpy(var_name_buf, var_start, var_len);
                                var_name_buf[var_len] = '\0';
                                current = var_end + 1;
                            } else {
                                // Handle $VAR format
                                var_end = var_start;
                                // Variable name can contain letters, numbers,
                                // and underscore
                                while ((*var_end >= 'a' && *var_end <= 'z') ||
                                       (*var_end >= 'A' && *var_end <= 'Z') ||
                                       (*var_end >= '0' && *var_end <= '9') || *var_end == '_') {
                                    var_end++;
                                }
                                size_t var_len = var_end - var_start;
                                if (var_len >= sizeof(var_name_buf)) {
                                    var_len = sizeof(var_name_buf) - 1;
                                }
                                if (var_len == 0) {
                                    // Just $, print it
                                    putchar('$');
                                    current++;
                                    continue;
                                }
                                strncpy(var_name_buf, var_start, var_len);
                                var_name_buf[var_len] = '\0';
                                current = var_end;
                            }

                            expansion = getenv(var_name_buf);
                            if (expansion != NULL) {
                                printf("%s", expansion);
                            }
                            // If variable doesn't exist, print nothing (standard
                            // shell behavior)
                        } else {
                            // Regular character, print it
                            putchar(*current);
                            current++;
                        }
                    }
                    // Print space between arguments (except after last one)
                    if (i < argc - 1) {
                        putchar(' ');
                    }
                }
                printf("\n");
            }
            // Reset colors after echo output, then restore prompt color
            printf(NSH_RESET NSH_ACCENT);
            fflush(stdout);
            handled = 1;
        } else if (strcmp(argv[0], "clear") == 0) {
            linenoiseClearScreen();
            banner();
            handled = 1;
        } else if (strcmp(argv[0], "help") == 0) {
            printf(NSH_ACCENT "  exit" NSH_RESET NSH_FG
                              "                    Exit the shell\n" NSH_RESET);
            printf(NSH_ACCENT "  pwd" NSH_RESET NSH_FG "                     Print current working "
                              "directory\n" NSH_RESET);
            printf(NSH_ACCENT "  cd <directory>" NSH_RESET NSH_FG
                              "          Change directory\n" NSH_RESET);
            printf(NSH_ACCENT "  export" NSH_RESET NSH_FG "                  List all environment "
                              "variables\n" NSH_RESET);
            printf(NSH_ACCENT "  export VAR=value" NSH_RESET NSH_FG
                              "        Set and export environment "
                              "variable\n" NSH_RESET);
            printf(NSH_ACCENT "  export VAR" NSH_RESET NSH_FG
                              "              Export existing variable\n" NSH_RESET);
            printf(NSH_ACCENT "  echo [text]" NSH_RESET NSH_FG "             Print text (supports "
                              "$VAR expansion)\n" NSH_RESET);
            printf(NSH_ACCENT "  clear" NSH_RESET NSH_FG
                              "                   Clear the screen\n" NSH_RESET);
            printf(NSH_ACCENT "  help" NSH_RESET NSH_FG
                              "                    Show this help message\n" NSH_RESET);
            printf("\n");
            fflush(stdout);
            handled = 1;
        }

        if (!handled) {
            char *expanded_argv[64];
            int expanded_argc = 0;
            while (argv[expanded_argc] != NULL && expanded_argc < 63) {
                expanded_argv[expanded_argc] = expand_variable(argv[expanded_argc]);
                expanded_argc++;
            }
            expanded_argv[expanded_argc] = NULL;

            int is_script = path_looks_like_script(expanded_argv[0]);
            printf(NSH_RESET);
            fflush(stdout);

            if (is_script) {
                char **script_args = (expanded_argc > 1) ? &expanded_argv[1] : NULL;
                int exit_status = execute_script(expanded_argv[0], script_args);
                if (exit_status != 0) {
                    fprintf(stderr, NSH_ERR "Script exited with status: %d\n" NSH_RESET, exit_status);
                }
            } else {
                execute_external(expanded_argv);
            }

            for (int i = 0; i < expanded_argc; i++) {
                free(expanded_argv[i]);
            }
            // Reset again after external app in case it changed colors
            printf(NSH_RESET);
            fflush(stdout);
        }

        if (history_line[0] != '\0') {
            linenoiseHistoryAdd(history_line);
        }

        if (build_history_path(history_path, sizeof(history_path)) == 0) {
            linenoiseHistorySave(history_path);
        }

        free(history_line);
        free(line);

        // Reset to default colors, then set prompt color for next iteration
        // This ensures external apps start with default colors
        printf(NSH_RESET NSH_ACCENT);
        fflush(stdout);
    }

    return EXIT_SUCCESS;
}

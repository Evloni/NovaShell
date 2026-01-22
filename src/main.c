/*
 * NovaShell - GPLv3
 * Copyright (C) 2026 Evloni
 *
 * This file is part of NovaShell.
 * See LICENSE in the project root for full license information.
 */

#include "libs/utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

int main(int argc_main, char **argv_main) {
    char *line;
    char cwd_buff[PATH_MAX];
    char *equals_pos;
    char *var_name;
    char *var_value;
    char *argv[64]; // Max 64 arguments
    int argc;
    int handled = 0; // Flag to track if command was handled as built-in

    // If script provided as command-line argument, execute it and exit
    if (argc_main > 1) {
        char *script_path = argv_main[1];
        char **script_args = (argc_main > 2) ? &argv_main[2] : NULL;
        
        // Check if it's a script
        int is_script = 0;
        char *ext = strrchr(script_path, '.');
        if (ext && strcmp(ext, ".sh") == 0) {
            is_script = 1;
        } else if (access(script_path, F_OK) == 0) {
            FILE *file = fopen(script_path, "r");
            if (file) {
                char first_line[3];
                if (fgets(first_line, sizeof(first_line), file) && strncmp(first_line, "#!", 2) == 0) {
                    is_script = 1;
                }
                fclose(file);
            }
        }
        
        if (is_script) {
            int exit_status = execute_script(script_path, script_args);
            exit(exit_status);
        } else {
            // Execute as external program
            execute_external(&argv_main[1]);
            exit(EXIT_SUCCESS);
        }
    }

    banner();

    linenoiseHistoryLoad("history.txt");
    linenoiseSetCompletionCallback(completion);

    // Set prompt color before first prompt
    printf(NSH_ACCENT);
    fflush(stdout);

    while ((line = linenoise("nsh $ ")) != NULL) {
            // Handles White lines
            if (line[0] == '\0') {
                free(line);
                continue;
            }

            // Parse the command line
            argc = parse_command(line, argv, 64);
            if (argc == 0) {
                free(line);
                continue;
            }

            handled = 0;

            // handles buildt-in commands
            if (strcmp(argv[0], "exit") == 0) {
                free(line);
                exit(EXIT_SUCCESS);
            } else if (strcmp(argv[0], "pwd") == 0) {
                getcwd(cwd_buff, sizeof(cwd_buff));
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
                                char *var_value;
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

                                // Get and print variable value
                                var_value = getenv(var_name_buf);
                                if (var_value != NULL) {
                                    printf("%s", var_value);
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

            // If not a built-in command
            if (!handled) {
                int is_script = 0;
                // Check for .sh extension first
                char *ext = strrchr(argv[0], '.');
                if (ext && strcmp(ext, ".sh") == 0) {
                    is_script = 1;
                } else if (access(argv[0], F_OK) == 0) {
                    // Check for shebang in any existing file
                    FILE *file = fopen(argv[0], "r");
                    if (file) {
                        char first_line[3];
                        if (fgets(first_line, sizeof(first_line), file) && strncmp(first_line, "#!", 2) == 0) {
                            is_script = 1;
                        }
                        fclose(file);
                    }
                }
                printf(NSH_RESET);
                fflush(stdout);

                if (is_script) {
                    char **script_args = (argc > 1) ? &argv[1] : NULL;
                    int exit_status = execute_script(argv[0], script_args);
                    if (exit_status != 0) {
                        fprintf(stderr, NSH_ERR "Script exited with status: %d\n" NSH_RESET, exit_status);
                    }
                } else {
                    execute_external(argv);
                }
                // Reset again after external app in case it changed colors
                printf(NSH_RESET);
                fflush(stdout);
            }

            if (line[0] != '\0') {
                linenoiseHistoryAdd(line);
            }
            linenoiseHistorySave("history.txt");
            free(line);

            // Reset to default colors, then set prompt color for next iteration
            // This ensures external apps start with default colors
            printf(NSH_RESET NSH_ACCENT);
            fflush(stdout);
        }

    return EXIT_SUCCESS;
}

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "libs/linenoise.h"

/* Foreground / text colors */
#define NSH_FG "\033[38;2;230;230;230m"     /* #E6E6E6 - white/light gray for regular output \
                                             */
#define NSH_DIM "\033[38;2;154;160;166m"    /* #9AA0A6 - dimmed text */
#define NSH_ACCENT "\033[38;2;100;200;255m" /* #64C8FF - brighter cyan for prompt/commands */

/* Status / feedback */
#define NSH_OK "\033[38;2;100;255;100m"   /* #64FF64 - bright green for success \
                                           */
#define NSH_WARN "\033[38;2;255;200;100m" /* #FFC864 - bright yellow/orange for warnings */
#define NSH_ERR "\033[38;2;255;100;100m"  /* #FF6464 - bright red for errors \
                                           */
#define NSH_INFO "\033[38;2;150;200;255m" /* #96C8FF - softer blue for info text */

/* Reset / default */
#define NSH_RESET "\033[0m"

extern char **environ;

void banner(void) {
    printf(NSH_ACCENT "nsh — Nova Shell\n" NSH_RESET);
    printf(NSH_INFO "nsh"
                    " "
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

int main(void) {
    char *line;
    char cwd_buff[PATH_MAX];
    char *equals_pos;
    char *var_name;
    char *var_value;
    char *argv[64]; // Max 64 arguments
    int argc;
    int handled = 0; // Flag to track if command was handled as built-in

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
        } else if (strcmp(argv[0], "pwd") == 0 || strcmp(argv[0], "dir") == 0) {
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
            printf(NSH_ACCENT "  pwd, ls, dir" NSH_RESET NSH_FG "            Print current working "
                              "directory\n" NSH_RESET);
            printf(NSH_ACCENT "  cd <directory>" NSH_RESET NSH_FG
                              "          Change directory\n" NSH_RESET);
            printf(NSH_ACCENT "  export" NSH_RESET NSH_FG "                  List all environment "
                              "variables\n" NSH_RESET);
            printf(NSH_ACCENT "  export VAR=value" NSH_RESET NSH_FG
                              "       Set and export environment "
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

        // If not a built-in command, try to execute as external program
        if (!handled) {
            // Reset colors so external apps use default terminal colors
            printf(NSH_RESET);
            fflush(stdout);
            execute_external(argv);
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

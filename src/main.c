#include "libs/linenoise.h"
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

void banner(void) {
  printf("nsh — Nova Shell\n");
  printf("Type `help` to show available commands!\n");
}

void completion(const char *buff, linenoiseCompletions *lc) {
  const char *commands[] = {"exit",  "cd",   "echo", "export",
                            "clear", "help", "pwd",  "dir"};
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
      printf("%s\n", cwd_buff);
      handled = 1;
    } else if (strcmp(argv[0], "cd") == 0) {
      if (argc > 1) {
        if (chdir(argv[1]) != 0) {
          perror("cd");
        }
      } else {
        // cd with no arguments - go to home directory
        const char *home = getenv("HOME");
        if (home != NULL) {
          chdir(home);
        } else {
          fprintf(stderr, "cd: HOME not set\n");
        }
      }
      handled = 1;
    } else if (strcmp(argv[0], "export") == 0) {
      // Handle export command
      if (argc == 1) {
        // List all environment variables
        for (char **env = environ; *env != NULL; env++) {
          printf("declare -x %s\n", *env);
        }
      } else {
        var_name = argv[1];
        // Find '=' to separate variable name and value
        equals_pos = strchr(var_name, '=');

        if (equals_pos != NULL) {
          // export VAR=value
          *equals_pos = '\0'; // Temporarily null-terminate at '='
          var_value = equals_pos + 1;

          if (setenv(var_name, var_value, 1) != 0) {
            perror("export");
          }

          *equals_pos = '='; // Restore '=' for proper cleanup
        } else {
          // export VAR (export existing variable)
          // Check if variable exists
          if (getenv(var_name) != NULL) {
            // Variable exists, it's already in the environment
            // No action needed as it's already exported
          } else {
            // Variable doesn't exist, set it to empty string
            if (setenv(var_name, "", 1) != 0) {
              perror("export");
            }
          }
        }
      }
      handled = 1;
    } else if (strcmp(argv[0], "echo") == 0) {
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
                // Variable name can contain letters, numbers, and underscore
                while ((*var_end >= 'a' && *var_end <= 'z') ||
                       (*var_end >= 'A' && *var_end <= 'Z') ||
                       (*var_end >= '0' && *var_end <= '9') ||
                       *var_end == '_') {
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
              // If variable doesn't exist, print nothing (standard shell
              // behavior)
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
      handled = 1;
    } else if (strcmp(argv[0], "clear") == 0) {
      linenoiseClearScreen();
      banner();
      handled = 1;
    } else if (strcmp(argv[0], "help") == 0) {
      printf("  exit                    Exit the shell\n");
      printf("  pwd, ls, dir            Print current working directory\n");
      printf("  cd <directory>          Change directory\n");
      printf("  export                  List all environment variables\n");
      printf("  export VAR=value       Set and export environment variable\n");
      printf("  export VAR              Export existing variable\n");
      printf(
          "  echo [text]             Print text (supports $VAR expansion)\n");
      printf("  clear                   Clear the screen\n");
      printf("  help                    Show this help message\n");
      printf("\n");
      handled = 1;
    }

    // If not a built-in command, try to execute as external program
    if (!handled) {
      execute_external(argv);
    }

    if (line[0] != '\0') {
      linenoiseHistoryAdd(line);
    }
    linenoiseHistorySave("history.txt");
    free(line);
  }

  return EXIT_SUCCESS;
}

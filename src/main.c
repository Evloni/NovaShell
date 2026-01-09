#include "libs/linenoise.h"
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

void banner(void) {
  printf("nsh — Nova Shell\n");
  printf("Type `help` to show available commands!\n");
}

int main(void) {
  char *line;
  char cwd_buff[PATH_MAX];
  char *targetDir = NULL;
  char *equals_pos;
  char *var_name;
  char *var_value;

  banner();

  while ((line = linenoise("nsh $ ")) != NULL) {
    // Handles White lines
    if (line[0] == '\0') {
      free(line);
      continue;
    }

    // handles buildt-in commands
    if (strcmp(line, "exit") == 0) {
      free(line);
      exit(EXIT_SUCCESS);
    } else if (strcmp(line, "pwd") == 0 || strcmp(line, "ls") == 0 ||
               strcmp(line, "dir") == 0) {
      getcwd(cwd_buff, sizeof(cwd_buff));
      printf("%s\n", cwd_buff);
    } else if (line[0] == 'c' && line[1] == 'd' && line[2] == ' ') {
      targetDir = line + 3;
      if (chdir(targetDir) != 0) {
        perror("cd");
      }
    } else if (strncmp(line, "export ", 7) == 0 ||
               strcmp(line, "export") == 0) {
      // Handle export command
      if (strcmp(line, "export") == 0) {
        // List all environment variables
        for (char **env = environ; *env != NULL; env++) {
          printf("declare -x %s\n", *env);
        }
      } else {
        var_name = line + 7; // Skip "export "

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
    } else if (strncmp(line, "echo ", 5) == 0 || strcmp(line, "echo") == 0) {
      // Handle echo command with variable expansion
      if (strcmp(line, "echo") == 0) {
        // Just "echo" - print newline
        printf("\n");
      } else {
        // "echo " followed by arguments
        char *args = line + 5; // Skip "echo "
        char *current = args;

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
            // If variable doesn't exist, print nothing (standard shell
            // behavior)
          } else {
            // Regular character, print it
            putchar(*current);
            current++;
          }
        }
        printf("\n");
      }
    } else if (strcmp(line, "clear") == 0) {
      printf("\033[2J\033[H");
      banner();
      fflush(stdout);
    }

    free(line);
  }
  return EXIT_SUCCESS;
}

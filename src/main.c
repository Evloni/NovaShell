#include "libs/linenoise.h"
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  char *line;
  char cwd_buff[PATH_MAX];
  char *targetDir = NULL;

  printf("nsh — Nova Shell\n");
  printf("Type `help` to show available commands!\n");

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
    }

    free(line);
  }

  return EXIT_SUCCESS;
}

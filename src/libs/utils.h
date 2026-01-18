/*
 * NovaShell - GPLv3
 * Copyright (C) 2026 Evloni
 *
 * This file is part of NovaShell.
 * See LICENSE in the project root for full license information.
 */

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "linenoise.h"

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

void banner(void);
void completion(const char *buff, linenoiseCompletions *lc);
int parse_command(char *line, char **argv, int max_args);
void execute_external(char **argv);
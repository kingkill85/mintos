/* One shell command at a time in a background thread, so the list stays responsive
 * (Wi-Fi scans take ~11 s). */
#pragma once

/* input (may be NULL) goes to the command's stdin through a 0600 file in /run; -1 when busy */
int job_start(int kind, const char *cmd, const char *input);
int job_busy(void);
int job_kind(void);
/* 1 once when the command has finished: its kind, exit code and output (valid until the next start) */
int job_take(int *kind, int *rc, const char **out);
/* cmd-safe single quoting of s into dst */
void sh_quote(char *dst, int n, const char *s);

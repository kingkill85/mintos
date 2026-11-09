/* Shared by the MintOS tools: running commands, input devices, small helpers. */
#pragma once
#include <stddef.h>

/* run argv (PATH lookup), stdout into out (size n, may be 0), stdin from input (may be 0);
 * killed after timeout_ms. Returns the exit status, -1 on failure or timeout. */
int run_cmd(char *const argv[], const char *input, char *out, size_t n, int timeout_ms);
#define RUN_OUT(out, ms, ...) run_cmd((char *[]){ __VA_ARGS__, NULL }, NULL, out, sizeof out, ms)
#define RUN_Q(ms, ...) run_cmd((char *[]){ __VA_ARGS__, NULL }, NULL, NULL, 0, ms)

void sleep_ms(int ms);
/* settings.cfg has "<key> = off" (a line starting so) */
int setting_off(const char *path, const char *key);

/* /dev/input/eventN with that name, opened with flags; -1 if none */
int input_find(const char *name, int flags);
/* a uinput keyboard with these key codes; -1 on failure */
int uinput_keys(const char *name, const int *codes, int n);
void input_emit(int fd, int type, int code, int value);
/* press and release a key (with SYN), hold_ms between, after_ms after */
void input_tap(int fd, int code, int hold_ms, int after_ms);

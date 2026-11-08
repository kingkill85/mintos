/* Where saves and states live (spec: /storage/saves/<system>, /storage/states/<system>).
 * The env var RGARCH_ROOT prefixes all paths (tests). */
#pragma once
#include <stddef.h>

typedef enum { P_SRAM, P_RTC, P_STATE, P_PREVIEW, P_AUTO } PathKind;

/* rom: <root>/storage/roms/<system>/<sub...>/<file>; slot for P_STATE/P_PREVIEW.
 * 0 ok, -1 when rom is not under storage/roms. */
int path_for(PathKind k, const char *rom, int slot, char *out, size_t n);
int path_system(const char *rom, char *out, size_t n);
int mkdirs_for(const char *file);   /* mkdir -p of the file's directory */

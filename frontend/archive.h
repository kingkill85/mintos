/* Zipped/7z ROMs for cores that cannot open archives themselves (RetroArch did this too). */
#pragma once
#include <stddef.h>

/* valid_exts: the core's valid_extensions ("gb|gbc|zip"). If rom is a .zip/.7z the core
 * does not accept, extract it under $TMPDIR (default /tmp)/rgarch-rom and put the path of
 * the first file with a valid extension into out; otherwise out = rom. 0 ok, -1 failed. */
int archive_prepare(const char *rom, const char *valid_exts, char *out, size_t n);

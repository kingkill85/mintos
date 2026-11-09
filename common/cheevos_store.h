/* On-disk layout shared by mintos-cheevos (sync tool) and rgarch. No network, no rcheevos, no SDL. */
#pragma once
#include <stddef.h>

const char *cs_dir(void);   /* $MINTOS_CHEEVOS_DIR, else $RGARCH_ROOT/storage/.config/mintos/cheevos */
int cs_path(char *out, size_t n, const char *fmt, ...) __attribute__((format(printf, 3, 4)));  /* 0 ok */
int cs_write(const char *rel, const char *data, size_t len);  /* atomic (tmp+rename), makes subdirs; 0 ok */
char *cs_read(const char *rel, size_t *len);                  /* malloc'd, NUL-terminated; NULL if missing */
int cs_token(char *user, size_t un, char *token, size_t tn);  /* file "token": user\ntoken\n; 0 ok */
/* hashes.tsv: path\tsize\tmtime\thash\tgame_id\tchecked; matches only if size+mtime equal the file now. 0 found */
int cs_hash_lookup(const char *rom, char hash[33], unsigned *game_id);
int cs_queue_add(unsigned game_id, unsigned ach_id, const char *hash, long when);
int cs_local_unlocks(unsigned game_id, unsigned *ids, long *when, int max);  /* returns count */

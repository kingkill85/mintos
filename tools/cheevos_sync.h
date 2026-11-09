/* mintos-cheevos' logic: login, upload queued unlocks, hash ROMs, fetch game data. The network is injected. */
#pragma once
#include <stddef.h>

/* POST post to url; *body malloc'd by the callee (caller frees), *status = HTTP status. 0 = transport ok, -1 = no network. */
typedef int (*cs_http_fn)(const char *url, const char *post, char **body, int *status);
int cs_http_post(const char *url, const char *post, char **body, int *status);   /* libcurl (cheevos_http.c) */

typedef struct { int uploaded, failed, games, hashed; int token_invalid; } CsReport;

int cs_login(cs_http_fn http, const char *user, const char *password);   /* 0 ok, 1 wrong password, -1 no network */
int cs_logout(void);                                                     /* removes token, token.invalid, login.json; 0 ok */
/* upload queue -> hash ROMs under roms_dir/<system>/ -> game/<hash>.json -> user/<id>.json + login.json.
 * 0 ok, 1 no/invalid token (nothing deleted), -1 no network */
int cs_sync(cs_http_fn http, const char *roms_dir, CsReport *r);

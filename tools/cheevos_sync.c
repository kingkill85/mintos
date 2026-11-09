#define _XOPEN_SOURCE 700   /* nftw */
#define _DEFAULT_SOURCE     /* strcasestr, d_name etc. */
#include "cheevos_sync.h"
#include "cheevos_store.h"
#include "archive.h"
#include "rc_api_info.h"
#include "rc_api_runtime.h"
#include "rc_api_user.h"
#include "rc_hash.h"
#include <dirent.h>
#include <ftw.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define GAME_MAX_AGE (7L * 86400)
#define RETRY_AGE (30L * 86400)

/* ---- helpers ---- */

/* 0 ok; -1 no network, or the server/a proxy is not answering sanely (5xx) */
static int call(cs_http_fn http, const rc_api_request_t *rq, char **body, int *status) {
    *body = NULL; *status = 0;
    if (http(rq->url, rq->post_data, body, status)) { free(*body); *body = NULL; return -1; }
    if (*status >= 500 || *status == 429 || !*body) { free(*body); *body = NULL; return -1; }
    return 0;
}

/* the awardachievement/unlocks parsers do not read "Code", so look in the raw body too */
static int body_has_bad_code(const char *body) {
    const char *c = body;
    while ((c = strstr(c, "\"Code\""))) {
        c += 6;
        while (*c == ' ' || *c == ':') c++;
        if (!strncmp(c, "\"invalid_credentials\"", 21) || !strncmp(c, "\"expired_token\"", 15)) return 1;
    }
    return 0;
}

static int invalid_token(const rc_api_response_t *r, int status, const char *body) {
    if (body_has_bad_code(body)) return 1;
    if (r->succeeded) return 0;
    if (r->error_code && (!strcmp(r->error_code, "invalid_credentials") || !strcmp(r->error_code, "expired_token"))) return 1;
    return status == 401;
}

/* the server answered, though possibly with an error code; not: garbage (proxy page, cut-off body) */
static int bad_answer(int rc) { return rc == RC_INVALID_JSON || rc == RC_NO_RESPONSE || rc == RC_OUT_OF_MEMORY || rc == RC_INSUFFICIENT_BUFFER; }

static int mark_invalid(CsReport *r) {
    cs_write("token.invalid", "1\n", 2);
    r->token_invalid = 1;
    return 1;
}

static rc_api_server_response_t srv(const char *body, int status) {
    rc_api_server_response_t s;
    memset(&s, 0, sizeof s);
    s.body = body; s.body_length = strlen(body); s.http_status_code = status;
    return s;
}

/* ---- helpers for files ---- */

static int rm_cb(const char *p, const struct stat *st, int flag, struct FTW *f) { (void)st; (void)flag; (void)f; return remove(p); }
static void rm_rf(const char *path) { nftw(path, rm_cb, 16, FTW_DEPTH | FTW_PHYS); }
static void rm_rf_rel(const char *rel) { char p[1024]; if (!cs_path(p, sizeof p, "%s", rel)) rm_rf(p); }

static void to_failed(const char *name) {
    char from[1024], dir[1024], to[1100];
    if (cs_path(from, sizeof from, "queue/%s", name) || cs_path(dir, sizeof dir, "queue/failed")) return;
    mkdir(dir, 0755);
    snprintf(to, sizeof to, "%s/%s", dir, name);
    rename(from, to);
}

/* another account: its unlocks must not go to the new one, and its caches are wrong */
static void drop_account_data(void) {
    char dirp[1024];
    if (!cs_path(dirp, sizeof dirp, "queue")) {
        DIR *d = opendir(dirp);
        struct dirent *de;
        while (d && (de = readdir(d))) {
            size_t l = strlen(de->d_name);
            if (de->d_name[0] != '.' && l > 5 && !strcmp(de->d_name + l - 5, ".json")) to_failed(de->d_name);
        }
        if (d) closedir(d);
    }
    rm_rf_rel("local");
    rm_rf_rel("user");
}

/* ---- login ---- */

int cs_login(cs_http_fn http, const char *user, const char *password) {
    rc_api_login_request_t lp = { user, NULL, password };
    rc_api_request_t rq;
    if (rc_api_init_login_request(&rq, &lp) != RC_OK) return -1;
    char *body; int status;
    int net = call(http, &rq, &body, &status);
    rc_api_destroy_request(&rq);
    if (net) return -1;
    rc_api_login_response_t resp;
    rc_api_server_response_t s = srv(body, status);
    int rc = rc_api_process_login_server_response(&resp, &s), ret;
    if (bad_answer(rc)) ret = -1;
    else if (!resp.response.succeeded) ret = 1;
    else {
        const char *name = resp.username && *resp.username ? resp.username : user;
        char tok[512], last[128];
        int n = snprintf(tok, sizeof tok, "%s\n%s\n", name, resp.api_token);
        char *lu = cs_read("last_user", NULL);
        if (lu) { snprintf(last, sizeof last, "%s", lu); last[strcspn(last, "\n")] = 0; free(lu); if (strcasecmp(last, name)) drop_account_data(); }
        if (n <= 0 || n >= (int)sizeof tok || cs_write("login.json", body, strlen(body)) || cs_write("token", tok, n)
            || cs_write("last_user", tok, strchr(tok, '\n') - tok)) ret = -1;
        else {
            char p[1024];
            if (!cs_path(p, sizeof p, "token.invalid")) unlink(p);
            ret = 0;
        }
        memset(tok, 0, sizeof tok);
    }
    rc_api_destroy_login_response(&resp);
    free(body);
    return ret;
}

/* keeps queue/ and local/ (a later login of the same account uploads them) and last_user (a different account then drops them) */
int cs_logout(void) {
    const char *f[] = { "token", "token.invalid", "login.json" };
    for (int i = 0; i < 3; i++) {
        char p[1024];
        if (!cs_path(p, sizeof p, "%s", f[i])) unlink(p);
    }
    rm_rf_rel("user");
    return 0;
}

/* ---- queue ---- */

static int by_name(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* 0 done, 1 token invalid, -1 no network */
static int upload_queue(cs_http_fn http, const char *user, const char *token, CsReport *r) {
    char dirp[1024];
    if (cs_path(dirp, sizeof dirp, "queue")) return 0;
    DIR *d = opendir(dirp);
    if (!d) return 0;
    char **names = NULL; int n = 0, cap = 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        size_t l = strlen(de->d_name);
        if (de->d_name[0] == '.' || l < 6 || strcmp(de->d_name + l - 5, ".json")) continue;   /* skips *.tmp and failed/ */
        char p[1400]; struct stat st;
        snprintf(p, sizeof p, "%s/%s", dirp, de->d_name);
        if (stat(p, &st) || !S_ISREG(st.st_mode)) continue;
        if (n == cap && !(names = realloc(names, (cap = cap ? cap * 2 : 64) * sizeof *names))) { closedir(d); return 0; }
        names[n++] = strdup(de->d_name);
    }
    closedir(d);
    qsort(names, n, sizeof *names, by_name);

    int ret = 0;
    for (int i = 0; i < n && !ret; i++) {
        char rel[300], hash[40] = "";
        unsigned g, a; long t;
        snprintf(rel, sizeof rel, "queue/%s", names[i]);
        char *js = cs_read(rel, NULL);
        if (!js) continue;
        int ok = sscanf(js, "{\"g\":%u,\"a\":%u,\"m\":\"%32[0-9a-fA-F]\",\"t\":%ld}", &g, &a, hash, &t) == 4 && strlen(hash) == 32;
        free(js);
        if (!ok) { to_failed(names[i]); r->failed++; continue; }
        long ago = (long)time(NULL) - t;
        rc_api_award_achievement_request_t ap = { user, token, a, 0, hash, ago < 0 ? 0 : (unsigned)ago };
        rc_api_request_t rq;
        if (rc_api_init_award_achievement_request(&rq, &ap) != RC_OK) { to_failed(names[i]); r->failed++; continue; }
        char *body; int status;
        int net = call(http, &rq, &body, &status);
        rc_api_destroy_request(&rq);
        if (net) { ret = -1; break; }
        rc_api_award_achievement_response_t resp;
        rc_api_server_response_t s = srv(body, status);
        int rc = rc_api_process_award_achievement_server_response(&resp, &s);
        char p[1024];
        if (bad_answer(rc)) ret = -1;                 /* not an RA answer (captive portal...): keep, retry later */
        else if (invalid_token(&resp.response, status, body)) ret = mark_invalid(r);
        else if (resp.response.succeeded) {
            if (!cs_path(p, sizeof p, "%s", rel)) unlink(p);
            if (!resp.response.error_message) r->uploaded++;   /* RF4: rcheevos turns "User already has ..." into success, with the message kept: done, not uploaded */
        } else { to_failed(names[i]); r->failed++; }
        rc_api_destroy_award_achievement_response(&resp);
        free(body);
    }
    for (int i = 0; i < n; i++) free(names[i]);
    free(names);
    return ret;
}

/* ---- ROM hashes ---- */

typedef struct {
    char path[1024];
    long long size, mtime;
    char hash[33];          /* "-" if hashing failed */
    unsigned gid;
    long checked;           /* 0: never looked up */
    unsigned console;
    int seen, need_hash, need_lookup;
} Row;

static int hash_rom(const CsSystem *sys, const char *rom, char hash[33]) {
    char out[1024];
    if (sys->console == RC_CONSOLE_ARCADE) return rc_hash_generate_from_file(hash, sys->console, rom) ? 0 : -1;   /* by file name */
    if (archive_prepare(rom, sys->exts, out, sizeof out)) return -1;
    int ok = rc_hash_generate_from_file(hash, sys->console, out);
    if (strcmp(out, rom)) {   /* extracted into $TMPDIR/rgarch-rom: remove again */
        char *s = strrchr(out, '/');
        if (s) { *s = 0; rm_rf(out); }   /* the whole extraction dir (cue+bins), as archive_prepare clears it */
    }
    return ok ? 0 : -1;
}

static Row *load_rows(int *n) {
    Row *rows = NULL; int cap = 0;
    *n = 0;
    char *tsv = cs_read("hashes.tsv", NULL);
    if (!tsv) return NULL;
    for (char *line = strtok(tsv, "\n"); line; line = strtok(NULL, "\n")) {
        char *fld[6]; int nf = 0; char *s = line;
        while (nf < 6) { fld[nf++] = s; char *t = strchr(s, '\t'); if (!t) break; *t = 0; s = t + 1; }
        if (nf < 6 || strlen(fld[0]) >= sizeof rows->path || strlen(fld[3]) > 32) continue;
        if (*n == cap && !(rows = realloc(rows, (cap = cap ? cap * 2 : 256) * sizeof *rows))) { *n = 0; free(tsv); return NULL; }
        Row *r = &rows[(*n)++];
        memset(r, 0, sizeof *r);
        snprintf(r->path, sizeof r->path, "%s", fld[0]);
        r->size = atoll(fld[1]); r->mtime = atoll(fld[2]);
        snprintf(r->hash, sizeof r->hash, "%s", fld[3]);
        r->gid = (unsigned)strtoul(fld[4], NULL, 10);
        r->checked = atol(fld[5]);
    }
    free(tsv);
    return rows;
}

static int save_rows(const Row *rows, int n) {
    size_t cap = (size_t)n * 1200 + 16, len = 0;
    char *b = malloc(cap);
    if (!b) return -1;
    for (int i = 0; i < n; i++)
        len += snprintf(b + len, cap - len, "%s\t%lld\t%lld\t%s\t%u\t%ld\n", rows[i].path, rows[i].size, rows[i].mtime, rows[i].hash, rows[i].gid, rows[i].checked);
    int rc = cs_write("hashes.tsv", b, len);
    free(b);
    return rc;
}

static int by_hash(const void *a, const void *b) {
    return strcasecmp((*(const rc_api_hash_library_entry_t *const *)a)->hash, (*(const rc_api_hash_library_entry_t *const *)b)->hash);
}

/* one hashlibrary request for console; sets gid of the rows that need it. 0 ok, -1 no network */
static int lookup_console(cs_http_fn http, Row *rows, int n, unsigned console) {
    rc_api_fetch_hash_library_request_t lp = { console };
    rc_api_request_t rq;
    if (rc_api_init_fetch_hash_library_request(&rq, &lp) != RC_OK) return 0;
    char *body; int status;
    int net = call(http, &rq, &body, &status);
    rc_api_destroy_request(&rq);
    if (net) return -1;
    rc_api_fetch_hash_library_response_t resp;
    rc_api_server_response_t s = srv(body, status);
    int garbage = bad_answer(rc_api_process_fetch_hash_library_server_response(&resp, &s));
    if (!garbage && resp.response.succeeded) {
        const rc_api_hash_library_entry_t **ix = malloc((resp.num_entries + 1) * sizeof *ix);
        for (uint32_t i = 0; ix && i < resp.num_entries; i++) ix[i] = &resp.entries[i];
        if (ix) qsort(ix, resp.num_entries, sizeof *ix, by_hash);
        time_t now = time(NULL);
        for (int i = 0; ix && i < n; i++) {
            if (!rows[i].need_lookup || rows[i].console != console) continue;
            rc_api_hash_library_entry_t key = { rows[i].hash, 0 };
            const rc_api_hash_library_entry_t *k = &key, **f = bsearch(&k, ix, resp.num_entries, sizeof *ix, by_hash);
            rows[i].gid = f ? (*f)->game_id : 0;
            rows[i].checked = now;
            rows[i].need_lookup = 0;
        }
        free(ix);
    }
    rc_api_destroy_fetch_hash_library_response(&resp);
    free(body);
    return garbage ? -1 : 0;
}

typedef struct { Row *rows; int n, cap; long now; CsReport *rep; } Scan;

static int ext_in(const char *list, const char *ext) {
    size_t n = strlen(ext);
    for (const char *p = list; p && *p;) {
        const char *bar = strchr(p, '|');
        size_t len = bar ? (size_t)(bar - p) : strlen(p);
        if (len == n && !strncasecmp(p, ext, n)) return 1;
        p = bar ? bar + 1 : NULL;
    }
    return 0;
}

static void scan_file(Scan *c, const CsSystem *sys, const char *path, const struct stat *st) {
    Row *r = NULL;
    for (int i = 0; i < c->n && !r; i++) if (!strcmp(c->rows[i].path, path)) r = &c->rows[i];
    if (!r) {
        if (c->n == c->cap) {
            Row *nr = realloc(c->rows, (c->cap ? c->cap * 2 : 256) * sizeof *nr);
            if (!nr) return;
            c->rows = nr; c->cap = c->cap ? c->cap * 2 : 256;
        }
        r = &c->rows[c->n++];
        memset(r, 0, sizeof *r);
        snprintf(r->path, sizeof r->path, "%s", path);
        r->size = -1;
    }
    r->seen = 1; r->console = sys->console;
    if (r->size != st->st_size || r->mtime != st->st_mtime) r->need_hash = 1;                  /* new or changed */
    else if (!r->gid && (!r->checked || c->now - r->checked > RETRY_AGE)) {                      /* unknown game: retry monthly */
        if (strlen(r->hash) == 32) r->need_lookup = 1; else r->need_hash = 1;
    }
    if (r->need_hash) {
        r->size = st->st_size; r->mtime = st->st_mtime; r->gid = 0; r->checked = 0;
        if (hash_rom(sys, path, r->hash)) snprintf(r->hash, sizeof r->hash, "-");
        else c->rep->hashed++;
        r->need_hash = 0;
        r->need_lookup = strlen(r->hash) == 32;
        if (!r->need_lookup) r->checked = c->now;   /* not a ROM: no server question */
    }
}

/* same rules as the launcher's scan_dir: depth 2, no dotfiles, no images/videos/manuals; only the system's extensions (+ archives) */
static void scan_dir(Scan *c, const CsSystem *sys, const char *dir, int depth) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d))) {
        char path[1024]; struct stat st;
        if (de->d_name[0] == '.' || snprintf(path, sizeof path, "%s/%s", dir, de->d_name) >= (int)sizeof path) continue;
        if (stat(path, &st)) continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth < 2 && strcasecmp(de->d_name, "images") && strcasecmp(de->d_name, "videos") && strcasecmp(de->d_name, "manuals"))
                scan_dir(c, sys, path, depth + 1);
        } else if (S_ISREG(st.st_mode)) {
            const char *dot = strrchr(de->d_name, '.');
            if (dot && dot != de->d_name && (ext_in(sys->exts, dot + 1) || ext_in("zip|7z", dot + 1))) scan_file(c, sys, path, &st);
        }
    }
    closedir(d);
}

/* scan roms_dir, update rows (hash what changed), ask the server about unknown hashes. 0 ok, -1 no network */
static int hash_step(cs_http_fn http, const char *roms_dir, Row **rowsp, int *np, CsReport *rep) {
    int n, cap;
    Row *rows = load_rows(&n);
    cap = n;
    char scanned[64][64]; int nscanned = 0;
    long now = (long)time(NULL);
    rc_hash_init_default_cdreader();

    Scan sc = { rows, n, cap, now, rep };
    DIR *rd = opendir(roms_dir);
    struct dirent *sd;
    while (rd && (sd = readdir(rd))) {
        const CsSystem *sys = cs_system(sd->d_name);
        char sdir[1024];
        if (!sys || snprintf(sdir, sizeof sdir, "%s/%s", roms_dir, sd->d_name) >= (int)sizeof sdir) continue;
        DIR *t = opendir(sdir);
        if (!t) continue;
        closedir(t);
        if (nscanned < 64) snprintf(scanned[nscanned++], 64, "%.63s", sd->d_name);
        scan_dir(&sc, sys, sdir, 0);
    }
    if (rd) closedir(rd);
    rows = sc.rows; n = sc.n;

    /* drop rows of ROMs that are gone (only in system dirs we could read) */
    size_t rl = strlen(roms_dir);
    int m = 0;
    for (int i = 0; i < n; i++) {
        int keep = rows[i].seen;
        if (!keep) {
            keep = 1;
            if (!strncmp(rows[i].path, roms_dir, rl) && rows[i].path[rl] == '/') {
                const char *sys = rows[i].path + rl + 1;
                const char *sl = strchr(sys, '/');
                for (int k = 0; k < nscanned && sl; k++)
                    if (strlen(scanned[k]) == (size_t)(sl - sys) && !strncmp(scanned[k], sys, sl - sys)) keep = 0;
            }
        }
        if (keep) rows[m++] = rows[i];
    }
    n = m;

    int net = 0;
    unsigned asked[64]; int nasked = 0;   /* one hashlibrary request per console and sync, whatever the answer */
    for (int i = 0; i < n && !net; i++) {
        if (!rows[i].need_lookup) continue;
        int done = 0;
        for (int k = 0; k < nasked; k++) done |= asked[k] == rows[i].console;
        if (done || nasked == 64) continue;
        asked[nasked++] = rows[i].console;
        net = lookup_console(http, rows, n, rows[i].console);
    }
    if (!net || n) save_rows(rows, n);
    *rowsp = rows; *np = n;
    return net ? -1 : 0;
}

/* ---- game data and sessions ---- */

static int has_unlock(const uint32_t *u, uint32_t n, unsigned id) {
    for (uint32_t i = 0; i < n; i++) if (u[i] == id) return 1;
    return 0;
}

/* drop local/<gid>.txt entries the server's list already has */
static void clean_local(unsigned gid, const rc_api_fetch_user_unlocks_response_t *s) {
    unsigned ids[1024]; long when[1024];
    int n = cs_local_unlocks(gid, ids, when, 1024), kept = 0;
    char buf[1024 * 32], *w = buf;
    for (int i = 0; i < n; i++) {
        if (has_unlock(s->achievement_ids, s->num_achievement_ids, ids[i])) continue;
        w += snprintf(w, buf + sizeof buf - w, "%u %ld\n", ids[i], when[i]);
        kept++;
    }
    if (kept == n) return;
    char rel[64];
    snprintf(rel, sizeof rel, "local/%u.txt", gid);
    if (kept) cs_write(rel, buf, w - buf);
    else { char p[1024]; if (!cs_path(p, sizeof p, "%s", rel)) unlink(p); }
}

/* a stored game/<hash>.json that still parses as a successful achievementsets answer */
static int game_file_ok(const char *rel) {
    char *body = cs_read(rel, NULL);
    if (!body) return 0;
    rc_api_fetch_game_sets_response_t resp;
    rc_api_server_response_t s = srv(body, 200);
    int ok = rc_api_process_fetch_game_sets_server_response(&resp, &s) == RC_OK && resp.response.succeeded;
    rc_api_destroy_fetch_game_sets_response(&resp);
    free(body);
    return ok;
}

/* 0 ok, 1 token invalid, -1 no network */
static int fetch_game(cs_http_fn http, const char *user, const char *token, const char *hash, CsReport *rep) {
    char rel[96], p[1024]; struct stat st;
    snprintf(rel, sizeof rel, "game/%s.json", hash);
    if (!cs_path(p, sizeof p, "%s", rel) && !stat(p, &st) && time(NULL) - st.st_mtime < GAME_MAX_AGE && game_file_ok(rel)) return 0;
    rc_api_fetch_game_sets_request_t gp = { user, token, 0, hash };
    rc_api_request_t rq;
    if (rc_api_init_fetch_game_sets_request(&rq, &gp) != RC_OK) return 0;
    char *body; int status;
    int net = call(http, &rq, &body, &status);
    rc_api_destroy_request(&rq);
    if (net) return -1;
    rc_api_fetch_game_sets_response_t resp;
    rc_api_server_response_t s = srv(body, status);
    int ret = 0;
    if (!bad_answer(rc_api_process_fetch_game_sets_server_response(&resp, &s))) {
        if (invalid_token(&resp.response, status, body)) ret = mark_invalid(rep);
        else if (resp.response.succeeded) cs_write(rel, body, strlen(body));
    }
    rc_api_destroy_fetch_game_sets_response(&resp);
    free(body);
    return ret;
}

/* user/<gid>.json = the raw r=unlocks answer ("UserUnlocks":[ids], softcore). Not startsession: that would mark the game as played on the profile. */
static int fetch_unlocks(cs_http_fn http, const char *user, const char *token, unsigned gid, CsReport *rep) {
    rc_api_fetch_user_unlocks_request_t up = { user, token, gid, 0 };
    rc_api_request_t rq;
    if (rc_api_init_fetch_user_unlocks_request(&rq, &up) != RC_OK) return 0;
    char *body; int status;
    int net = call(http, &rq, &body, &status);
    rc_api_destroy_request(&rq);
    if (net) return -1;
    rc_api_fetch_user_unlocks_response_t resp;
    rc_api_server_response_t s = srv(body, status);
    int rc = rc_api_process_fetch_user_unlocks_server_response(&resp, &s), ret = 0;
    if (bad_answer(rc)) ret = -1;
    else if (invalid_token(&resp.response, status, body)) ret = mark_invalid(rep);
    else if (resp.response.succeeded) {
        char rel[64];
        snprintf(rel, sizeof rel, "user/%u.json", gid);
        if (!cs_write(rel, body, strlen(body))) clean_local(gid, &resp);
    }
    rc_api_destroy_fetch_user_unlocks_response(&resp);
    free(body);
    return ret;
}

static int refresh_login(cs_http_fn http, const char *user, const char *token, CsReport *rep) {
    rc_api_login_request_t lp = { user, token, NULL };
    rc_api_request_t rq;
    if (rc_api_init_login_request(&rq, &lp) != RC_OK) return 0;
    char *body; int status;
    int net = call(http, &rq, &body, &status);
    rc_api_destroy_request(&rq);
    if (net) return -1;
    rc_api_login_response_t resp;
    rc_api_server_response_t s = srv(body, status);
    int ret = 0;
    if (bad_answer(rc_api_process_login_server_response(&resp, &s))) ret = -1;
    else {
        if (invalid_token(&resp.response, status, body)) ret = mark_invalid(rep);
        else if (resp.response.succeeded) cs_write("login.json", body, strlen(body));
    }
    rc_api_destroy_login_response(&resp);
    free(body);
    return ret;
}

/* ---- sync ---- */

static int has_game(const Row *r) {   /* seen this run, known game, its game/<hash>.json is there */
    char p[1024];
    return r->seen && r->gid && !cs_path(p, sizeof p, "game/%s.json", r->hash) && !access(p, F_OK);
}

int cs_sync(cs_http_fn http, const char *roms_dir, CsReport *rep) {
    memset(rep, 0, sizeof *rep);
    char user[128], token[256];
    if (cs_token(user, sizeof user, token, sizeof token)) return 1;

    int rc = upload_queue(http, user, token, rep);
    if (rc) return rc;
    /* the server must answer before any local work (hashing): with an empty queue this is the first call */
    if ((rc = refresh_login(http, user, token, rep))) return rc;

    Row *rows; int n;
    rc = hash_step(http, roms_dir, &rows, &n, rep);
    if (rc) { free(rows); return rc; }


    /* distinct hashes with a known game */
    for (int i = 0; i < n && !rc; i++) {
        if (!rows[i].seen || !rows[i].gid) continue;
        int dup = 0;
        for (int j = 0; j < i && !dup; j++) dup = rows[j].seen && !strcmp(rows[j].hash, rows[i].hash);
        if (!dup) rc = fetch_game(http, user, token, rows[i].hash, rep);
    }
    for (int i = 0; i < n && !rc; i++) {
        if (!has_game(&rows[i])) continue;
        int dup = 0;
        for (int j = 0; j < i && !dup; j++) dup = rows[j].gid == rows[i].gid && has_game(&rows[j]);
        if (dup) continue;
        rep->games++;
        rc = fetch_unlocks(http, user, token, rows[i].gid, rep);
    }
    free(rows);
    return rc;
}

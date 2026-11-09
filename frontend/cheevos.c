/* RetroAchievements in rgarch (see cheevos.h): rc_client with an offline server call. */
#include "cheevos.h"
#include "cheevos_store.h"
#include "osd.h"
#include "settings.h"
#include "rc_client.h"
#include "rc_libretro.h"
#include "rc_api_user.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_UNLOCKS 4096

static rc_client_t *client;
static rc_libretro_memory_regions_t regions;
static Core *core;
static unsigned game_id;
static char hash[33], user[128], token[128];
static int login_rc, load_rc;   /* results of the (synchronous) begin_* calls */

/* ---------- offline server ---------- */

/* value of key in "r=x&k=v&..." (no URL decoding: names, hashes and numbers need none) */
static bool param(const char *post, const char *key, char *out, size_t n) {
    size_t kl = strlen(key);
    for (const char *s = post; s; s = strchr(s, '&') ? strchr(s, '&') + 1 : NULL)
        if (!strncmp(s, key, kl) && s[kl] == '=') {
            const char *v = s + kl + 1;
            size_t l = strcspn(v, "&");
            if (l >= n) return false;
            memcpy(out, v, l);
            out[l] = 0;
            return true;
        }
    return false;
}

static bool alnum(const char *s) {
    if (!*s) return false;
    for (; *s; s++)
        if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))) return false;
    return true;
}

static void answer(rc_client_server_callback_t cb, void *data, const char *body, int status) {
    rc_api_server_response_t r = {body, strlen(body), status};
    cb(&r, data);
}

/* login.json is the raw login2 answer the sync tool stored */
static char *login_answer(void) {
    char *b = cs_read("login.json", NULL);
    if (b) return b;
    b = malloc(512);
    if (b) snprintf(b, 512, "{\"Success\":true,\"User\":\"%s\",\"Token\":\"%s\",\"Score\":0,\"SoftcoreScore\":0}", user, token);
    return b;
}

/* startsession: what the server knows (user/<g>.json = raw r=unlocks answer, softcore, no times)
 * plus the unlocks made here and not uploaded yet (local/<g>.txt), without duplicates */
static char *session_answer(unsigned g) {
    static unsigned ids[MAX_UNLOCKS];
    static long when[MAX_UNLOCKS];
    long now = (long)time(NULL);
    int n = cs_local_unlocks(g, ids, when, MAX_UNLOCKS);
    char rel[64];
    snprintf(rel, sizeof rel, "user/%u.json", g);
    size_t len = 0;
    char *body = cs_read(rel, &len);
    if (body) {
        rc_api_fetch_user_unlocks_response_t resp;
        rc_api_server_response_t sr = {body, len, 200};
        if (rc_api_process_fetch_user_unlocks_server_response(&resp, &sr) == RC_OK && resp.response.succeeded)
            for (uint32_t i = 0; i < resp.num_achievement_ids && n < MAX_UNLOCKS; i++) {
                int dup = 0;
                for (int j = 0; j < n && !dup; j++) dup = ids[j] == resp.achievement_ids[i];
                if (!dup) { ids[n] = resp.achievement_ids[i]; when[n] = now; n++; }
            }
        else rg_log("cheevos: %s unreadable\n", rel);
        rc_api_destroy_fetch_user_unlocks_response(&resp);
        free(body);
    }
    size_t cap = 64 + (size_t)n * 48;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t k = snprintf(out, cap, "{\"Success\":true,\"Unlocks\":[");
    for (int i = 0; i < n; i++) k += snprintf(out + k, cap - k, "%s{\"ID\":%u,\"When\":%ld}", i ? "," : "", ids[i], when[i]);
    snprintf(out + k, cap - k, "],\"ServerNow\":%ld}", now);
    return out;
}

static void server_call(const rc_api_request_t *req, rc_client_server_callback_t cb, void *data, rc_client_t *c) {
    (void)c;
    const char *post = req->post_data ? req->post_data : "";
    char r[32] = "", v[64] = "";
    param(post, "r", r, sizeof r);
    char *body = NULL;
    if (!strcmp(r, "login2")) {
        body = login_answer();
    } else if (!strcmp(r, "achievementsets")) {
        char rel[96];
        if (param(post, "m", v, sizeof v) && alnum(v)) {
            snprintf(rel, sizeof rel, "game/%s.json", v);
            body = cs_read(rel, NULL);
        }
        if (!body) { answer(cb, data, "{\"Success\":false,\"Error\":\"Unknown game\",\"Code\":\"not_found\",\"Status\":404}", 404); return; }
    } else if (!strcmp(r, "startsession")) {
        body = session_answer(param(post, "g", v, sizeof v) ? (unsigned)strtoul(v, NULL, 10) : game_id);
    } else if (!strcmp(r, "awardachievement")) {
        unsigned a = param(post, "a", v, sizeof v) ? (unsigned)strtoul(v, NULL, 10) : 0;
        if (cs_queue_add(game_id, a, hash, (long)time(NULL))) {
            rg_log("cheevos: cannot queue achievement %u, retrying\n", a);
            answer(cb, data, "cannot write the unlock queue", RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR);
            return;
        }
        const rc_client_user_t *u = rc_client_get_user_info(client);
        char b[160];
        snprintf(b, sizeof b, "{\"Success\":true,\"AchievementID\":%u,\"Score\":%u,\"SoftcoreScore\":%u}",
                 a, u ? u->score : 0, u ? u->score_softcore : 0);
        rg_log("cheevos: achievement %u queued\n", a);
        answer(cb, data, b, 200);
        return;
    }
    if (!body) { answer(cb, data, "{\"Success\":true}", 200); return; }   /* ping and the rest */
    answer(cb, data, body, 200);
    free(body);
}

/* ---------- rc_client glue ---------- */

static uint32_t read_memory(uint32_t addr, uint8_t *buf, uint32_t n, rc_client_t *c) {
    (void)c;
    return rc_libretro_memory_read(&regions, addr, buf, n);
}

static void memory_info(uint32_t id, rc_libretro_core_memory_info_t *info) {
    size_t size = 0;
    info->data = core_memory(core, id, &size);
    info->size = info->data ? size : 0;
}

static void log_message(const char *msg, const rc_client_t *c) { (void)c; rg_log("[rcheevos] %s\n", msg); }

static void on_event(const rc_client_event_t *e, rc_client_t *c) {
    (void)c;
    if (e->type == RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED && e->achievement) {
        char t[128];
        snprintf(t, sizeof t, "Erfolg: %s (%u)", e->achievement->title, e->achievement->points);
        osd_toast(t);
        rg_log("cheevos: unlocked %u %s\n", e->achievement->id, e->achievement->title);
    } else if (e->type == RC_CLIENT_EVENT_SERVER_ERROR && e->server_error) {
        rg_log("cheevos: %s: %s\n", e->server_error->api, e->server_error->error_message);
    }
}

static void login_done(int rc, const char *msg, rc_client_t *c, void *ud) {
    (void)c; (void)ud;
    login_rc = rc;
    if (rc != RC_OK) rg_log("cheevos: login: %s\n", msg ? msg : "failed");
}

static void load_done(int rc, const char *msg, rc_client_t *c, void *ud) {
    (void)c; (void)ud;
    load_rc = rc;
    if (rc != RC_OK) rg_log("cheevos: load game: %s\n", msg ? msg : "failed");
}

static bool enabled(void) {
    char p[1024];
    const char *r = getenv("RGARCH_ROOT");
    snprintf(p, sizeof p, "%s%s", r ? r : "", SETTINGS_FILE);
    return kv_get_int(p, "cheevos", 1) != 0;
}

/* ---------- API ---------- */

bool cheevos_start(Core *c, const char *system, const char *rom) {
    cheevos_stop();
    if (!enabled()) return false;
    const CsSystem *sys = cs_system(system);
    if (!sys) return false;
    if (cs_token(user, sizeof user, token, sizeof token)) return false;
    if (cs_hash_lookup(rom, hash, &game_id)) { rg_log("cheevos: no hash for %s (not synced, or changed since)\n", rom); return false; }
    char p[1024];
    if (cs_path(p, sizeof p, "game/%s.json", hash) || access(p, R_OK)) { rg_log("cheevos: no game data for %s\n", hash); return false; }

    core = c;
    if (!rc_libretro_memory_init(&regions, core_memory_map(c), memory_info, sys->console)) {
        rg_log("cheevos: no memory to read for %s\n", system);
        return false;
    }
    client = rc_client_create(read_memory, server_call);
    if (!client) return false;
    rc_client_enable_logging(client, RC_CLIENT_LOG_LEVEL_WARN, log_message);
    rc_client_set_hardcore_enabled(client, 0);
    rc_client_set_event_handler(client, on_event);

    /* the server call answers at once, so both calls complete before they return */
    login_rc = load_rc = -1;
    rc_client_begin_login_with_token(client, user, token, login_done, NULL);
    if (login_rc == RC_OK) rc_client_begin_load_game(client, hash, load_done, NULL);
    if (login_rc != RC_OK || load_rc != RC_OK) { cheevos_stop(); return false; }

    rc_client_user_game_summary_t s;
    rc_client_get_user_game_summary(client, &s);
    char t[64];
    snprintf(t, sizeof t, "%u/%u Erfolge", s.num_unlocked_achievements, s.num_core_achievements);
    osd_toast(t);
    rg_log("cheevos: game %u %s: %s\n", game_id, hash, t);
    return true;
}

void cheevos_frame(void) { if (client) rc_client_do_frame(client); }
void cheevos_idle(void) { if (client) rc_client_idle(client); }

void cheevos_reset(void) {
    if (!client) return;
    rc_client_reset(client);
    rg_log("cheevos reset\n");
}

void cheevos_stop(void) {
    if (client) {
        rc_client_unload_game(client);
        rc_client_destroy(client);
        client = NULL;
    }
    rc_libretro_memory_destroy(&regions);
    core = NULL;
}

int cheevos_list(CheevoItem *out, int max) {
    if (!client) return 0;
    rc_client_achievement_list_t *l = rc_client_create_achievement_list(client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,
                                                                        RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (!l) return 0;
    int n = 0;
    for (uint32_t b = 0; b < l->num_buckets; b++)
        for (uint32_t i = 0; i < l->buckets[b].num_achievements && n < max; i++) {
            const rc_client_achievement_t *a = l->buckets[b].achievements[i];
            snprintf(out[n].title, sizeof out[n].title, "%s", a->title);
            snprintf(out[n].desc, sizeof out[n].desc, "%s", a->description);
            out[n].points = a->points;
            out[n].unlocked = a->unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE;
            n++;
        }
    rc_client_destroy_achievement_list(l);
    return n;
}

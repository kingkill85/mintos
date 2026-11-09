/* cs_http_post over libcurl. */
#include "cheevos_sync.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

#ifndef MINTOS_VERSION
#define MINTOS_VERSION "dev"
#endif

typedef struct { char *p; size_t n; } Buf;

static size_t sink(char *d, size_t sz, size_t nm, void *u) {
    Buf *b = u;
    size_t k = sz * nm;
    if (b->n + k > (8u << 20)) return 0;   /* hash libraries are a few MB at most */
    char *np = realloc(b->p, b->n + k + 1);
    if (!np) return 0;
    memcpy(np + b->n, d, k);
    b->p = np; b->n += k; b->p[b->n] = 0;
    return k;
}

int cs_http_post(const char *url, const char *post, char **body, int *status) {
    static int init;
    if (!init) { curl_global_init(CURL_GLOBAL_DEFAULT); init = 1; }
    CURL *c = curl_easy_init();
    if (!c) return -1;
    Buf b = { NULL, 0 };
    struct curl_slist *hd = curl_slist_append(NULL, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, post);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hd);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "MintOS/" MINTOS_VERSION " rcheevos/12.5.0");
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, sink);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
    CURLcode rc = curl_easy_perform(c);
    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    curl_slist_free_all(hd);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) { free(b.p); return -1; }
    if (!b.p) b.p = calloc(1, 1);
    *body = b.p; *status = (int)code;
    return 0;
}

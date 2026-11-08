// uses: launcher/job.c
// libs: $(pkg-config --libs sdl2)
#include <string.h>
#include <SDL.h>
#include "check.h"
#include "../../launcher/job.h"

static int wait_take(int *k, int *rc, const char **out) {
    for (int i = 0; i < 300; i++) { if (job_take(k, rc, out)) return 1; SDL_Delay(10); }
    return 0;
}

int main(void) {
    char q[64];
    sh_quote(q, sizeof q, "it's");
    CHECK_STR(q, "'it'\\''s'");
    sh_quote(q, sizeof q, "");
    CHECK_STR(q, "''");

    int k, rc;
    const char *out;
    CHECK_INT(job_take(&k, &rc, &out), 0);              /* nothing yet */
    CHECK_INT(job_start(7, "echo hi; sleep 0.3; exit 3", NULL), 0);
    CHECK(job_busy());
    CHECK_INT(job_start(8, "true", NULL), -1);          /* one at a time */
    CHECK(wait_take(&k, &rc, &out));
    CHECK_INT(k, 7); CHECK_INT(rc, 3);
    CHECK_STR(out, "hi\n");
    CHECK(!job_busy());

    /* stdin from the input text; a quoted argument with a quote survives */
    char cmd[200], a[64];
    sh_quote(a, sizeof a, "Bob's Net");
    snprintf(cmd, sizeof cmd, "printf '%%s|' %s; cat", a);
    if (job_start(1, cmd, "pass word") == 0 && wait_take(&k, &rc, &out)) CHECK_STR(out, "Bob's Net|pass word\n");
    else CHECK(0);
    TEST_END();
}

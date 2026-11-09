// uses: launcher/settings.c launcher/keyboard.c common/ui.c common/settings.c common/cheevos_store.c frontend/config.c frontend/gameopts.c -Ithird_party/rcheevos/include
// libs: $(pkg-config --libs sdl2 SDL2_ttf SDL2_image)
/* The launcher's RetroAchievements settings page, with a fake job runner (job.c is not linked). */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"
#include "../../launcher/settings.h"
#include "../../launcher/job.h"

/* fake jobs: every start is recorded, nothing runs */
static char last_cmd[1200], last_input[200], all_cmds[8000];
static int last_kind, has_input;
int job_start(int kind, const char *cmd, const char *input) {
    last_kind = kind;
    snprintf(last_cmd, sizeof last_cmd, "%s", cmd);
    has_input = input != NULL;
    snprintf(last_input, sizeof last_input, "%s", input ? input : "");
    strncat(all_cmds, cmd, sizeof all_cmds - strlen(all_cmds) - 2);
    strcat(all_cmds, "\n");
    return 0;
}
int job_busy(void) { return 0; }
int job_kind(void) { return last_kind; }
int job_take(int *kind, int *rc, const char **out) { (void)kind; (void)rc; (void)out; return 0; }
void sh_quote(char *dst, int n, const char *s) { snprintf(dst, n, "'%s'", s); }

static int find(const char *label) {
    for (int i = 0; i < settings_count(); i++) if (!strcmp(settings_label(i), label)) return i;
    return -1;
}
static void go_to(const char *label) {
    int i = find(label);
    CHECK(i >= 0);
    while (settings_sel() < i) settings_key(NAV_DOWN);
    while (settings_sel() > i) settings_key(NAV_UP);
}
static void open_page(void) {
    settings_open();
    go_to("RetroAchievements");
    settings_key(NAV_A);
    CHECK_STR(settings_title(), "Einstellungen › RetroAchievements");
}

int main(void) {
    char dir[] = "/tmp/mintos-test-ls-XXXXXX";
    CHECK(mkdtemp(dir));
    setenv("MINTOS_CHEEVOS_DIR", dir, 1);
    char tok[300];
    snprintf(tok, sizeof tok, "%s/token", dir);

    /* signed out: only "Anmelden" */
    open_page();
    CHECK(find("Anmelden") >= 0);
    CHECK(find("Abmelden") < 0);
    CHECK(find("Jetzt abgleichen") < 0);

    /* login: user name, then password; the password goes only to the job's stdin */
    go_to("Anmelden");
    settings_key(NAV_A);
    CHECK_STR(settings_title(), "RetroAchievements › Benutzername");
    settings_key(NAV_A);                   /* "q" */
    settings_key(NAV_START);
    CHECK_STR(settings_title(), "RetroAchievements › Passwort");
    settings_key(NAV_RIGHT);               /* "w" x4 */
    for (int i = 0; i < 4; i++) settings_key(NAV_A);
    settings_key(NAV_START);
    CHECK(strstr(last_cmd, "mintos-cheevos login 'q'") != NULL);
    CHECK(strstr(last_cmd, "wwww") == NULL);
    CHECK(strstr(all_cmds, "wwww") == NULL);
    CHECK(has_input);
    CHECK_STR(last_input, "wwww");

    /* signed in */
    FILE *f = fopen(tok, "w");
    CHECK(f);
    fputs("tester\nsecrettoken\n", f);
    fclose(f);
    settings_key(NAV_B);                   /* back to main */
    open_page();
    CHECK(find("Konto: tester") >= 0);
    CHECK(find("Abmelden") >= 0);
    CHECK(find("Anmelden") < 0);
    go_to("Jetzt abgleichen");
    settings_key(NAV_A);
    CHECK(strstr(last_cmd, "mintos-cheevos sync") != NULL);
    CHECK(!has_input);

    /* logout needs a second A */
    go_to("Abmelden");
    last_cmd[0] = 0;
    settings_key(NAV_A);
    CHECK_STR(last_cmd, "");
    settings_key(NAV_A);
    CHECK(strstr(last_cmd, "mintos-cheevos logout") != NULL);

    /* the token was rejected by the server: sign in again */
    snprintf(tok, sizeof tok, "%s/token.invalid", dir);
    f = fopen(tok, "w");
    CHECK(f);
    fclose(f);
    settings_key(NAV_B);
    open_page();
    CHECK(find("Neu anmelden") >= 0);
    TEST_END();
}

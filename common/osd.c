#include "osd.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static long long cur_seq = -1, mark_seq;   /* -1: not read yet (a file from before start is not shown) */
static char cur_kind;
static int cur_val;
static Uint32 until;
static int visible;
static struct timespec seen;
static char toast[128];
static Uint32 toast_until;
static int toast_new;

int osd_parse(const char *s, long long *seq, char *kind, int *value) {
    char k[16];
    if (sscanf(s, "%lld %15s %d", seq, k, value) != 3) return -1;
    if (!strcmp(k, "vol")) *kind = 'v';
    else if (!strcmp(k, "bright")) *kind = 'b';
    else return -1;
    if (*value < 0) *value = 0;
    if (*value > 10) *value = 10;
    return 0;
}

/* re-read the file when it changed; returns 1 if it holds a new change */
static int reread(void) {
    struct stat st;
    if (stat(OSD_FILE, &st)) { if (cur_seq < 0) cur_seq = 0; return 0; }
    if (cur_seq >= 0 && st.st_mtim.tv_sec == seen.tv_sec && st.st_mtim.tv_nsec == seen.tv_nsec) return 0;
    seen = st.st_mtim;
    FILE *f = fopen(OSD_FILE, "r");
    char line[64] = "";
    if (f) { if (!fgets(line, sizeof line, f)) line[0] = 0; fclose(f); }
    long long seq; char kind; int val;
    if (osd_parse(line, &seq, &kind, &val)) { if (cur_seq < 0) cur_seq = 0; return 0; }
    int first = cur_seq < 0, fresh = seq != cur_seq;
    cur_seq = seq; cur_kind = kind; cur_val = val;
    return fresh && !first;
}

void osd_toast(const char *text) {
    snprintf(toast, sizeof toast, "%s", text);
    toast_until = SDL_GetTicks() + OSD_MS * 2;
    toast_new = 1;
}

const char *osd_toast_text(void) {
    return toast[0] && (Sint32)(SDL_GetTicks() - toast_until) < 0 ? toast : "";
}

int osd_poll(void) {
    Uint32 now = SDL_GetTicks();
    int r = 0;
    if (toast_new) { toast_new = 0; r = 1; }
    else if (toast[0] && (Sint32)(now - toast_until) >= 0) { toast[0] = 0; r = 1; }
    if (reread()) { visible = 1; until = now + OSD_MS; return 1; }
    if (visible && (Sint32)(now - until) >= 0) { visible = 0; return 1; }
    return r;
}

void osd_mark(void) { reread(); mark_seq = cur_seq; }

int osd_used_since_mark(void) { reread(); return cur_seq != mark_seq && cur_kind == 'b'; }

/* a bar pill centred just below the top bar: "HELLIGKEIT" / "LAUTSTÄRKE" and 10 steps as in the settings */
static void draw_toast(Ui *ui, int y) {
    if (!toast[0] || !ui->btn) return;
    int tw = ui_text_w(ui->btn, toast), maxw = ui->w - 2 * PAD - 40;
    if (tw > maxw) tw = maxw;
    int w = tw + 40, x = (ui->w - w) / 2;
    ui_pill(ui, x - 3, y - 3, w + 6, HINT + 6, UI_BLACK);
    ui_pill(ui, x, y, w, HINT, UI_BAR);
    ui_text(ui, ui->btn, toast, x + 20, y + HINT / 2, UI_WHITE, tw);
}

/* the volume/brightness bar just below the top bar, a toast below it (or in its place) */
void osd_draw(Ui *ui) {
    int y0 = PAD + HINT + 12;
    draw_toast(ui, visible ? y0 + HINT + 12 : y0);
    if (!visible) return;
    const char *label = cur_kind == 'b' ? "HELLIGKEIT" : "LAUTSTÄRKE";
    int bw = 14, gap = 5, bars = 10 * bw + 9 * gap, lw = ui_text_w(ui->btn, label);
    int w = 20 + lw + 16 + bars + 20, x = (ui->w - w) / 2, y = PAD + HINT + 12, cy = y + HINT / 2;   /* below the top bar */
    ui_pill(ui, x - 3, y - 3, w + 6, HINT + 6, UI_BLACK);   /* black edge: it sits on the list */
    ui_pill(ui, x, y, w, HINT, UI_BAR);
    ui_text(ui, ui->btn, label, x + 20, cy, UI_LIGHT, lw);
    int bx = x + 20 + lw + 16;
    for (int i = 0; i < 10; i++)
        ui_pill(ui, bx + i * (bw + gap), cy - 9, bw, 18, i < cur_val ? UI_WHITE : UI_DARK);
}

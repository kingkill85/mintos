#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#define PSU "/sys/class/power_supply"

const SDL_Color UI_WHITE = {255, 255, 255, 255}, UI_BLACK = {0, 0, 0, 255},
                UI_GRAY = {0x99, 0x99, 0x99, 255}, UI_DARK = {0x26, 0x26, 0x26, 255},
                UI_LIGHT = {0xcc, 0xcc, 0xcc, 255}, UI_MID = {0x80, 0x80, 0x80, 255},
                UI_BAR = {0x44, 0x44, 0x44, 255};

int ui_fonts(Ui *ui, const char *font, const char *fallback) {
    if (TTF_Init() < 0) return -1;
    /* MinUI at 640x480: large 32, ui->small 24, buttons 28 */
    ui->big = TTF_OpenFont(font, 32);
    ui->small = TTF_OpenFont(font, 24);
    ui->btn = TTF_OpenFont(font, 22);
    int bold = 0;
    if (!ui->big || !ui->small || !ui->btn) {
        ui->big = TTF_OpenFont(fallback, 30); ui->small = TTF_OpenFont(fallback, 22); ui->btn = TTF_OpenFont(fallback, 20);
        bold = 1;
    }
    if (!ui->big || !ui->small) return -1;
    if (!ui->btn) ui->btn = ui->small;
    if (bold) { TTF_SetFontStyle(ui->big, TTF_STYLE_BOLD); TTF_SetFontStyle(ui->small, TTF_STYLE_BOLD); TTF_SetFontStyle(ui->btn, TTF_STYLE_BOLD); }
    return 0;
}

void ui_set(Ui *ui, SDL_Color c) { SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, 255); }

/* filled rounded rectangle with fully round ends */
void ui_pill(Ui *ui, int x, int y, int w, int h, SDL_Color c) {
    ui_set(ui, c);
    int r = h / 2;
    for (int dy = 0; dy < h; dy++) {
        float cy = dy + 0.5f - h / 2.0f;
        float dx2 = (float)r * r - cy * cy;
        int inset = dx2 > 0 ? r - (int)(SDL_sqrtf(dx2) + 0.5f) : r;
        SDL_RenderDrawLine(ui->ren, x + inset, y + dy, x + w - 1 - inset, y + dy);
    }
}

int ui_text_w(TTF_Font *f, const char *s) {
    int w = 0;
    TTF_SizeUTF8(f, s, &w, NULL);
    return w;
}

/* draws s vertically centered on cy, cut with "..." to fit maxw; returns drawn width */
int ui_text(Ui *ui, TTF_Font *f, const char *s, int x, int cy, SDL_Color c, int maxw) {
    char buf[160];
    snprintf(buf, sizeof buf, "%s", s);
    if (ui_text_w(f, buf) > maxw) {
        size_t len = strlen(buf);
        while (len > 1) {
            len--;
            while (len > 1 && (buf[len] & 0xc0) == 0x80) len--;  /* keep UTF-8 intact */
            snprintf(buf + len, sizeof buf - len, "...");
            if (ui_text_w(f, buf) <= maxw) break;
        }
    }
    if (!*buf) return 0;
    SDL_Surface *sf = TTF_RenderUTF8_Blended(f, buf, c);
    if (!sf) return 0;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ui->ren, sf);
    SDL_Rect dst = {x, cy - sf->h / 2, sf->w, sf->h};
    SDL_RenderCopy(ui->ren, t, NULL, &dst);
    int w = sf->w;
    SDL_DestroyTexture(t);
    SDL_FreeSurface(sf);
    return w;
}

static int read_int(const char *path) {
    FILE *f = fopen(path, "r");
    int v = -1;
    if (f) { if (fscanf(f, "%d", &v) != 1) v = -1; fclose(f); }
    return v;
}

/* first power supply of type Battery ("battery" on ROCKNIX, "rk817-battery" on mainline) */
static const char *battery_dir(void) {
    static char dir[300];
    if (dir[0]) return dir;
    DIR *d = opendir(PSU);
    struct dirent *de;
    while (d && (de = readdir(d))) {
        char p[300], t[16] = "";
        snprintf(p, sizeof p, PSU "/%s/type", de->d_name);
        FILE *f = fopen(p, "r");
        if (!f) continue;
        if (!fgets(t, sizeof t, f)) t[0] = 0;
        fclose(f);
        if (!strncmp(t, "Battery", 7)) { snprintf(dir, sizeof dir, PSU "/%s", de->d_name); break; }
    }
    if (d) closedir(d);
    return dir;
}

void ui_battery(Ui *ui) {
    char cap[320], stp[320];
    snprintf(cap, sizeof cap, "%s/capacity", battery_dir());
    snprintf(stp, sizeof stp, "%s/status", battery_dir());
    int pct = read_int(cap);
    if (pct < 0) return;
    char st[32] = "";
    FILE *f = fopen(stp, "r");
    if (f) { if (!fgets(st, sizeof st, f)) st[0] = 0; fclose(f); }
    int charging = !strncmp(st, "Charging", 8);
    int cx = ui->w - PAD - HINT, cy = PAD;
    ui_pill(ui, cx, cy, HINT, HINT, UI_BAR);                        /* circle */
    int bw = 26, bh = 14, x = cx + (HINT - bw - 3) / 2, y = cy + (HINT - bh) / 2;
    ui_set(ui, UI_WHITE);
    for (int i = 0; i < 2; i++) { SDL_Rect r = {x + i, y + i, bw - 2 * i, bh - 2 * i}; SDL_RenderDrawRect(ui->ren, &r); }
    SDL_Rect nub = {x + bw, y + 4, 3, bh - 8};
    SDL_RenderFillRect(ui->ren, &nub);
    int fillw = (bw - 8) * (pct > 100 ? 100 : pct) / 100;
    SDL_Color c = pct <= 10 && !charging ? (SDL_Color){0xff, 0x30, 0x30, 255} : UI_WHITE;
    ui_set(ui, charging ? (SDL_Color){0x40, 0xd0, 0x60, 255} : c);
    SDL_Rect lvl = {x + 4, y + 4, fillw, bh - 8};
    SDL_RenderFillRect(ui->ren, &lvl);
}

/* MinUI hint group: one dark pill holding "[button] WORD" pairs. A button is a light
 * circle with a dark letter, or (long names like POWER) a light pill with gray text.
 * Returns the group's width; drawn right-aligned at `right` or left-aligned at `left`. */
int ui_hints(Ui *ui, int left, int right, const char *pairs[][2], int n) {
    int w = 8, bws[4], lws[4];
    for (int i = 0; i < n; i++) {
        int tw = ui_text_w(ui->btn, pairs[i][0]);
        bws[i] = pairs[i][0][1] ? tw + 20 : BTN;
        lws[i] = ui_text_w(ui->btn, pairs[i][1]);
        w += bws[i] + 10 + lws[i] + 14;
    }
    int x = right ? right - w : left, y = ui->h - PAD - HINT, cy = y + HINT / 2;
    ui_pill(ui, x, y, w, HINT, UI_BAR);
    int cx = x + 8;
    for (int i = 0; i < n; i++) {
        const char *b = pairs[i][0];
        ui_pill(ui, cx, cy - BTN / 2, bws[i], BTN, UI_LIGHT);
        TTF_Font *f = ui->btn;
        ui_text(ui, f, b, cx + (bws[i] - ui_text_w(f, b)) / 2, cy, b[1] ? UI_MID : UI_BAR, bws[i]);
        cx += bws[i] + 10;
        ui_text(ui, ui->btn, pairs[i][1], cx, cy, UI_LIGHT, lws[i]);
        cx += lws[i] + 14;
    }
    return w;
}


int ui_title(Ui *ui, const char *s, int maxw) {
    int tw = ui_text_w(ui->btn, s), inner = maxw - 32;
    if (tw > inner) tw = inner;
    ui_pill(ui, PAD, PAD, tw + 32, HINT, UI_BAR);
    ui_text(ui, ui->btn, s, PAD + 16, PAD + HINT / 2, UI_LIGHT, inner);
    return tw + 32;
}

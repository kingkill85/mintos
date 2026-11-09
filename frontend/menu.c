#include "menu.h"
#include "gameopts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void menu_init(Menu *m, bool can_state, int last_slot, bool game_menu) {
    *m = (Menu){.can_state = can_state, .slot = last_slot >= 0 && last_slot < MENU_SLOTS ? last_slot : 0};
    m->items[m->nitems++] = M_CONTINUE;
    if (can_state) { m->items[m->nitems++] = M_SAVE; m->items[m->nitems++] = M_LOAD; }
    else if (game_menu) m->items[m->nitems++] = M_GAMEMENU;
    m->items[m->nitems++] = M_RESET;   /* needs a second A: it ends the current game */
    m->items[m->nitems++] = M_QUIT;
}

void menu_set_options(Menu *m, bool has_frame, const int vals[MO_N]) {
    m->items[m->nitems] = m->items[m->nitems - 1];   /* before Beenden */
    m->items[m->nitems - 1] = M_OPTIONS;
    m->nitems++;
    memcpy(m->val, vals, sizeof m->val);
    m->nopts = 0;
    m->opts[m->nopts++] = MO_SCALE;
    m->opts[m->nopts++] = MO_FILTER;
    if (has_frame) m->opts[m->nopts++] = MO_FRAME;
    m->opts[m->nopts++] = MO_BRIGHT;
}

const char *menu_opt_value(Menu *m, int i) {
    MenuOpt o = m->opts[i];
    if (o == MO_BRIGHT) { snprintf(m->text, sizeof m->text, "%d/10", m->val[o]); return m->text; }
    return gameopt_choice((GameOpt)o, m->val[o]);
}

static const char *opt_label(MenuOpt o) { return o == MO_BRIGHT ? "Helligkeit" : gameopt_label((GameOpt)o); }

static MenuAction opt_key(Menu *m, int id) {
    MenuOpt o = m->opts[m->osel];
    int d = id == RETRO_DEVICE_ID_JOYPAD_RIGHT ? 1 : -1;
    switch (id) {
    case RETRO_DEVICE_ID_JOYPAD_UP: m->osel = (m->osel + m->nopts - 1) % m->nopts; break;
    case RETRO_DEVICE_ID_JOYPAD_DOWN: m->osel = (m->osel + 1) % m->nopts; break;
    case RETRO_DEVICE_ID_JOYPAD_LEFT: case RETRO_DEVICE_ID_JOYPAD_RIGHT:
        if (o == MO_BRIGHT) {
            int v = m->val[o] + d;
            if (v < 1 || v > 10) return M_NONE;
            m->val[o] = v;
        } else {
            int n = gameopt_count((GameOpt)o);
            m->val[o] = (m->val[o] + d + n) % n;
        }
        m->changed = o;
        return M_OPTION;
    case RETRO_DEVICE_ID_JOYPAD_B: m->page = false; break;
    }
    return M_NONE;
}

void menu_set_cheevos(Menu *m, bool on) {
    if (!on) return;
    m->items[m->nitems] = m->items[m->nitems - 1];   /* before Beenden */
    m->items[m->nitems - 1] = M_CHEEVOS;
    m->nitems++;
}

void menu_set_cheevo_list(Menu *m, CheevoItem *l, int n) {
    /* stable: unlocked first, each group in its original order */
    CheevoItem *t = n > 0 ? malloc(sizeof *t * n) : NULL;
    int u = 0;
    if (t) {
        for (int i = 0; i < n; i++) if (l[i].unlocked) t[u++] = l[i];
        int k = u;
        for (int i = 0; i < n; i++) if (!l[i].unlocked) t[k++] = l[i];
        memcpy(l, t, sizeof *t * n);
        free(t);
    } else n = 0;
    m->cl = l; m->ncl = n; m->nunlocked = u; m->csel = 0; m->cpage = n > 0;
}

const char *menu_cheevo_title(const Menu *m, char *buf, size_t n) {
    snprintf(buf, n, "Erfolge %d/%d", m->nunlocked, m->ncl);
    return buf;
}

static MenuAction cheevo_key(Menu *m, int id) {
    int last = m->ncl - 1;
    switch (id) {
    case RETRO_DEVICE_ID_JOYPAD_UP: m->csel = (m->csel + m->ncl - 1) % m->ncl; break;
    case RETRO_DEVICE_ID_JOYPAD_DOWN: m->csel = (m->csel + 1) % m->ncl; break;
    case RETRO_DEVICE_ID_JOYPAD_L: m->csel = m->csel - CHEEVO_ROWS < 0 ? 0 : m->csel - CHEEVO_ROWS; break;
    case RETRO_DEVICE_ID_JOYPAD_R: m->csel = m->csel + CHEEVO_ROWS > last ? last : m->csel + CHEEVO_ROWS; break;
    case RETRO_DEVICE_ID_JOYPAD_B: m->cpage = false; break;
    }
    return M_NONE;
}

bool menu_on_slot_item(const Menu *m) {
    if (m->page || m->cpage) return false;
    return m->items[m->sel] == M_SAVE || m->items[m->sel] == M_LOAD;
}

void menu_set_discs(Menu *m, unsigned count, unsigned index) {
    int at = -1;
    for (int i = 0; i < m->nitems; i++) if (m->items[i] == M_DISC) at = i;
    if (at >= 0) {   /* remove */
        for (int i = at; i < m->nitems - 1; i++) m->items[i] = m->items[i + 1];
        m->nitems--;
        if (m->sel >= m->nitems) m->sel = m->nitems - 1;
    }
    m->discs = count; m->disc = index;
    if (count > 1) {   /* insert before Beenden (the last item) */
        m->items[m->nitems] = m->items[m->nitems - 1];
        m->items[m->nitems - 1] = M_DISC;
        m->nitems++;
    }
}

const char *menu_item_text(Menu *m, int i) {
    if (m->items[i] != M_DISC) return menu_label(m->items[i]);
    snprintf(m->text, sizeof m->text, "Disc wechseln (%u/%u)", m->disc + 1, m->discs);
    return m->text;
}

void menu_failed(Menu *m, MenuAction a) {
    m->msg = a == M_SAVE ? "Speichern fehlgeschlagen" : "Laden fehlgeschlagen";
}

MenuAction menu_key(Menu *m, int id) {
    m->msg = NULL;
    bool armed = m->armed;
    m->armed = false;
    if (m->page) return opt_key(m, id);
    if (m->cpage) return cheevo_key(m, id);
    if (id == RETRO_DEVICE_ID_JOYPAD_A && m->items[m->sel] == M_RESET && !armed) {
        m->armed = true;
        m->msg = "Nochmal A: Spiel neu starten";
        return M_NONE;
    }
    if (id == RETRO_DEVICE_ID_JOYPAD_A && m->items[m->sel] == M_OPTIONS) { m->page = true; m->osel = 0; return M_NONE; }
    if (id == RETRO_DEVICE_ID_JOYPAD_A && m->items[m->sel] == M_CHEEVOS) return M_CHEEVOS;
    switch (id) {
    case RETRO_DEVICE_ID_JOYPAD_UP: m->sel = (m->sel + m->nitems - 1) % m->nitems; break;
    case RETRO_DEVICE_ID_JOYPAD_DOWN: m->sel = (m->sel + 1) % m->nitems; break;
    case RETRO_DEVICE_ID_JOYPAD_LEFT: if (menu_on_slot_item(m)) m->slot = (m->slot + MENU_SLOTS - 1) % MENU_SLOTS; break;
    case RETRO_DEVICE_ID_JOYPAD_RIGHT: if (menu_on_slot_item(m)) m->slot = (m->slot + 1) % MENU_SLOTS; break;
    case RETRO_DEVICE_ID_JOYPAD_A: return m->items[m->sel];
    case RETRO_DEVICE_ID_JOYPAD_B: return M_CONTINUE;
    }
    return M_NONE;
}

const char *menu_label(MenuAction a) {
    switch (a) {
    case M_CONTINUE: return "Weiter";
    case M_SAVE: return "Speichern";
    case M_LOAD: return "Laden";
    case M_GAMEMENU: return "ScummVM-Menü";
    case M_OPTIONS: return "Optionen";
    case M_CHEEVOS: return "Erfolge";
    case M_RESET: return "Neustart";
    case M_QUIT: return "Beenden";
    default: return "";
    }
}

/* text with a soft shadow so it reads over the game picture */
static void shadow_text(Ui *ui, const char *s, int x, int cy, SDL_Color c, int maxw) {
    ui_text(ui, ui->big, s, x + 2, cy + 2, UI_BLACK, maxw);
    ui_text(ui, ui->big, s, x, cy, c, maxw);
}

/* geometry from MinUI's menu (minui-menu-gbc.png at 640x480) */
#define ITEM_Y (PAD + HINT + 16)    /* first item's top, as in the launcher list */
#define PV_X 284
#define PV_Y 100
#define PV_W 336
#define PV_H 276

static void preview_box(Ui *ui, const Menu *m, SDL_Texture *preview) {
    ui_set(ui, UI_WHITE);
    SDL_Rect box = {PV_X, PV_Y, PV_W, PV_H};
    ui_pill(ui, box.x, box.y, box.w, 24, UI_WHITE);                 /* rounded top/bottom edges */
    ui_pill(ui, box.x, box.y + box.h - 24, box.w, 24, UI_WHITE);
    SDL_Rect body = {box.x, box.y + 12, box.w, box.h - 24};
    SDL_RenderFillRect(ui->ren, &body);
    SDL_Rect pic = {PV_X + 8, PV_Y + 8, 320, 240};
    if (preview) {
        SDL_RenderCopy(ui->ren, preview, NULL, &pic);
    } else {
        ui_set(ui, UI_DARK);
        SDL_RenderFillRect(ui->ren, &pic);
        ui_text(ui, ui->small, "Leer", pic.x + (pic.w - ui_text_w(ui->small, "Leer")) / 2, pic.y + pic.h / 2, UI_GRAY, pic.w);
    }
    int cy = PV_Y + 8 + 240 + (PV_H - 8 - 240) / 2, step = 30, x0 = PV_X + PV_W / 2 - step * (MENU_SLOTS - 1) / 2;
    for (int i = 0; i < MENU_SLOTS; i++) {
        int r = i == m->slot ? 7 : 2;
        ui_pill(ui, x0 + i * step - r, cy - r, 2 * r, 2 * r, i == m->slot ? UI_BLACK : UI_MID);
    }
}

/* options page: label left, value right; the selected line is a full-width pill */
static void draw_options(Ui *ui, Menu *m) {
    for (int i = 0; i < m->nopts; i++) {
        int y = ITEM_Y + i * PILL, cy = y + PILL / 2, on = i == m->osel;
        const char *v = menu_opt_value(m, i);
        char sel[64];
        if (on) snprintf(sel, sizeof sel, "< %s >", v);
        const char *vs = on ? sel : v;
        int vw = ui_text_w(ui->small, vs), right = ui->w - PAD - 24;
        if (on) {
            ui_pill(ui, PAD, y, ui->w - 2 * PAD, PILL, UI_WHITE);
            ui_text(ui, ui->big, opt_label(m->opts[i]), PAD + 24, cy, UI_BLACK, ui->w);
            ui_text(ui, ui->small, vs, right - vw, cy, UI_BLACK, vw);
        } else {
            shadow_text(ui, opt_label(m->opts[i]), PAD + 24, cy, UI_WHITE, ui->w);
            ui_text(ui, ui->small, vs, right - vw + 2, cy + 2, UI_BLACK, vw);
            ui_text(ui, ui->small, vs, right - vw, cy, UI_WHITE, vw);
        }
    }
}

/* "+"/"·" (the fonts have no check mark), title left, points right; the description of the
 * selected row sits on the bottom line */
static void draw_cheevos(Ui *ui, Menu *m) {
    int step = 52, first = m->csel / CHEEVO_ROWS * CHEEVO_ROWS;
    for (int r = 0; r < CHEEVO_ROWS && first + r < m->ncl; r++) {
        const CheevoItem *c = &m->cl[first + r];
        int i = first + r, y = ITEM_Y + r * step + (step - PILL) / 2, cy = y + PILL / 2, on = i == m->csel;
        char pts[16], row[96];
        snprintf(pts, sizeof pts, "%u", c->points);
        snprintf(row, sizeof row, "%s %s", c->unlocked ? "+" : "\xC2\xB7", c->title);
        int pw = ui_text_w(ui->small, pts), right = ui->w - PAD - 24, maxw = right - pw - 16 - (PAD + 24);
        if (on) {
            ui_pill(ui, PAD, y, ui->w - 2 * PAD, PILL, UI_WHITE);
            ui_text(ui, ui->big, row, PAD + 24, cy, UI_BLACK, maxw);
            ui_text(ui, ui->small, pts, right - pw, cy, UI_BLACK, pw);
        } else {
            shadow_text(ui, row, PAD + 24, cy, c->unlocked ? UI_WHITE : UI_GRAY, maxw);
            ui_text(ui, ui->small, pts, right - pw + 2, cy + 2, UI_BLACK, pw);
            ui_text(ui, ui->small, pts, right - pw, cy, UI_WHITE, pw);
        }
    }
    const char *d = m->cl[m->csel].desc;
    int dy = ui->h - PAD - HINT - 30;
    ui_text(ui, ui->small, d, PAD + 24 + 2, dy + 2, UI_BLACK, ui->w - 2 * PAD - 48);
    ui_text(ui, ui->small, d, PAD + 24, dy, UI_WHITE, ui->w - 2 * PAD - 48);
}

void menu_draw(Ui *ui, Menu *m, const char *title, SDL_Texture *preview) {
    /* title pill, battery */
    char where[300];
    char ct[48];
    if (m->cpage) snprintf(where, sizeof where, "%s › %s", title, menu_cheevo_title(m, ct, sizeof ct));
    else snprintf(where, sizeof where, m->page ? "%s › Optionen" : "%s", title);
    int right = ui_status(ui);
    ui_title(ui, where, right - 14 - PAD);

    if (m->cpage) {
        draw_cheevos(ui, m);
    } else if (m->page) {
        draw_options(ui, m);
    } else {
        /* six items (with Disc wechseln and Optionen) only fit with tighter rows */
        int step = m->nitems > 5 ? 52 : PILL;
        for (int i = 0; i < m->nitems; i++) {
            const char *s = menu_item_text(m, i);
            int y = ITEM_Y + i * step + (step - PILL) / 2;
            if (i == m->sel) {
                ui_pill(ui, PAD, y, ui_text_w(ui->big, s) + 48, PILL, UI_WHITE);
                ui_text(ui, ui->big, s, PAD + 24, y + PILL / 2, UI_BLACK, ui->w);
            } else {
                shadow_text(ui, s, PAD + 24, y + PILL / 2, UI_WHITE, ui->w);
            }
        }
        if (menu_on_slot_item(m)) preview_box(ui, m, preview);
    }

    if (m->msg) {
        ui_text(ui, ui->small, m->msg, PAD + 24 + 2, ui->h - PAD - HINT - 30 + 2, UI_BLACK, ui->w);
        ui_text(ui, ui->small, m->msg, PAD + 24, ui->h - PAD - HINT - 30, UI_WHITE, ui->w);
    }
    const char *power[][2] = {{"POWER", "AUS"}};
    ui_hints(ui, PAD, 0, power, 1);
    const char *nav[][2] = {{"B", "ZURÜCK"}, {"A", "OK"}};
    ui_hints(ui, 0, ui->w - PAD, nav, m->page || m->cpage ? 1 : 2);
}

#include "menu.h"

void menu_init(Menu *m, bool can_state, int last_slot, bool game_menu) {
    *m = (Menu){.can_state = can_state, .slot = last_slot >= 0 && last_slot < MENU_SLOTS ? last_slot : 0};
    m->items[m->nitems++] = M_CONTINUE;
    if (can_state) { m->items[m->nitems++] = M_SAVE; m->items[m->nitems++] = M_LOAD; }
    else if (game_menu) m->items[m->nitems++] = M_GAMEMENU;
    m->items[m->nitems++] = M_QUIT;
}

bool menu_on_slot_item(const Menu *m) {
    return m->items[m->sel] == M_SAVE || m->items[m->sel] == M_LOAD;
}

void menu_failed(Menu *m, MenuAction a) {
    m->msg = a == M_SAVE ? "Speichern fehlgeschlagen" : "Laden fehlgeschlagen";
}

MenuAction menu_key(Menu *m, int id) {
    m->msg = NULL;
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
    case M_GAMEMENU: return "Speichern / Laden";
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
#define ITEM_Y (PAD + PILL + 20)    /* first item's top */
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

void menu_draw(Ui *ui, const Menu *m, const char *title, SDL_Texture *preview) {
    /* title pill, battery */
    int maxw = ui->w - 2 * PAD - PILL - 10 - 48;
    int tw = ui_text_w(ui->big, title);
    ui_pill(ui, PAD, PAD, (tw < maxw ? tw : maxw) + 48, PILL, UI_DARK);
    ui_text(ui, ui->big, title, PAD + 24, PAD + PILL / 2, UI_WHITE, maxw);
    ui_battery(ui);

    for (int i = 0; i < m->nitems; i++) {
        const char *s = menu_label(m->items[i]);
        int y = ITEM_Y + i * PILL;
        if (i == m->sel) {
            ui_pill(ui, PAD, y, ui_text_w(ui->big, s) + 48, PILL, UI_WHITE);
            ui_text(ui, ui->big, s, PAD + 24, y + PILL / 2, UI_BLACK, ui->w);
        } else {
            shadow_text(ui, s, PAD + 24, y + PILL / 2, UI_WHITE, ui->w);
        }
    }
    if (menu_on_slot_item(m)) preview_box(ui, m, preview);

    if (m->msg) {
        ui_text(ui, ui->small, m->msg, PAD + 24 + 2, ui->h - PAD - HINT - 30 + 2, UI_BLACK, ui->w);
        ui_text(ui, ui->small, m->msg, PAD + 24, ui->h - PAD - HINT - 30, UI_WHITE, ui->w);
    }
    const char *power[][2] = {{"POWER", "AUS"}};
    ui_hints(ui, PAD, 0, power, 1);
    const char *nav[][2] = {{"B", "ZURÜCK"}, {"A", "OK"}};
    ui_hints(ui, 0, ui->w - PAD, nav, 2);
}

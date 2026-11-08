#include "keyboard.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *LETTERS[KB_ROWS - 1] = {"1234567890", "qwertzuiop", "asdfghjkl-", "yxcvbnm._@"};
static const char *SYMBOLS[KB_ROWS - 1] = {"1234567890", "!\"#$%&'()*", "+,/:;<=>?[", "\\]^`{|}~_@"};
static const char *WIDE[4] = {"Aa", "#+=", "Leer", "OK"};

void kb_init(Kb *k) { memset(k, 0, sizeof *k); k->row = 1; }

int kb_cols(int row) { return row == KB_ROWS - 1 ? 4 : KB_COLS; }

static char ch(Kb *k, int row, int col) {
    char c = (k->sym ? SYMBOLS : LETTERS)[row][col];
    return k->upper && !k->sym ? toupper((unsigned char)c) : c;
}

const char *kb_label(Kb *k, int row, int col) {
    if (row == KB_ROWS - 1) return WIDE[col];
    k->lbl[0] = ch(k, row, col); k->lbl[1] = 0;
    return k->lbl;
}

static void put(Kb *k, char c) {
    size_t n = strlen(k->text);
    if (n < KB_MAX) { k->text[n] = c; k->text[n + 1] = 0; }
}

int kb_key(Kb *k, int key) {
    switch (key) {
    case KB_UP: case KB_DOWN: {
        int old = kb_cols(k->row);
        k->row = (k->row + (key == KB_UP ? KB_ROWS - 1 : 1)) % KB_ROWS;
        int n = kb_cols(k->row);
        if (n != old) k->col = k->col * n / old + (old < n ? 1 : 0);   /* wide key -> the keys under it */
        if (k->col >= n) k->col = n - 1;
        break;
    }
    case KB_LEFT: k->col = (k->col + kb_cols(k->row) - 1) % kb_cols(k->row); break;
    case KB_RIGHT: k->col = (k->col + 1) % kb_cols(k->row); break;
    case KB_B: {
        size_t n = strlen(k->text);
        if (!n) return KB_CANCEL;
        k->text[n - 1] = 0;
        break;
    }
    case KB_X: k->upper = !k->upper; break;
    case KB_Y: put(k, ' '); break;
    case KB_SELECT: k->sym = !k->sym; break;
    case KB_START: return KB_DONE;
    case KB_A:
        if (k->row < KB_ROWS - 1) { put(k, ch(k, k->row, k->col)); break; }
        switch (k->col) {
        case 0: k->upper = !k->upper; break;
        case 1: k->sym = !k->sym; break;
        case 2: put(k, ' '); break;
        case 3: return KB_DONE;
        }
        break;
    }
    return KB_NONE;
}

/* prompt + text field, then the key grid */
void kb_draw(Ui *ui, Kb *k, const char *prompt, int y0) {
    ui_text(ui, ui->btn, prompt, PAD + 8, y0 + 14, UI_GRAY, ui->w - 2 * PAD);
    ui_pill(ui, PAD, y0 + 32, ui->w - 2 * PAD, 48, UI_DARK);
    char shown[KB_MAX + 2];
    snprintf(shown, sizeof shown, "%s_", k->text);
    int tw = ui_text_w(ui->small, shown), maxw = ui->w - 2 * PAD - 40;
    /* keep the end (the cursor) visible */
    const char *s = shown;
    while (tw > maxw && *s) { s++; tw = ui_text_w(ui->small, s); }
    ui_text(ui, ui->small, s, PAD + 20, y0 + 56, UI_WHITE, maxw);
    int gy = y0 + 92, kh = 42, gap = 4, kw = (ui->w - 2 * PAD - (KB_COLS - 1) * gap) / KB_COLS;
    for (int r = 0; r < KB_ROWS; r++) {
        int n = kb_cols(r), w = n == KB_COLS ? kw : (ui->w - 2 * PAD - (n - 1) * gap) / n;
        for (int c = 0; c < n; c++) {
            int x = PAD + c * (w + gap), y = gy + r * (kh + gap), on = r == k->row && c == k->col;
            SDL_Color bg = on ? UI_WHITE : UI_DARK, fg = on ? UI_BLACK : UI_WHITE;
            int active = r == KB_ROWS - 1 && ((c == 0 && k->upper) || (c == 1 && k->sym));
            if (active && !on) bg = UI_MID;
            ui_pill(ui, x, y, w, kh, bg);
            const char *l = kb_label(k, r, c);
            TTF_Font *f = r == KB_ROWS - 1 ? ui->btn : ui->small;
            ui_text(ui, f, l, x + (w - ui_text_w(f, l)) / 2, y + kh / 2, fg, w);
        }
    }
}

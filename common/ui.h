/* MinUI-style drawing shared by rglauncher and rgarch (640x480, MinUI's 2x geometry). */
#pragma once
#include <SDL.h>
#include <SDL_ttf.h>

#define PAD 20
#define PILL 60
#define HINT 44   /* top and bottom bar height */
#define BTN 32

typedef struct { SDL_Renderer *ren; int w, h; TTF_Font *big, *small, *btn; } Ui;

extern const SDL_Color UI_WHITE, UI_BLACK, UI_GRAY, UI_DARK, UI_LIGHT, UI_MID, UI_BAR;

/* BPreplay at 32/24/28; fallback font (bold style) if it is missing; 0 ok */
int  ui_fonts(Ui *ui, const char *font, const char *fallback);
void ui_set(Ui *ui, SDL_Color c);
void ui_pill(Ui *ui, int x, int y, int w, int h, SDL_Color c);       /* round-ended box */
int  ui_text_w(TTF_Font *f, const char *s);
/* text vertically centred on cy, cut with "..." to maxw; returns drawn width */
int  ui_text(Ui *ui, TTF_Font *f, const char *s, int x, int cy, SDL_Color c, int maxw);
/* hint group "[button] WORD"...: left-aligned at left, or right-aligned at right if right != 0 */
int  ui_hints(Ui *ui, int left, int right, const char *pairs[][2], int n);
void ui_battery(Ui *ui);                                                 /* circle, top right */
/* top bar: where we are (small text in a bar pill, top left); returns its width */
int  ui_title(Ui *ui, const char *s, int maxw);

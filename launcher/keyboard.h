/* On-screen keyboard for passwords: A types, B deletes (on empty: back), X upper/lower case,
 * Y space, SELECT symbols, START done. ASCII only. */
#pragma once
#include "ui.h"

#define KB_ROWS 5
#define KB_COLS 10
#define KB_MAX 63

enum { KB_UP, KB_DOWN, KB_LEFT, KB_RIGHT, KB_A, KB_B, KB_X, KB_Y, KB_START, KB_SELECT };
enum { KB_NONE, KB_DONE, KB_CANCEL };

typedef struct { char text[KB_MAX + 1]; int row, col, upper, sym, mask; char lbl[8]; } Kb;

void kb_init(Kb *k);
int kb_key(Kb *k, int key);
const char *kb_label(Kb *k, int row, int col);
int kb_cols(int row);
void kb_draw(Ui *ui, Kb *k, const char *prompt, int y0);

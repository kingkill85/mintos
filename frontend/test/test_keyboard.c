// uses: launcher/keyboard.c common/ui.c
// libs: $(pkg-config --libs sdl2 SDL2_ttf SDL2_image)
#include <string.h>
#include "check.h"
#include "../../launcher/keyboard.h"

static void type(Kb *k, int row, int col) { k->row = row; k->col = col; CHECK_INT(kb_key(k, KB_A), KB_NONE); }

int main(void) {
    Kb k;
    kb_init(&k);
    CHECK_STR(k.text, "");
    CHECK_INT(k.row, 1); CHECK_INT(k.col, 0);       /* starts on "q" */
    CHECK_STR(kb_label(&k, 1, 0), "q");
    CHECK_STR(kb_label(&k, 0, 0), "1");
    type(&k, 1, 0);
    CHECK_STR(k.text, "q");
    /* X: upper case for the following keys */
    kb_key(&k, KB_X);
    CHECK_STR(kb_label(&k, 1, 0), "Q");
    type(&k, 1, 1);
    CHECK_STR(k.text, "qW");
    /* Y: space, B: delete the last character (UTF-8 safe not needed: ASCII only) */
    kb_key(&k, KB_Y);
    CHECK_STR(k.text, "qW ");
    kb_key(&k, KB_B); kb_key(&k, KB_B);
    CHECK_STR(k.text, "q");
    CHECK_INT(kb_key(&k, KB_B), KB_NONE);
    CHECK_STR(k.text, "");
    CHECK_INT(kb_key(&k, KB_B), KB_CANCEL);          /* B on an empty text leaves */
    kb_init(&k); kb_key(&k, KB_X);
    /* SELECT: symbols layer */
    kb_key(&k, KB_SELECT);
    CHECK_STR(kb_label(&k, 1, 0), "!");
    type(&k, 1, 0);
    CHECK_STR(k.text, "!");
    kb_key(&k, KB_SELECT);                           /* back to letters (upper still on) */
    CHECK_STR(kb_label(&k, 1, 0), "Q");

    /* moving wraps; the bottom row has 4 wide keys: Aa, #+=, Leer, OK */
    k.row = 0; k.col = 0;
    kb_key(&k, KB_LEFT);
    CHECK_INT(k.col, 9);
    kb_key(&k, KB_UP);
    CHECK_INT(k.row, KB_ROWS - 1);
    CHECK_INT(k.col, 3);                             /* column clamped into the 4 wide keys */
    CHECK_STR(kb_label(&k, KB_ROWS - 1, 3), "OK");
    kb_key(&k, KB_DOWN);
    CHECK_INT(k.row, 0);
    CHECK_INT(k.col, 8);                             /* under "OK": 3 * 10 / 4 + 1 */
    /* wide keys act like the buttons */
    k.row = KB_ROWS - 1; k.col = 2;
    kb_key(&k, KB_A);
    CHECK_STR(k.text, "! ");
    k.col = 0; kb_key(&k, KB_A);                     /* Aa toggles case */
    CHECK_STR(kb_label(&k, 1, 0), "q");
    k.col = 3;
    CHECK_INT(kb_key(&k, KB_A), KB_DONE);
    CHECK_INT(kb_key(&k, KB_START), KB_DONE);
    /* length limit (WPA: 63) */
    kb_init(&k);
    for (int i = 0; i < 100; i++) type(&k, 1, 0);
    CHECK_INT((int)strlen(k.text), KB_MAX);
    TEST_END();
}

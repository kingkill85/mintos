// uses: -DRC_DISABLE_LUA -Ithird_party/rcheevos/include -Ithird_party/rcheevos/src third_party/rcheevos/src/*.c third_party/rcheevos/src/*/*.c
#include <string.h>
#include <stdlib.h>
#include "check.h"
#include "rc_client.h"
#include "rc_hash.h"
#include "rc_consoles.h"

int main(void) {
    /* iNES: "NES\x1a" + 12 bytes header, then 8192 zero bytes; NES hash skips the 16-byte header */
    static unsigned char buf[16 + 8192];
    memcpy(buf, "NES\x1a", 4);
    char h[33] = {0};
    CHECK_INT(rc_hash_generate_from_buffer(h, RC_CONSOLE_NINTENDO, buf, sizeof buf), 1);
    CHECK_STR(h, "0829f71740aab1ab98b33eae21dee122");

    rc_client_t *c = rc_client_create(NULL, NULL);
    CHECK(c != NULL);
    if (c) rc_client_destroy(c);
    TEST_END();
}

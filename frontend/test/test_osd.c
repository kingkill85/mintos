// uses: common/osd.c common/ui.c
// libs: $(pkg-config --libs sdl2 SDL2_ttf)
/* osd_parse: the line mintosd writes to /run/mintos-osd */
#include "check.h"
#include "osd.h"

int main(void) {
    long long seq; char k; int v;
    CHECK(osd_parse("1731150000123 vol 7\n", &seq, &k, &v) == 0 && seq == 1731150000123LL && k == 'v' && v == 7);
    CHECK(osd_parse("5 bright 10", &seq, &k, &v) == 0 && k == 'b' && v == 10);
    CHECK(osd_parse("5 bright 14", &seq, &k, &v) == 0 && v == 10);   /* clamped */
    CHECK(osd_parse("5 bright -3", &seq, &k, &v) == 0 && v == 0);
    CHECK(osd_parse("5 mute 1", &seq, &k, &v) != 0);
    CHECK(osd_parse("", &seq, &k, &v) != 0);
    CHECK(osd_parse("garbage", &seq, &k, &v) != 0);
    TEST_END();
}

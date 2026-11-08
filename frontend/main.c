/* rgarch: libretro frontend with a MinUI-style menu (see docs/superpowers/specs/2026-11-08-rgarch-design.md). */
#include <stdio.h>

int main(int argc, char **argv) {
    (void)argv;
    if (argc < 3) {
        fprintf(stderr, "usage: rgarch [--resume] <system> <rom>\n");
        return 2;
    }
    return 0;
}

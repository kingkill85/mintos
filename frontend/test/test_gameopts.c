// uses: frontend/config.c frontend/gameopts.c
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"
#include "gameopts.h"

static char dir[] = "/tmp/rgarch-test-XXXXXX", sys[64], user[64], frames[64];

static void load(const char *system, SystemCfg *c) {
    config_load(sys, system, c);
    config_merge(user, system, c);
    config_frame(frames, user, system, c);
}

int main(void) {
    if (!mkdtemp(dir)) return 1;
    snprintf(sys, sizeof sys, "%s/systems.cfg", dir);
    snprintf(user, sizeof user, "%s/user.cfg", dir);
    snprintf(frames, sizeof frames, "%s/frames.cfg", dir);
    FILE *f = fopen(sys, "w");
    fputs("[default]\nscale = integer\n[gbc]\nscale = custom 80 12 480 432\noverlay = rgc/J.png\n[nes]\nscale = full\n[gb]\n"
          "[snes]\nscale = full\ncrt = yes\n", f);
    fclose(f);
    f = fopen(frames, "w");
    fputs("[gbc]\nJeltron = rgc/J.png 80 12 480 432\nGBC Raster = GB-GBC/GBC_grid.png 80 24 480 432\n"
          "[nes]\nNES 1 = tv/nes-1.png 64 0 512 480\n[md]\nRot = @schwarz-rot\nKaputt = @gibtsnicht\n[snes]\nSNES schwarz = @snes-schwarz\n", f);
    fclose(f);

    CHECK_STR(gameopt_label(GO_SCALE), "Bild");
    CHECK_STR(gameopt_label(GO_FILTER), "Filter");
    CHECK_STR(gameopt_label(GO_FRAME), "Rahmen");
    CHECK_INT(gameopt_count(GO_SCALE), 4);
    CHECK_STR(gameopt_choice(GO_SCALE, 0), "Standard");
    CHECK_STR(gameopt_choice(GO_SCALE, 3), "Vollbild");
    CHECK_STR(gameopt_choice(GO_FILTER, 2), "Scharf");

    /* Rahmen: offered when systems.cfg has a frame or frames.cfg lists some */
    CHECK(gameopt_has_frame(sys, "gbc"));
    CHECK_INT(gameopt_count(GO_FRAME), 6);   /* Standard, Aus, Jeltron, GBC Raster, 2 drawn */
    CHECK_STR(gameopt_choice(GO_FRAME, 0), "Standard");
    CHECK_STR(gameopt_choice(GO_FRAME, 1), "Aus");
    CHECK_STR(gameopt_choice(GO_FRAME, 3), "GBC Raster");
    CHECK(gameopt_has_frame(sys, "nes"));
    CHECK_INT(gameopt_count(GO_FRAME), 4);   /* no default frame: Aus, NES 1, 2 drawn */
    CHECK_STR(gameopt_choice(GO_FRAME, 2), "Schlicht schwarz");
    CHECK_STR(gameopt_choice(GO_FRAME, 0), "Aus");
    CHECK_STR(gameopt_choice(GO_FRAME, 1), "NES 1");
    CHECK(gameopt_has_frame(sys, "gb"));   /* the drawn frames are there for every system */
    CHECK_INT(gameopt_count(GO_FRAME), 3);

    /* a drawn frame keeps the picture as it is and only sets the border style */
    {
        SystemCfg d;
        CHECK_INT(gameopt_set(user, "gb", GO_FRAME, 2), 0);   /* Schlicht grau */
        load("gb", &d);
        CHECK_INT(d.border, 2);
        CHECK_INT(d.scale, SCALE_INTEGER);
        CHECK_STR(d.overlay, "");
        CHECK_INT(gameopt_get(user, "gb", GO_FRAME), 2);
        CHECK_INT(gameopt_set(user, "gb", GO_FRAME, 0), 0);
        load("gb", &d);
        CHECK_INT(d.border, 0);
    }
    {   /* "@style" in frames.cfg: a drawn frame, the picture keeps its size */
        SystemCfg d;
        CHECK(gameopt_has_frame(sys, "md"));
        CHECK_STR(gameopt_choice(GO_FRAME, 1), "Rot");
        CHECK_INT(gameopt_set(user, "md", GO_FRAME, 1), 0);
        load("md", &d);
        CHECK(config_border(d.border) != NULL);
        CHECK_STR(config_border(d.border)->name, "schwarz-rot");
        CHECK_INT(d.scale, SCALE_INTEGER);
        CHECK_INT(gameopt_set(user, "md", GO_FRAME, 2), 0);   /* unknown style: no frame */
        load("md", &d);
        CHECK_INT(d.border, 0);
    }

    /* nothing set = Standard; set writes user.cfg, Standard removes the key again */
    SystemCfg c;
    CHECK_INT(gameopt_get(user, "gbc", GO_SCALE), 0);
    CHECK_INT(gameopt_set(user, "gbc", GO_SCALE, 2), 0);
    CHECK_INT(gameopt_set(user, "gbc", GO_FRAME, 1), 0);
    CHECK_INT(gameopt_get(user, "gbc", GO_SCALE), 2);
    CHECK_INT(gameopt_get(user, "gbc", GO_FRAME), 1);
    load("gbc", &c);
    CHECK_INT(c.scale, SCALE_ASPECT);
    CHECK_STR(c.overlay, "");
    CHECK_INT(gameopt_set(user, "gbc", GO_SCALE, 0), 0);
    CHECK_INT(gameopt_set(user, "gbc", GO_FRAME, 0), 0);
    load("gbc", &c);
    CHECK_INT(c.scale, SCALE_CUSTOM);
    CHECK(strstr(c.overlay, "rgc/J.png") != NULL);
    CHECK_INT(gameopt_get(user, "gbc", GO_SCALE), 0);

    /* a chosen frame brings its PNG and the picture rectangle */
    CHECK_INT(gameopt_set(user, "gbc", GO_FRAME, 3), 0);
    CHECK_INT(gameopt_get(user, "gbc", GO_FRAME), 3);
    load("gbc", &c);
    CHECK(strstr(c.overlay, "GB-GBC/GBC_grid.png") != NULL);
    CHECK_INT(c.scale, SCALE_CUSTOM);
    CHECK_INT(c.cx, 80); CHECK_INT(c.cy, 24); CHECK_INT(c.cw, 480); CHECK_INT(c.ch, 432);
    CHECK_INT(gameopt_set(user, "nes", GO_FRAME, 1), 0);
    load("nes", &c);
    CHECK(strstr(c.overlay, "tv/nes-1.png") != NULL);
    CHECK_INT(c.cx, 64); CHECK_INT(c.cw, 512); CHECK_INT(c.ch, 480);
    CHECK_INT(gameopt_set(user, "nes", GO_FRAME, 0), 0);   /* Aus = nothing stored */
    load("nes", &c);
    CHECK_STR(c.overlay, "");
    CHECK_INT(c.scale, SCALE_FULL);

    /* older user.cfg "overlay = none" reads as Aus; an unknown frame label as Standard */
    config_set(user, "gbc", "frame", NULL);
    config_set(user, "gbc", "overlay", "none");
    CHECK_INT(gameopt_get(user, "gbc", GO_FRAME), 1);
    config_set(user, "gbc", "overlay", NULL);
    config_set(user, "gbc", "frame", "gone");
    CHECK_INT(gameopt_get(user, "gbc", GO_FRAME), 0);
    load("gbc", &c);
    CHECK(strstr(c.overlay, "rgc/J.png") != NULL);

    /* an unknown value in user.cfg reads as Standard */
    config_set(user, "gbc", "filter", "weird");
    CHECK_INT(gameopt_get(user, "gbc", GO_FILTER), 0);
    /* Scanlines set Bild to Ganzzahlig; leaving them sets it back to Standard */
    CHECK_INT(gameopt_set(user, "snes", GO_FILTER, 4), 0);
    CHECK_STR(gameopt_choice(GO_FILTER, 4), "Scanlines");
    CHECK_INT(gameopt_get(user, "snes", GO_SCALE), 1);
    CHECK_INT(gameopt_set(user, "snes", GO_FILTER, 4), 0);         /* same filter again: Bild untouched */
    CHECK_INT(gameopt_get(user, "snes", GO_SCALE), 1);
    CHECK_INT(gameopt_set(user, "snes", GO_FILTER, 2), 0);
    CHECK_INT(gameopt_get(user, "snes", GO_SCALE), 0);
    CHECK_INT(gameopt_get(user, "snes", GO_FILTER), 2);
    /* the rules that keep Bild, Filter and Rahmen consistent */
    {
        unlink(user);
        SystemCfg d;
        /* Scanlines only where crt = yes */
        gameopt_get(user, "gbc", GO_FILTER);
        CHECK_INT(gameopt_count(GO_FILTER), 4);
        gameopt_get(user, "snes", GO_FILTER);
        CHECK_INT(gameopt_count(GO_FILTER), 5);
        /* a drawn frame on a full-screen system brings Ganzzahlig (else it would be invisible) */
        CHECK_INT(gameopt_set(user, "snes", GO_FRAME, 1), 0);   /* SNES schwarz */
        CHECK_INT(gameopt_get(user, "snes", GO_SCALE), 1);
        /* Vollbild leaves no border: the drawn frame goes off */
        CHECK_INT(gameopt_set(user, "snes", GO_SCALE, 3), 0);
        CHECK_INT(gameopt_get(user, "snes", GO_FRAME), 0);
        /* Standard Bild of snes is full screen as well */
        CHECK_INT(gameopt_set(user, "snes", GO_FRAME, 1), 0);
        CHECK_INT(gameopt_set(user, "snes", GO_SCALE, 0), 0);
        CHECK_INT(gameopt_get(user, "snes", GO_FRAME), 0);
        /* Seitenverhaeltnis keeps a drawn frame */
        CHECK_INT(gameopt_set(user, "snes", GO_FRAME, 1), 0);
        CHECK_INT(gameopt_set(user, "snes", GO_SCALE, 2), 0);
        CHECK_INT(gameopt_get(user, "snes", GO_FRAME), 1);
        /* Scanlines: Ganzzahlig; another Bild ends them */
        CHECK_INT(gameopt_set(user, "snes", GO_FILTER, 4), 0);
        CHECK_INT(gameopt_get(user, "snes", GO_SCALE), 1);
        CHECK_INT(gameopt_set(user, "snes", GO_SCALE, 2), 0);
        CHECK_INT(gameopt_get(user, "snes", GO_FILTER), 0);
        /* leaving Scanlines keeps Ganzzahlig while a drawn frame needs the border */
        CHECK_INT(gameopt_set(user, "snes", GO_FILTER, 4), 0);
        CHECK_INT(gameopt_get(user, "snes", GO_FRAME), 1);
        CHECK_INT(gameopt_set(user, "snes", GO_FILTER, 2), 0);
        CHECK_INT(gameopt_get(user, "snes", GO_SCALE), 1);
        /* a PNG frame places the picture: Bild back to Standard; another Bild turns it off */
        CHECK_INT(gameopt_set(user, "gbc", GO_SCALE, 3), 0);
        CHECK_INT(gameopt_set(user, "gbc", GO_FRAME, 3), 0);   /* GBC Raster */
        CHECK_INT(gameopt_get(user, "gbc", GO_SCALE), 0);
        CHECK_INT(gameopt_set(user, "gbc", GO_SCALE, 1), 0);
        CHECK_INT(gameopt_get(user, "gbc", GO_FRAME), 1);      /* Aus */
        /* Standard frame (systems.cfg's PNG) counts as a PNG frame too */
        CHECK_INT(gameopt_set(user, "gbc", GO_FRAME, 0), 0);
        CHECK_INT(gameopt_get(user, "gbc", GO_SCALE), 0);
        /* frame off: the frame's position is dropped, the picture is centred */
        CHECK_INT(gameopt_set(user, "gbc", GO_FRAME, 1), 0);
        load("gbc", &d);
        config_finish(&d, 640, 480);
        CHECK_INT(d.cx, 80); CHECK_INT(d.cy, 24); CHECK_INT(d.ch, 432);
        /* Scanlines from an old user.cfg on a handheld are not used */
        config_set(user, "gbc", "filter", "scanlines");
        load("gbc", &d);
        config_finish(&d, 640, 480);
        CHECK(d.filter != FILTER_SCANLINES);
    }
    unlink(sys); unlink(user); unlink(frames); rmdir(dir);
    TEST_END();
}

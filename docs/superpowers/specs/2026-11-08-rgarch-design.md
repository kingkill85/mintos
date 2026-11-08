# rgarch — eigenes Emulator-Frontend mit MinUI-Menü

Stand: 2026-11-08 · Gerät: Anbernic RG353V, eigenes Debian-System (siehe README)

## Ziel

Spielen soll sich anfühlen wie MinUI: Im normalen Gebrauch ist RetroArch nie zu sehen.
Ein Spiel startet direkt aus der Liste, **F** öffnet ein Menü, das aussieht wie MinUIs
Menü (siehe `~/rg353-build/minui-ref/minui-menu-gbc.png`), und nach dem Beenden ist man
sofort wieder in der Liste.

### Was der Nutzer will (gesagt)

- Menü im Spiel wie MinUI, nur das Normale: Weiter, Speichern, Laden, Beenden, nahtlos.
- RetroArch soll bei normalen Interaktionen bei **keinem** System sichtbar sein.
- Menüs nur über F, keine anderen Menütasten.
- Spielstände in eigenen Ordnern, nicht neben den Spielen.

### Annahmen (von mir, korrigierbar)

- Kein Optionen-Untermenü. Bild- und Core-Einstellungen bleiben fest pro System.
- Die jetzigen libretro-Cores (buildbot, `/storage/.config/retroarch/cores`) werden
  weiterverwendet, ebenso ihre Core-Optionen (`.opt`).
- RetroArch bleibt installiert, bis Phase 2 fertig ist, wird danach entfernt.

### Erfolgskriterien

1. Jedes System aus der Liste startet in rgarch. Bild, Ton und Tasten funktionieren, die
   Bildeinstellungen pro System entsprechen dem heutigen Stand.
2. F öffnet das MinUI-Menü. Weiter, Speichern und Laden (9 Plätze mit Vorschau) sowie
   Beenden funktionieren. Beenden führt in unter 2 s zurück zur Liste.
3. Power-Taste im Spiel speichert automatisch und setzt beim Einschalten fort.
4. Spielstände liegen unter `/storage/saves/<system>/`, Speicherstände unter
   `/storage/states/<system>/`. Vorhandene Dateien sind dorthin migriert.
5. Kein Knacken im Ton, kein Ruckeln bei 2D-Systemen (60 fps bei GB bis PS1).

## Architektur

```
rglauncher ──exec──> launch.sh ──exec──> rgarch <core.so> <rom> [--resume]
                                          │
          ┌──────────────┬────────────────┼───────────────┬──────────────┐
        core.c         video.c          audio.c         input.c        menu.c
     (libretro-Host) (EGL/GLES, Skal.,  (SDL-Audio,    (Gamepad ->    (MinUI-Optik,
                      Overlay, Shader)   Resampling)    RetroPad, F)   Slots, Vorschau)
          └──────────────┴──────── state.c (SRAM, Savestates, Resume) ──────┘
                                   config.c (Systemeinstellungen)
```

C, SDL2 (KMSDRM, GameController, Audio), OpenGL ES 3 über SDL. Ein Prozess pro Spiel.
Quellcode unter `frontend/` im Repo, Build per `zig cc` wie der Launcher, gegen die
Debian-Bibliotheken des Geräts.

### Einheiten

- **core.c — libretro-Host.** Lädt den Core per `dlopen`, ruft `retro_init`,
  `retro_load_game`, `retro_run`. Beantwortet die Environment-Aufrufe:
  System-/Save-Verzeichnisse, Pixelformat, Geometrie und Timing, Core-Optionen (liest und
  schreibt die bestehende `.opt`-Datei des Cores), Variablen-Updates, Tastatur-Callback
  (ScummVM), Logging, HW-Render (Phase 2). Unbekannte Aufrufe werden mit „nicht
  unterstützt“ beantwortet und geloggt.
- **video.c — Bild.** Ein Vollbild-Fenster mit GLES-Kontext. Software-Bilder werden als
  Textur hochgeladen (RGB565/XRGB8888/0RGB1555). HW-Bilder (Phase 2) werden aus dem FBO
  des Cores übernommen. Skalierungsmodi: `integer`, `aspect` (Core-Seitenverhältnis),
  `full`, `custom x,y,w,h`. Filter `nearest` oder `sharp` (eigener kleiner Shader nach Art
  von sharp-bilinear). Overlay-PNG optional über dem Bild (die heutigen GB/GBC/GG-Rahmen).
  Darstellung mit Vsync.
- **audio.c — Ton.** SDL-Audio mit 48 kHz Stereo. Die Samples des Cores laufen über
  `SDL_AudioStream` in die Ausgabe-Rate. Taktung: Ein Frame wird erst gerechnet, wenn die
  Audio-Warteschlange unter ein Ziel (ca. 64 ms) fällt. Zusammen mit Vsync hält das das
  Tempo ohne Knacken.
- **input.c — Tasten.** SDL GameController auf „RG353V Gamepad“, abgebildet auf die
  libretro-RetroPad-Tasten und beide Analogsticks (Nintendo-Belegung: A rechts, B unten).
  F (Guide-Taste des Controllers) öffnet das Menü. Tastatur-Events (ScummVM-Mapper) gehen
  an den Core-Callback.
- **menu.c — Menü.** Pausiert das Spiel, zeigt den letzten Frame abgedunkelt.
  - Oben links eine weiße Pille mit dem Spielnamen, oben rechts die Akku-Pille.
  - Einträge: Weiter, Speichern, Laden, Beenden. Ohne Savestate-Fähigkeit des Cores
    (z. B. ScummVM) nur Weiter und Beenden.
  - Bei Speichern und Laden rechts das Vorschaubild des gewählten Platzes mit 9
    Platz-Punkten. Links/Rechts wechselt den Platz, A führt aus.
  - Unten Hinweisleiste wie im Launcher: „POWER AUS“ sowie „B ZURÜCK · A OK“.
  - Gleiche Schrift (BPreplay Bold), Farben und Maße wie im Launcher. Gemeinsame
    Zeichenfunktionen kommen nach `common/ui.c`, das Launcher und rgarch nutzen.
- **state.c — Speichern.**
  - SRAM: beim Start laden, beim Beenden und alle 60 s speichern, wenn geändert.
  - Savestates: `retro_serialize` in eine Datei, dazu ein Vorschaubild (PNG, halbe
    Auflösung).
  - Auto-State für die Power-Taste: Bei SIGTERM wird er geschrieben und sauber beendet.
    `--resume` lädt ihn direkt nach dem Spielstart.
- **config.c — Systemeinstellungen.** Eine Datei `/storage/.config/rgarch/systems.cfg`
  mit einem Abschnitt pro System: Core, Skalierung, Filter, Overlay. Sie ersetzt die
  Core-Zuordnung in `launch.sh` und die RetroArch-Overrides. Die Werte entsprechen dem
  heutigen Stand.

## Ablage

| Was | Wo |
|---|---|
| Spielstand (SRAM) | `/storage/saves/<system>/<Spielname>.srm` |
| Speicherstand Platz n | `/storage/states/<system>/<Spielname>.st<n>` + `.st<n>.png` |
| Auto-State (Power) | `/storage/states/<system>/<Spielname>.auto` |
| Core-Optionen | `/storage/.config/retroarch/config/<Core>/<Core>.opt` (unverändert) |
| BIOS | `/storage/roms/bios` (unverändert) |
| Log | `/var/log/rgarch.log` (pro Start überschrieben) |

`<system>` ist der Ordnername unter `/storage/roms`. Eine einmalige Migration verschiebt
die vorhandenen `.srm`, `.state*` und `.state.auto` aus den Spieleordnern dorthin.

## Abläufe

- **Start:** Der Launcher führt `launch.sh` aus. `launch.sh` liest Core und Einstellungen
  aus `systems.cfg` und startet rgarch. rgarch übernimmt das Display, ohne es vorher kurz
  zu öffnen und wieder zu schließen (Lehre aus dem Panel-Problem).
- **Beenden:** Über Menü „Beenden“: SRAM schreiben, Prozess endet, systemd startet den
  Launcher neu (wie heute).
- **Power-Taste:** `rg353-power` bleibt. Es schreibt die Resume-Datei, stoppt den Dienst
  (SIGTERM, rgarch schreibt den Auto-State) und schaltet aus. Beim Booten startet der
  Launcher das Spiel mit `--resume`.

## Fehlerbehandlung

- Core lädt nicht oder Spiel lässt sich nicht laden: Fehlerseite im MinUI-Stil
  („Spiel konnte nicht gestartet werden“) und nach Tastendruck zurück zur Liste. Details
  stehen im Log.
- Speichern scheitert: kurze Meldung im Menü, das Spiel läuft weiter.
- Ein Core stürzt ab: Der Prozess endet und der Launcher kommt zurück. Das SRAM ist
  höchstens 60 s alt.

## Phasen

1. **Software-Cores:** alle Systeme bis PS1 sowie ScummVM, Arcade und Atari. Danach läuft
   alles, was heute auf der Karte liegt, ohne RetroArch.
2. **HW-Render-Cores:** N64 (Mupen64Plus-Next), Dreamcast (Flycast), PSP (PPSSPP) mit
   GLES-Kontext für den Core. Danach wird RetroArch entfernt.

## Test

- Auf dem Gerät für jedes System mit vorhandenem Spiel:
  - starten, Bildschirm-Mitschnitt (`fbgrab`) mit dem heutigen Stand vergleichen;
  - per `rg353-press`: Menü öffnen, in Platz 1 speichern, laden, beenden;
  - prüfen, dass `.srm`, `.st1` und `.png` am richtigen Ort liegen.
- Power-Ablauf wie heute ohne echtes Ausschalten (Dienst stoppen und starten).
- Ton: 10 Minuten Mario Land ohne Unterlauf-Meldungen im Log.
- Menü-Optik: Mitschnitt neben `minui-menu-gbc.png` legen.

## Nicht im Umfang

Optionen-Untermenü, Kurzbefehle, Vorspulen und Zurückspulen, Shader außer sharp/nearest,
Netplay, Cheats, Achievements.

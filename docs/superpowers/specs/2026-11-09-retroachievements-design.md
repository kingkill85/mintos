# RetroAchievements in MintOS

Stand: 2026-11-09 · Gerät: Anbernic RG353V, eigenes Debian-System (siehe README)

## Ziel

Erfolge von RetroAchievements (retroachievements.org) in den Spielen, die in rgarch laufen,
auch ohne Netz. Ein Dienst gleicht im Hintergrund ab, welche Erfolge es gibt, welche man hat,
und lädt neue hoch. rgarch selbst geht nie ins Netz.

### Was der Nutzer will (gesagt)

- RetroAchievements, und es soll auch offline gehen.
- Ein Dienst, der für alle Spiele holt, welche Erfolge es gibt und welche man hat, und die
  neuen hochlädt.
- Nur Softcore: Speichern, Laden und Weiterspielen nach dem Ausschalten bleiben wie jetzt.

### Annahmen (von mir, korrigierbar)

- Anmeldung in den Einstellungen mit Benutzername + Passwort (Bildschirmtastatur);
  gespeichert wird nur das Token des Servers, nie das Passwort.
- Hinweise im Spiel über das vorhandene OSD: beim Start „12/40 Erfolge“, beim Freischalten
  Name + Punkte. Im F-Menü ein Punkt „Erfolge“ mit der Liste.
- Nicht dabei: Hardcore, Bestenlisten (Leaderboards), Rich Presence, die Systeme, die noch
  in RetroArch laufen (n64, nds, psp, dreamcast), ScummVM (hat keine Erfolge).
- Ein Zeichen „12/40“ in der Spielliste ist ein späterer Schritt, nicht Teil dieses Entwurfs.

### Erfolgskriterien

1. Nach der Anmeldung und einem Abgleich mit WLAN zeigt ein Spiel mit Erfolgen beim Start
   „x/y Erfolge“, ohne WLAN genauso, auch wenn es vorher nie gestartet wurde.
2. Ein Erfolg, der im Spiel erreicht wird, erscheint sofort als OSD-Hinweis und in der
   Liste im F-Menü, online wie offline.
3. Beim nächsten Abgleich mit Netz steht er auf der Website im Konto (Softcore).
4. Erfolge, die auf einem anderen Gerät freigeschaltet wurden, gelten nach dem Abgleich
   auch hier als erreicht.
5. Ohne Anmeldung oder mit Schalter „Aus“ ändert sich an rgarch nichts (kein Aufwand pro Bild).
6. Der Spielstart wartet nie auf das Netz oder auf eine Prüfsumme.

## Aufbau

```
                 WLAN                         SD: /storage/.config/mintos/cheevos/
retroachievements.org  <--->  mintos-cheevos  --->  token, hashes.tsv, game/<id>.json,
                                (Dienst, C)            user/<id>.json
                                     ^                         |
                                     | queue/*.json            v (nur lesen)
                                     +-------------------  rgarch (rcheevos rc_client)
```

### rcheevos

Die offizielle C-Bibliothek (github.com/RetroAchievements/rcheevos), fest auf eine
Release-Version gepinnt und als Quelltext unter `third_party/rcheevos/` eingebunden
(kein Debian-Paket). Beide Programme nutzen sie:

- `rc_hash`: Prüfsumme einer ROM, so wie der Server Spiele erkennt (inkl. CD-Images).
- `rc_api_*`: baut die Server-Anfragen und liest die Antworten.
- `rc_client` (nur rgarch): wertet die Erfolgs-Bedingungen bei jedem Bild aus.

### mintos-cheevos (neu, `tools/mintos-cheevos.c`)

Ein Programm mit Unterbefehlen, HTTPS über libcurl.

- `mintos-cheevos login <user>` (Passwort auf stdin, wie bei `mintos-wifi`): holt das Token,
  schreibt `token` (Benutzername + Token, Rechte 600). Ergebnis/Fehler als eine Zeile für die
  Einstellungen.
- `mintos-cheevos logout`: löscht `token` und die Benutzerdaten unter `user/`.
- `mintos-cheevos sync`: der Abgleich, endet sofort, wenn kein Token da ist oder der Server
  nicht erreichbar ist. Schritte:
  1. **Warteschlange hochladen** (zuerst, damit nichts verloren geht): jede Datei in `queue/`
     an den Server (`awardachievement`). Bei Erfolg oder „hatte man schon“ wird sie gelöscht,
     bei Netzfehler bleibt sie liegen, bei einer Ablehnung wandert sie nach `queue/failed/`.
  2. **ROMs erkennen**: für jedes Spiel unter `/storage/roms/<system>` (nur Systeme mit
     bekannter RetroAchievements-Konsole, Tabelle Ordner → Konsolen-ID im Programm) die
     Prüfsumme mit `rc_hash`, gespeichert in `hashes.tsv` (Pfad, Größe, mtime, Hash, Spiel-ID).
     Unveränderte Dateien werden nicht neu berechnet.
  3. **Welche es gibt**: pro Spiel-ID die Erfolgsliste (`patch`) nach `game/<id>.json`,
     neu geholt, wenn älter als 7 Tage oder noch nicht da.
  4. **Welche man hat**: pro Spiel-ID die Freischaltungen des Kontos (`startsession`/
     `unlocks`) nach `user/<id>.json`.
  Schreiben immer über Temp-Datei + `rename`, damit rgarch nie eine halbe Datei liest.
  Ein Lock (`flock` auf `sync.lock`) verhindert zwei Abgleiche gleichzeitig.
- Systemd: `mintos-cheevos.service` (oneshot, niedrige Priorität: `Nice=19`,
  `IOSchedulingClass=idle`, damit Spiele nicht ruckeln). Gestartet wird er
  - von rglauncher beim Start der Liste (`systemctl start --no-block`), also nach jedem Spiel
    und nach dem Booten,
  - von `wifi-check`, sobald das WLAN verbunden ist.

### rgarch (`frontend/cheevos.c`, neu)

Nur aktiv, wenn `token` existiert und der Schalter in `settings.cfg` an ist.

- **Spielstart**: Prüfsumme aus `hashes.tsv` (Pfad + Größe + mtime). Fehlt sie (neue ROM),
  berechnet rgarch sie nicht selbst, das Spiel läuft dann ohne Erfolge bis zum nächsten
  Abgleich. Dann `rc_client` laden.
- **Kein Netz in rgarch**: `rc_client` bekommt eine eigene Server-Funktion. Sie beantwortet
  die Anfragen aus den Dateien:
  - login → aus `token`,
  - patch → `game/<id>.json`,
  - startsession → `user/<id>.json`,
  - awardachievement → schreibt `queue/<zeit>-<id>.json` (Spiel, Erfolg, Hash, Zeitpunkt)
    und antwortet „ok“; zusätzlich wird der Erfolg in `user/<id>.json` eingetragen, damit er
    beim nächsten Start schon zählt.
  Andere Anfragen (ping, Bestenlisten) werden mit „ok“ ignoriert.
- **Speicher**: rgarch übernimmt `RETRO_ENVIRONMENT_SET_MEMORY_MAPS` (jetzt nur mit `true`
  quittiert) und reicht Speicher an `rc_client` weiter: Memory Maps, wenn der Core welche
  setzt, sonst `RETRO_MEMORY_SYSTEM_RAM`/`SAVE_RAM`.
- **Jedes Bild**: `rc_client_do_frame` nach `retro_run`. Im F-Menü `rc_client_idle`.
- **Laden eines Spielstands** (Slot oder Weiterspielen): `rc_client_reset`/Deserialisierung
  des rcheevos-Zustands, damit Erfolge nicht falsch auslösen (Softcore erlaubt Laden).
- **Anzeige**: Ereignisse von `rc_client` (Spiel geladen, Erfolg freigeschaltet) als
  OSD-Hinweis. Das F-Menü bekommt „Erfolge“: Liste mit Titel, Punkten, erreicht/offen,
  blättern mit dem Steuerkreuz wie die Spielliste. Kein Abzeichen-Bild (bräuchte Downloads,
  später möglich).

### Einstellungen (`launcher/settings.c`)

Neuer Punkt „RetroAchievements“:
- abgemeldet: „Anmelden“ → Benutzername, Passwort (Bildschirmtastatur) → Job `mintos-cheevos login`.
- angemeldet: Benutzername, Schalter An/Aus, „Jetzt abgleichen“ (Job `mintos-cheevos sync`,
  zeigt danach „x Spiele, y Erfolge hochgeladen“), „Abmelden“.

## Fehlerfälle

- Server nicht erreichbar → sync endet ohne Änderung, Warteschlange bleibt.
- Token abgelaufen/ungültig → sync schreibt `token.invalid`, Einstellungen zeigen „Neu anmelden“;
  Warteschlange bleibt und wird nach der Anmeldung hochgeladen.
- Kaputte JSON-Datei → rgarch startet das Spiel ohne Erfolge und schreibt ins Log; sync holt
  die Datei neu.
- Spiel, das der Server nicht kennt → in `hashes.tsv` mit Spiel-ID 0, wird nicht jedes Mal
  neu gefragt (erneut nach 30 Tagen).

## Tests

- Host (`frontend/test/run.sh`): Test-Core mit einem Speicherbyte, das sich ändert; eine
  Test-Erfolgsliste in `game/<id>.json`. Prüft: Erfolg löst aus, Queue-Datei entsteht,
  `user/<id>.json` aktualisiert, nach Laden eines Spielstands kein falsches Auslösen.
- Host: `mintos-cheevos sync` gegen einen kleinen lokalen Test-Server (aufgezeichnete
  Antworten, wie die iwd-Daten in `os/test/`): Upload + Löschen der Queue, Netzfehler lässt
  sie liegen, Hash-Cache wird wiederverwendet.
- Gerät (erst nach OK): Anmeldung, Abgleich, ein Spiel mit einem leichten ersten Erfolg
  online und mit WLAN aus, danach Abgleich und Prüfung auf der Website.

## Geprüft (Gerät, 2026-11-09)

- libcurl: `libcurl3t64-gnutls` (libcurl-gnutls.so.4) ist installiert; das Tool linkt dagegen.
- ROM-Formate: viele `.zip` (megadrive, gba, snes, gamegear), PS1 als cue/bin, kein CHD.
  Zips werden vor dem Hashen über `archive_prepare` (frontend/archive.c) entpackt.
- Etwa 30 ROMs: der erste Abgleich ist kurz.
- Arcade (fbneo): Erkennung über den Dateinamen (rc_hash macht das für RC_CONSOLE_ARCADE).

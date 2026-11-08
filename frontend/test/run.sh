#!/usr/bin/env bash
# Host unit tests for rgarch: each frontend/test/test_<x>.c is compiled with the
# modules it names in a "// uses: a.c b.c" line and run. SDL tests use the dummy drivers.
#   frontend/test/run.sh [name...]     (default: all)  -> "ok <name>" / "FAIL <name>"
set -uo pipefail
cd "$(dirname "$0")/../.."
OUT=build/host-tests; mkdir -p "$OUT"
names=("$@"); [ ${#names[@]} -eq 0 ] && names=($(ls frontend/test/test_*.c | sed 's|.*/test_||; s|\.c$||'))
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
fail=0
for n in "${names[@]}"; do
  src=frontend/test/test_$n.c
  uses=$(sed -n 's|^// uses: ||p' "$src" | head -1)
  libs=$(sed -n 's|^// libs: ||p' "$src" | head -1)
  if nix-shell -p gcc SDL2 SDL2_ttf SDL2_image pkg-config --run \
      "gcc -std=gnu11 -g -Wall -Wno-unused-function -Ifrontend -Icommon -Ifrontend/test \$(pkg-config --cflags sdl2) $src $uses -o $OUT/$n $libs -lm -ldl" >"$OUT/$n.build.log" 2>&1 \
     && (cd "$OUT" && ./"$n") >"$OUT/$n.log" 2>&1; then
    echo "ok $n"
  else
    echo "FAIL $n"; tail -5 "$OUT/$n.build.log" "$OUT/$n.log" 2>/dev/null | grep -v '^==>' | sed 's/^/    /'
    fail=$((fail + 1))
  fi
done
exit $fail

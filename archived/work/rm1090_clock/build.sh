#!/bin/sh
# Diagnose-Binary gegen die Bibliotheken von build/macos-arm64 bauen (kein eigenes Build-Verzeichnis).
set -eu
R=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd); B="$R/build/macos-arm64"; O="${TMPDIR:-/tmp}/clockdiag"
c++ -std=gnu++17 -O1 -arch arm64 -mmacosx-version-min=11.0 -I "$R/native" -I "$R/native/enemies" -I "$R/include/ps2/veronica/prog" -I "$B/_deps/sdl2-src/include" \
  "$R/archived/work/rm1090_clock/clockdiag.cpp" -o "$O" -Wl,-rpath,"$B/_deps/sdl2-build" \
  "$B/librecvx_input.a" "$B/librecvx_game.a" "$B/librecvx_scene.a" "$B/librecvx_data.a" "$B/librecvx_disc.a" "$B/librecvx_math.a" -lm "$B/_deps/sdl2-build/libSDL2-2.0.0.dylib" 2>&1 | grep -v warning || true
echo "$O"

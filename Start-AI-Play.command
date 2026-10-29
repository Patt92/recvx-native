#!/bin/sh
# AI play: starts the game with the agent control channel and lets tools/agent_play/driver.py
# play it through the LLM endpoint configured in .env (RECVX_LLM_BASE_URL/_MODEL/_API_KEY).
# Extra arguments go to driver.py (e.g. --steps 50, --no-tools, --model name).
recvx_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$recvx_root" || exit 1
python3 "$recvx_root/tools/check_no_original_core.py" "$recvx_root" || exit 1
recvx_play="$recvx_root/build/play"
recvx_start=
recvx_binary=
recvx_development="$recvx_root/build/start/macos-arm64/RECVX Native.app/Contents/MacOS"
recvx_package="$recvx_root/build/packages/macos-arm64-v0.5/RECVX Native.app/Contents/MacOS"
if [ -x "$recvx_development/Start" ] && [ -x "$recvx_development/recvx-runtime" ]; then
    recvx_start="$recvx_development/Start"; recvx_binary="$recvx_development/recvx-runtime"
elif [ -x "$recvx_package/Start" ]; then
    recvx_start="$recvx_package/Start"; recvx_binary="$recvx_package/recvx-runtime"
elif [ -x "$recvx_root/build/macos-arm64/recvx-runtime" ]; then
    recvx_binary="$recvx_root/build/macos-arm64/recvx-runtime"
else
    printf '%s\n' 'Build fehlt. Im Projektordner: ./build.sh --platform macos' 'Anleitung: STARTEN.md'
    read -r recvx_response
    exit 1
fi
python3 "$recvx_root/tools/check_no_original_core.py" "$recvx_root" --binary "$recvx_binary" || exit 1
if [ ! -f "$recvx_root/.env" ]; then
    printf '%s\n' 'Hinweis: keine .env; driver.py nutzt http://localhost:8080/v1 ohne API-Key.' \
        'Vorlage: RECVX_LLM_BASE_URL=..., RECVX_LLM_MODEL=..., RECVX_LLM_API_KEY=... (siehe docs/AI_PLAY.md).'
fi
# The runtime replays the control file from the start: every session begins with a fresh one.
mkdir -p "$recvx_play" || exit 1
rm -f "$recvx_play/auto.json" "$recvx_play/stream.log.lock"
printf '1 autodump 5 auto\n2 run auto 18\n' > "$recvx_play/live.txt" || exit 1
recvx_ai_args="--control
$recvx_play/live.txt
--snapshots
$recvx_play/snap
--video-config
$recvx_root/video.ini
--enemy-ai
on
--enemy-grab
on"
if [ -n "$recvx_start" ]; then
    (IFS='
'; set -f; exec "$recvx_start" $recvx_ai_args) &
else
    (IFS='
'; set -f; exec "$recvx_binary" --data "$recvx_root/data" --mods "$recvx_root/mods" --controls "$recvx_root/controls.ini" \
        --debug-config "$recvx_root/debug.ini" $recvx_ai_args) &
fi
recvx_game=$!
trap 'kill "$recvx_game" 2>/dev/null; exit 130' INT TERM
recvx_wait=0
while [ ! -f "$recvx_play/auto.json" ]; do
    if ! kill -0 "$recvx_game" 2>/dev/null; then
        printf '%s\n' 'Spiel wurde beendet, bevor Metriken kamen.'; read -r recvx_response; exit 1
    fi
    recvx_wait=$((recvx_wait + 1))
    if [ "$recvx_wait" -gt 120 ]; then
        printf '%s\n' 'Keine Metriken nach 60 s (build/play/auto.json fehlt).'; kill "$recvx_game"; read -r recvx_response; exit 1
    fi
    sleep 0.5
done
python3 "$recvx_root/tools/agent_play/driver.py" --dir "$recvx_play" "$@"
recvx_status=$?
if [ "$recvx_status" -ne 0 ] && [ "$recvx_status" -ne 130 ]; then
    printf '%s\n' "driver.py endete mit Status $recvx_status (Log: build/play/driver.log). Das Spiel bleibt offen."
fi
# The game stays open after the driver stops (endpoint error, --steps reached); closing it ends this window.
wait "$recvx_game"
exit "$recvx_status"

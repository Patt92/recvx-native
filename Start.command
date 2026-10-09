#!/bin/sh
recvx_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
python3 "$recvx_root/tools/check_no_original_core.py" "$recvx_root" || exit 1
recvx_package="$recvx_root/build/packages/macos-arm64-v0.5"
if [ -x "$recvx_package/RECVX Native.app/Contents/MacOS/Start" ]; then
    python3 "$recvx_root/tools/check_no_original_core.py" "$recvx_root" --binary "$recvx_package/RECVX Native.app/Contents/MacOS/recvx-runtime" || exit 1
    exec "$recvx_package/RECVX Native.app/Contents/MacOS/Start" "$@"
fi
if [ -x "$recvx_root/build/macos-arm64/recvx-runtime" ]; then
    python3 "$recvx_root/tools/check_no_original_core.py" "$recvx_root" --binary "$recvx_root/build/macos-arm64/recvx-runtime" || exit 1
    exec "$recvx_root/build/macos-arm64/recvx-runtime" --data "$recvx_root/data" --mods "$recvx_root/mods" --controls "$recvx_root/controls.ini" --debug-config "$recvx_root/debug.ini" "$@"
fi
printf '%s\n' 'Build fehlt. Im Projektordner: ./build.sh --platform macos' 'Anleitung: STARTEN.md'
read -r recvx_response
exit 1

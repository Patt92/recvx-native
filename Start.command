#!/bin/sh
recvx_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
recvx_package="$recvx_root/build/packages/macos-universal-v0.5"
if [ -x "$recvx_package/RECVX Native.app/Contents/MacOS/Start" ]; then
    exec "$recvx_package/RECVX Native.app/Contents/MacOS/Start" "$@"
fi
if [ -x "$recvx_root/build/macos-universal/recvx-runtime" ]; then
    exec "$recvx_root/build/macos-universal/recvx-runtime" --data "$recvx_root/data" --mods "$recvx_root/mods" --controls "$recvx_root/controls.ini" --debug-config "$recvx_root/debug.ini" "$@"
fi
printf '%s\n' 'Build fehlt. Im Projektordner: ./build.sh --platform macos' 'Anleitung: STARTEN.md'
read -r recvx_response
exit 1

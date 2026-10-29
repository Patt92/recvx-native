#!/bin/sh
# Prepares data/ from the disc image(s) in iso/ (see README, "GitHub Actions"). Needs Python 3.12+ and FFmpeg.
recvx_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$recvx_root/tools/extract_iso.py" "$@"

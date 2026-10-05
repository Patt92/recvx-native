#!/bin/sh
set -eu
recvx_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$recvx_root/tools/build_native.py" "$@"

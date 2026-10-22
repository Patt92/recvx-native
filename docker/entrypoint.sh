#!/bin/sh
# Builder entrypoint: builds the requested targets from the source baked into the
# image and writes everything to /out. Disc images are read from /iso.
#
#   targets:  linux | windows | windows-arm64   (default: all three)
#   options:  --test        build and run CTest for the Linux target
#             --no-package  build only, do not create Windows packages
#
# Output layout:
#   /out/data                              cooked data (created once from /iso/*.iso)
#   /out/linux-<arch>-v<version>/          Linux install tree (no project package format exists)
#   /out/packages/windows-x64-v<version>/  Windows packages (need /out/data)
set -eu
cd /src

targets=""
test_linux=0
package=1
for argument in "$@"; do
    case "$argument" in
        linux|windows|windows-arm64) targets="$targets $argument" ;;
        --test) test_linux=1 ;;
        --no-package) package=0 ;;
        macos|ios|ios-simulator)
            echo "error: $argument needs Xcode and the Apple SDKs; build it on a Mac with ./build.sh" >&2; exit 2 ;;
        *) echo "error: unknown argument '$argument' (targets: linux windows windows-arm64; options: --test --no-package)" >&2; exit 2 ;;
    esac
done
[ -n "$targets" ] || targets="linux windows windows-arm64"

[ -w /out ] || { echo "error: /out is not writable for uid $(id -u); set UID/GID for the image (see docs/DOCKER.md)" >&2; exit 1; }
python3 tools/check_no_original_core.py

jobs=${RECVX_JOBS:-$(nproc)}
[ "$jobs" -le 256 ] || jobs=256
data=/out/data
version=$(python3 -c 'import sys; sys.path.insert(0, "tools"); import build_native; print(build_native.FULL_VERSION)')
arch=$(uname -m)

# All disc images in /iso, in name order; repeated --iso merges discs.
set --
for image in /iso/*.iso /iso/*.ISO; do
    [ -f "$image" ] && set -- "$@" --iso "$image"
done

need_linux=0
case " $targets " in *" linux "*) need_linux=1 ;; esac
# The ISO exporters (recvx-assets, recvx-world-assets) run on the build host, so
# preparing data for any target needs the Linux build first.
if [ ! -f "$data/native-data.version" ]; then
    if [ $# -gt 0 ]; then
        need_linux=1
    else
        echo "note: no prepared data in $data and no ISO in /iso; packages are skipped" >&2
    fi
fi

if [ "$need_linux" -eq 1 ]; then
    if [ $# -gt 0 ] && [ ! -f "$data/native-data.version" ]; then
        # Docker Desktop's host mounts reject libstdc++ std::filesystem::copy_file (the
        # exporter's file copy), while cp and Python copies work. Cook inside the
        # container volume and publish the finished tree to /out afterwards.
        staging=/src/build/docker-data
        rm -rf "$staging" && mkdir -p "$staging"
        python3 tools/build_native.py --platform linux --jobs "$jobs" --data "$staging/data" "$@"
        rm -rf "$data.partial"
        cp -a "$staging/data" "$data.partial"
        mv "$data.partial" "$data"
        rm -rf "$staging"
        set --
    fi
    python3 tools/build_native.py --platform linux --jobs "$jobs" --data "$data" "$@"
    if [ "$test_linux" -eq 1 ]; then
        # The data-dependent CTest cases read <source>/data.
        [ -f "$data/native-data.version" ] && ln -sfn "$data" /src/data
        python3 tools/build_native.py --platform linux --jobs "$jobs" --data "$data" --test
    fi
fi

case " $targets " in *" linux "*)
    destination=/out/linux-$arch-v$version
    rm -rf "$destination"
    cmake --install build/native --prefix "$destination"
    mkdir -p "$destination/lib"
    # SDL2 as a FetchContent subproject has no install rule; the runtime finds it via $ORIGIN/../lib.
    cp -P build/native/_deps/sdl2-build/libSDL2-2.0.so* "$destination/lib/"
    cp LICENSE "$destination/LICENSE.txt"
    cp build/native/_deps/sdl2-src/LICENSE.txt "$destination/SDL2-LICENSE.txt"
    echo "Linux: $destination (start with bin/recvx-runtime --data <data>)"
    ;;
esac

for target in windows windows-arm64; do
    case " $targets " in *" $target "*) ;; *) continue ;; esac
    label=$([ "$target" = windows ] && echo windows-x64 || echo windows-arm64)
    output=/out/packages/$label-v$version
    if [ "$package" -eq 1 ] && [ -f "$data/native-data.version" ] && [ -e "$output" ]; then
        # Packages are never overwritten (tools/package_native.py); rebuild, keep the existing one.
        python3 tools/build_native.py --platform "$target" --jobs "$jobs" --data "$data"
        echo "Windows: $output already exists and was kept; remove it to package this version again"
    elif [ "$package" -eq 1 ] && [ -f "$data/native-data.version" ]; then
        mkdir -p /out/packages
        python3 tools/build_native.py --platform "$target" --jobs "$jobs" --data "$data" --package --output "$output"
        echo "Windows: $output"
    else
        python3 tools/build_native.py --platform "$target" --jobs "$jobs" --data "$data"
        mkdir -p "/out/$label-v$version"
        build=build/$label
        cp "$build/recvx-runtime.exe" "$build/recvx-native.exe" "$build/SDL2.dll" "/out/$label-v$version/"
        echo "Windows (binaries only, no data): /out/$label-v$version"
    fi
done

# Docker builder

`Dockerfile` and `compose.builder.yaml` (Compose project `recvx-builder`) build an image with every toolchain for the
targets that can be built on Linux:

| Target | In the container | Result |
|--------|------------------|--------|
| `linux` | native (container architecture: arm64 on Apple silicon, x86_64 on x86 hosts) | install tree `out/linux-<arch>-v<version>/` |
| `windows` | cross-build, pinned Zig 0.16.0 + SDL2 MinGW | package `out/packages/windows-x64-v<version>/` |
| `windows-arm64` | cross-build, pinned Zig 0.16.0, SDL2 from source | package `out/packages/windows-arm64-v<version>/` |
| `macos`, `ios`, `ios-simulator` | **not supported** | build on a Mac with `./build.sh` |
| web (browser) | separate project in `compose.yaml` (`docker/web.Dockerfile`) | nginx on port 8722 serving the WebAssembly runtime; game data mounted read-only — see [Web](WEB.md) |

macOS and iOS need Xcode, the Apple SDK frameworks (Cocoa, Metal, UIKit),
`lipo`/`codesign` and for devices a provisioning profile. Apple's SDK license
only permits these on Apple hardware, and a Linux container has none of them.
Cross-toolchains such as osxcross would need a copied Xcode SDK and are not used.

## Use

```bash
RECVX_BUILD_NUMBER=$(git rev-list --count HEAD) docker compose -f compose.builder.yaml build
```

```bash
docker compose -f compose.builder.yaml run --rm builder
```

Targets and options can be given explicitly, e.g.
`docker compose -f compose.builder.yaml run --rm builder windows --no-package` or
`docker compose -f compose.builder.yaml run --rm builder linux --test`.

Folders on the host:

- `./iso` (read-only, `RECVX_ISO_DIR`): the disc image(s). Every `*.iso` is
  passed as `--iso`, in name order.
- `./out` (`RECVX_OUTPUT_DIR`): `data/` (cooked once from the ISO; later runs
  reuse it), the Linux install tree and the Windows packages. An existing
  package of the same version is kept (the binaries are still rebuilt); remove
  it to package again.
- The named volume `build-cache` holds `/src/build` (CMake trees, Zig cache,
  SDL2 downloads) so that later runs are incremental.

Without prepared data and without an ISO, Windows targets produce only the
binaries (`out/windows-*-v<version>/`) and no package.

## Verified (2026-10-22, Docker Desktop on Apple silicon)

- Full run (`linux windows windows-arm64` from the USA ISO): Linux aarch64
  install tree and both Windows packages built; policy checks PASS.
- `linux --test`: 224/225 on Linux aarch64; the one red test (`door_room_case`)
  fails identically on macOS with the same working tree.
- Cooked data equals a macOS export file by file except the 18 movies: the
  container's FFmpeg (Ubuntu 24.04, 6.1) encodes the MJPEG frames with other
  bytes than a newer host FFmpeg. Both are valid re-encodes of the same stream.
- Docker Desktop's host mounts reject libstdc++ `std::filesystem::copy_file`
  (used by the exporter), while `cp` and Python copies work. The entrypoint
  therefore cooks inside the build volume and publishes the finished tree to
  `out/data` with `cp -a`.

## Notes

- The image contains the tracked sources only (`.dockerignore` is an allowlist).
  `archived/`, `build/`, `data/`, `iso/`, user INIs, saves, snapshots and `.git`
  are never part of the build context. Rebuild the image after source changes.
- Without `.git` the build number comes from the `RECVX_BUILD_NUMBER` build
  argument (`tools/build_native.py`).
- The container runs as uid/gid 1000. On Linux hosts with another user, build
  with `RECVX_UID=$(id -u) RECVX_GID=$(id -g)` so that `out/` stays writable.
- An x86_64 Linux build on Apple silicon: `DOCKER_DEFAULT_PLATFORM=linux/amd64`
  for `build` and `run` (emulated, slow). Windows results are the same on both
  architectures.
- Windows cross-builds cannot run tests in the container; test them on Windows
  or Wine. `--test` runs the Linux CTest suite (headless).
- Linux has no project package format yet (see README); the install tree holds
  `bin/recvx-runtime`, `bin/recvx-native` and `lib/libSDL2-2.0.so*`. Start with
  `bin/recvx-runtime --data <out>/data`.

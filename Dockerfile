# RECVX Native builder: Linux (native to the container architecture) plus
# Windows x64/ARM64 cross-builds with the pinned Zig/SDL2 from tools/build_windows.py.
# macOS and iOS need Xcode and the Apple SDKs and are built on a Mac with ./build.sh.
# Usage: see docs/DOCKER.md and compose.yaml.
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
# Python 3.12+ (README), CMake/Ninja, SDL2 host backends for the Linux runtime,
# OpenGL headers for the optional GL backend, FFmpeg for --iso movie cooking,
# llvm-nm for the policy binary audit of ELF and COFF/PE objects.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build pkg-config git ca-certificates xz-utils \
        python3 ffmpeg llvm \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev \
        libwayland-dev libxkbcommon-dev libegl-dev libgl-dev libgles-dev \
        libasound2-dev libpulse-dev \
    && rm -rf /var/lib/apt/lists/*

ARG UID=1000
ARG GID=1000
RUN if getent passwd ${UID} >/dev/null; then userdel -r "$(getent passwd ${UID} | cut -d: -f1)"; fi \
    && if ! getent group ${GID} >/dev/null; then groupadd -g ${GID} builder; fi \
    && useradd -m -u ${UID} -g ${GID} builder \
    && mkdir -p /src /out /iso && chown ${UID}:${GID} /src /out

USER builder
WORKDIR /src
COPY --chown=${UID}:${GID} . /src

# Zig toolchain and SDL2 MinGW libraries are downloaded and checksum-verified once
# into build/toolchains and build/dependencies; the named build volume keeps them.
RUN python3 tools/build_windows.py --prepare-only --arch x64 \
    && python3 tools/build_windows.py --prepare-only --arch arm64

# Build number for the app version (normally `git rev-list --count HEAD`; .git is
# not part of the image).
ARG RECVX_BUILD_NUMBER=0
ENV RECVX_BUILD_NUMBER=${RECVX_BUILD_NUMBER}

ENTRYPOINT ["/src/docker/entrypoint.sh"]
CMD ["linux", "windows", "windows-arm64"]

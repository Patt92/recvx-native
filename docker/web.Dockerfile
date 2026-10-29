# RECVX Native — Web-Build und Auslieferung (Browser: Firefox, Chrome).
# Stufe 1 baut recvx.js/recvx.wasm mit dem gepinnten Emscripten (emsdk 6.0.13, enthaelt den SDL2-2.32.10-Port).
# Stufe 2 liefert die statische Seite mit nginx auf Port 8722 aus. Spieldaten sind NICHT im Image: data/ (und
# optional mods/) werden zur Laufzeit read-only eingehaengt (compose.yaml, docs/WEB.md).
FROM emscripten/emsdk:6.0.13 AS build
RUN apt-get update && apt-get install -y --no-install-recommends ninja-build && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . /src
# Policy-Pruefung vor dem Build (AGENTS.md); der Build selbst prueft recvx.wasm erneut (Symbolaudit).
RUN python3 tools/check_no_original_core.py /src \
    && emcmake cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    && cmake --build /build --target recvx-runtime --parallel \
    && mkdir -p /site && cp /build/recvx.js /build/recvx.wasm /site/ && cp /src/web/shell.html /site/index.html \
    && cp /src/android/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png /site/favicon.png

FROM nginx:1.29-alpine
RUN rm -f /etc/nginx/conf.d/default.conf
COPY web/nginx.conf /etc/nginx/conf.d/recvx.conf
COPY --from=build /site /srv/recvx/site
# Einhaengepunkte fuer die Spieldaten (read-only Volumes).
RUN mkdir -p /srv/recvx/data /srv/recvx/mods
EXPOSE 8722
HEALTHCHECK --interval=30s --timeout=3s CMD wget -q -O /dev/null http://127.0.0.1:8722/index.html || exit 1

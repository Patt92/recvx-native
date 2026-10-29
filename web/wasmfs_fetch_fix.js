/**
 * Ersatz fuer _wasmfs_create_fetch_backend_js aus Emscripten 6.0.13 (src/lib/libwasmfs_fetch.js). Plattform-
 * anpassung des Datenzugriffs, keine Spiellogik; per --js-library nach der Systembibliothek eingebunden und
 * damit an ihrer Stelle verwendet (CMakeLists.txt).
 *
 * Fehler im Original (belegt mit native movies/0007.rvv, 68 MB): liefert der Server auf HEAD+Range kein
 * "Accept-Ranges: bytes" (nginx laesst den Kopf bei 206 weg), wird die Datei ganz geladen und als ein Block mit
 * chunkSize = Dateigroesse gespeichert. Ein spaeterer Lesezugriff ueber mehr als 16 MiB rechnet aber mit der
 * Backend-Blockgroesse, haelt die Bloecke 1..n fuer fehlend, laedt sie per Range nach und legt sie neben den
 * Ganzblock; read() nutzt wieder chunkSize = Dateigroesse und kopiert nur Block 0 -> ab Byte 16 MiB Nullen.
 * Korrektur: jede Datei hat genau eine Blockgroesse, mit der Laden und Lesen rechnen; Range wird an der
 * tatsaechlichen 206-Antwort erkannt statt am Kopf "Accept-Ranges"; fehlende Bloecke werden einzeln
 * angefordert und auf Laenge geprueft. Kurze Antworten, Abbrueche und falsche Range-Antworten sind Fehler
 * (EIO), nie stille Nullen.
 */
addToLibrary({
  _wasmfs_create_fetch_backend_js__deps: [
    '$wasmFS$backends',
    '$wasmFS$JSMemoryRanges',
    '_wasmfs_fetch_get_file_url',
    '_wasmfs_fetch_get_chunk_size',
  ],
  _wasmfs_create_fetch_backend_js: async function(backend) {
    function fileUrl(file) {
      var relative = UTF8ToString(__wasmfs_fetch_get_file_url(file));
      if (relative.indexOf('://') !== -1) return relative;
      return new URL(relative, self.location.href).toString();
    }
    // Groesse und Range-Faehigkeit einmal je Datei: Range-Anfrage auf Byte 0 statt HEAD (dieselbe Antwortform wie
    // spaetere Blockanfragen; Content-Range traegt die Gesamtgroesse).
    async function describe(file) {
      var info = wasmFS$JSMemoryRanges[file];
      if (info) return info;
      var url = fileUrl(file), chunkSize = __wasmfs_fetch_get_chunk_size(file);
      var probe = await fetch(url, {headers: {'Range': 'bytes=0-0'}});
      if (probe.status === 404) throw {status: 404};
      if (probe.status === 206) {
        var range = /\/(\d+)$/.exec(probe.headers.get('Content-Range') || '');
        await probe.arrayBuffer();
        if (!range) throw {status: 500, reason: 'range response without total size: ' + url};
        info = {size: parseInt(range[1], 10), chunkSize: chunkSize, chunks: [], url: url};
      } else if (probe.ok) {
        // Server ohne Range: ganze Datei als ein Block (Blockgroesse = Dateigroesse, beim Lesen ebenso).
        var whole = new Uint8Array(await probe.arrayBuffer());
        info = {size: whole.byteLength, chunkSize: Math.max(1, whole.byteLength), chunks: [whole], url: url};
      } else {
        throw {status: probe.status};
      }
      wasmFS$JSMemoryRanges[file] = info;
      return info;
    }
    async function ensure(info, offset, length) {
      if (length <= 0 || offset >= info.size) return;
      var size = info.chunkSize, first = Math.floor(offset / size), last = Math.floor((Math.min(info.size, offset + length) - 1) / size);
      var missing = [];
      for (var i = first; i <= last; i++) if (!info.chunks[i]) missing.push(i);
      // Zusammenhaengende fehlende Bloecke in einer Anfrage.
      for (var m = 0; m < missing.length;) {
        var a = missing[m], b = a;
        while (m + 1 < missing.length && missing[m + 1] === b + 1) { m++; b++; }
        m++;
        var start = a * size, end = Math.min(info.size, (b + 1) * size);
        var response = await fetch(info.url, {headers: {'Range': `bytes=${start}-${end - 1}`}});
        if (response.status !== 206) throw {status: response.status || 500, reason: 'expected 206 for ' + info.url};
        var bytes = new Uint8Array(await response.arrayBuffer());
        if (bytes.byteLength !== end - start) throw {status: 500, reason: `short range ${start}-${end - 1} of ${info.url}: ${bytes.byteLength} bytes`};
        for (var i = a; i <= b; i++) info.chunks[i] = bytes.subarray((i - a) * size, Math.min(bytes.byteLength, (i - a + 1) * size));
      }
    }
    function failure(error) {
      if (error && error.status === 404) return -{{{ cDefs.ENOENT }}};
      console.error('RECVX data fetch failed:', error && (error.reason || error.status || error));
      return -{{{ cDefs.EIO }}};
    }
    wasmFS$backends[backend] = {
      allocFile: async (file) => {},
      freeFile: async (file) => { wasmFS$JSMemoryRanges[file] = undefined; },
      write: async (file, buffer, length, offset) => -{{{ cDefs.EROFS }}},
      read: async (file, buffer, length, offset) => {
        if (offset < 0 || length <= 0) return 0;
        try {
          var info = await describe(file);
          length = Math.min(length, info.size - offset);
          if (length <= 0) return 0;
          await ensure(info, offset, length);
          var size = info.chunkSize, done = 0;
          while (done < length) {
            var at = offset + done, index = Math.floor(at / size), within = at - index * size;
            var chunk = info.chunks[index], take = Math.min(length - done, chunk.byteLength - within);
            if (take <= 0) throw {status: 500, reason: 'missing block ' + index + ' of ' + info.url};
            HEAPU8.set(chunk.subarray(within, within + take), buffer + done);
            done += take;
          }
          return done;
        } catch (error) {
          return failure(error);
        }
      },
      getSize: async (file) => {
        try { return (await describe(file)).size; } catch (error) { return failure(error); }
      },
    };
  },
});

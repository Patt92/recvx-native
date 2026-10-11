#pragma once
/* D10: Objektbaum der Etagenkarte.
 *
 * bhMlbBinRealize (map.c:478 fuer den DAT_SET-Chunk) macht aus dem MDL-Block
 * die NJS_CNK_OBJECT-Kette, die MapCodeProcess danach liest. Diese Einheit
 * liest genau diesen Baum aus dem geladenen Paket und liefert ihn als
 * recvx::MapObject mit Index-Verknuepfungen; MapScreen::set_map_objects
 * fuehrt anschliessend das originale MapPurgeTree (map.c:1245) aus.
 *
 * Belegt am Originaldekoder und an den Daten:
 *   native/world.cpp:91       MDL-Tag und obj_off bei +12
 *   native/scene.cpp:208-215  Knotenlayout: flags +0, model +4, pos +8,
 *                             ang +20, scale +32, child +44, sibling +48,
 *                             Schrittweite 52, 0xffffffff = kein Verweis
 *   data/archives/MRY/0006.bin  Zeigerbasis ist Chunkanfang + 16 (der
 *                             MDL-Kopf): obj_off 0x50dc trifft den ersten
 *                             Knoten, dessen child 0x5110 den zweiten und
 *                             dessen sibling 0x5144 den dritten.
 */
#include "map_screen.h"
#include <cstdint>
#include <vector>

namespace recvx {
// Liest den Objektbaum des MDL-Chunks `chunk` aus `packet`. Ein leerer Chunk
// oder ein obj_off von -1 ergibt eine leere Liste (das Original realisiert
// dann keine Objekte, world.cpp:88).
std::vector<MapObject> decode_map_objects(const std::vector<std::uint8_t>& packet,const MapChunk& chunk);
}

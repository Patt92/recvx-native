#pragma once
#include "scene.h"
#include <filesystem>
namespace recvx {
std::vector<std::uint8_t> read_asset_file(const std::filesystem::path& path,std::size_t limit);
void write_asset_file(const std::filesystem::path& path,const std::vector<std::uint8_t>& data);
std::string asset_number(unsigned index);
// A SYSTEM archive entry is a chain of size-prefixed blocks; the last size
// carries bit 31 and the PVP texture bundle starts at the next 32-byte
// boundary. Returns the bundle offset, or size_t(-1) when the chain does not
// terminate inside the entry.
std::size_t system_texture_bundle_offset(const std::vector<std::uint8_t>& entry);
SceneTexture read_tga(const std::filesystem::path& path);
// Verlustfreie Datenbaumform einer TGA (prepare_native_data.py --compressed): 8-Bit-Farbtabelle mit RLE
// (Typ 9), wenn die Datei hoechstens 256 Farben traegt, sonst RGB/RGBA-RLE (Typ 10). read_tga und der
// HD-Inhaltsschluessel ergeben daraus dieselben Bildpunkte bzw. Bytes wie aus der Quelle (geprueft).
std::vector<std::uint8_t> compress_tga(const std::vector<std::uint8_t>& bytes,const std::filesystem::path& path);
void write_tga(const std::filesystem::path& path,const SceneTexture& texture);
void apply_texture_files(Scene& scene,const std::filesystem::path& data,const std::filesystem::path& mods,const std::filesystem::path& identity);
// Herkunft der von apply_texture_files gesetzten Texturen seit Prozessstart: explizite Mod-Datei,
// HD-Ersetzung (mods/textures/hd), gekochte Datei aus data/, decodiertes Original ohne Datei.
struct TextureFileStats {std::size_t mod=0,hd=0,data=0,original=0;};
TextureFileStats texture_file_stats();
// Hostschalter (Debugmenue): Textur-Mods an/aus; wirkt auf alle danach geladenen Texturen.
void set_texture_mods_enabled(bool enabled);
bool texture_mods_enabled();
// Format der ersten TGA unter data/textures, fuer die Startmeldung.
std::string texture_tree_format(const std::filesystem::path& data);
// Anzahl der Dateien in mods/textures/hd.
std::size_t hd_texture_count(const std::filesystem::path& mods);
// Anzahl PCSX2-Ersatztexturen (*.dds mit Hash-Namen) direkt in mods/textures; nicht direkt geladen.
std::size_t pcsx2_pack_count(const std::filesystem::path& mods);
}

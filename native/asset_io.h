#pragma once
#include "scene.h"
#include <filesystem>
namespace recvx {
std::vector<std::uint8_t> read_asset_file(const std::filesystem::path& path,std::size_t limit);
void write_asset_file(const std::filesystem::path& path,const std::vector<std::uint8_t>& data);
std::string asset_number(unsigned index);
SceneTexture read_tga(const std::filesystem::path& path);
void write_tga(const std::filesystem::path& path,const SceneTexture& texture);
void apply_texture_files(Scene& scene,const std::filesystem::path& data,const std::filesystem::path& mods,const std::filesystem::path& identity);
}

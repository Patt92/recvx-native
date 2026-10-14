#pragma once
#include <filesystem>
#include <string>
namespace recvx {
// EINE gemeinsame Aufloesung fuer Benutzerdaten (AGENTS.md "Benutzerdaten-Ort"):
// INIs (controls/video/debug), Snapshots und Spielstaende. Im Release-Paket
// liegen sie im app-eigenen Benutzerdatenverzeichnis (%APPDATA%\RECVX bzw.
// ~/Library/Application Support/RECVX), im Test-/Entwicklungsbuild (./build.sh,
// Start.command-Dev-App) im Git-Wurzelverzeichnis. Keine verstreuten Einzelpfade.
struct UserPaths {
    std::filesystem::path root;   // Basis der Benutzerdaten
    bool dev=false;               // true: Git-Arbeitsbaum (Entwicklungsbuild)
    std::filesystem::path saves()const{return root/"saves";}
    std::filesystem::path snapshots()const{return root/"snapshots";}
    std::filesystem::path file(const std::string& name)const{return root/name;}
};
// Plattform-Benutzerdatenverzeichnis (Release): die dokumentierten Orte, ohne
// eigene SDL-Bindung. Windows %APPDATA%\RECVX, macOS
// ~/Library/Application Support/RECVX, sonst $XDG_DATA_HOME/RECVX.
std::filesystem::path platform_user_data_dir();
// true, wenn `data_dir` im Git-Arbeitsbaum dieses Projekts liegt (Wurzel neben
// data/ mit .git oder CLAUDE.md): die Dev-App uebergibt --data <git-root>/data
// (tools/build_native.py), waehrend das Release-Paket --data <bundle>/data und
// einen leeren Elternordner hat.
bool is_dev_tree(const std::filesystem::path& data_dir);
// Prioritaet (AGENTS.md "Benutzerdaten-Ort"): explizit uebergebener Pfad >
// Dev-Modus (Git-Wurzel) > Benutzerdatenverzeichnis.
UserPaths resolve_user_paths(const std::filesystem::path& data_dir,const std::filesystem::path& explicit_root={});
}

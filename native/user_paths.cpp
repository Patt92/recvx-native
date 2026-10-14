#include "user_paths.h"
#include <cstdlib>
#include <stdexcept>
namespace recvx {
namespace {
const char* env(const char* name){const char* v=std::getenv(name);return v&&*v?v:nullptr;}
}
std::filesystem::path platform_user_data_dir(){
#ifdef _WIN32
    if(const char* appdata=env("APPDATA"))return std::filesystem::path(appdata)/"RECVX";
    if(const char* profile=env("USERPROFILE"))return std::filesystem::path(profile)/"AppData"/"Roaming"/"RECVX";
    throw std::runtime_error("cannot locate the Windows user-data directory (APPDATA/USERPROFILE unset)");
#elif defined(__APPLE__)
    if(const char* home=env("HOME"))return std::filesystem::path(home)/"Library"/"Application Support"/"RECVX";
    throw std::runtime_error("cannot locate the macOS user-data directory (HOME unset)");
#else
    if(const char* xdg=env("XDG_DATA_HOME"))return std::filesystem::path(xdg)/"RECVX";
    if(const char* home=env("HOME"))return std::filesystem::path(home)/".local"/"share"/"RECVX";
    throw std::runtime_error("cannot locate the user-data directory (XDG_DATA_HOME/HOME unset)");
#endif
}
bool is_dev_tree(const std::filesystem::path& data_dir){
    // Auch ein relatives --data ("data") wird gegen das Arbeitsverzeichnis
    // aufgeloest; die Dev-App uebergibt zwar absolute Pfade, direkte
    // recvx-runtime-Aufrufe aus dem Git-Wurzelverzeichnis aber "data".
    std::error_code ec;
    auto absolute=std::filesystem::absolute(data_dir,ec);
    if(ec)absolute=data_dir;
    const auto parent=absolute.parent_path();
    if(parent.empty())return false;
    return std::filesystem::exists(parent/".git",ec)||std::filesystem::exists(parent/"CLAUDE.md",ec);
}
UserPaths resolve_user_paths(const std::filesystem::path& data_dir,const std::filesystem::path& explicit_root){
    UserPaths paths;
    if(!explicit_root.empty()){paths.root=explicit_root;paths.dev=false;return paths;}
    if(is_dev_tree(data_dir)){
        std::error_code ec;
        auto absolute=std::filesystem::absolute(data_dir,ec);
        paths.root=(ec?data_dir:absolute).parent_path();
        paths.dev=true;
        return paths;
    }
    paths.root=platform_user_data_dir();paths.dev=false;return paths;
}
}

// Web-Einstieg (Emscripten). Plattformanpassung, keine Spiellogik:
//  - Browser-Hauptthread: besitzt SDL (web_sdl_proxy.cpp), kehrt sofort in die Ereignisschleife zurueck.
//  - Spielthread (pthread): haengt die Datenquellen ein und ruft die unveraenderte Runtime
//    (recvx_web_runtime_main = recvx_runtime_main) mit den Pfaden /data, /data/../mods und /user auf.
//
// Dateien (docs/WEB.md):
//  - /data: WasmFS-Fetch-Backend auf <Seite>/data (HTTP-Range, Laden bei Bedarf). Das Backend listet keine
//    Verzeichnisse; der Baum wird aus web-files.txt (vom Build aus data/ erzeugt: eine Zeile je Datei,
//    relative Pfade) vorab als leere Eintraege angelegt. Groessen und Inhalte kommen beim ersten Zugriff vom
//    Server.
//  - /mods: dasselbe fuer einen optionalen Mods-Baum (mods-files.txt, fehlt er: kein Mods-Ordner).
//  - /user: WasmFS-OPFS-Backend (Origin Private File System), dauerhaft je Browser und Origin:
//    INIs, Spielstaende, Snapshots, runtime-warnings.log.
// Das Fetch-Backend muss auf einem pthread erzeugt werden (WasmFS-Vorgabe; im Hauptthread haengt Firefox beim
// ersten Lesen, belegt in der Probe).
#include "web_sdl_proxy.h"
#include <emscripten.h>
#include <emscripten/wasmfs.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int recvx_web_runtime_main(int argc,char** argv);

namespace {
// Fortschritt/Fehler fuer die Startseite (web/shell.html liest Module.recvxStatus).
void status(const char* stage,const std::string& detail){
    // Synchron: der String lebt nur waehrend des Aufrufs (asynchron laese der Hauptthread freigegebenen Speicher).
    MAIN_THREAD_EM_ASM({if(Module.recvxStatus)Module.recvxStatus(UTF8ToString($0),UTF8ToString($1));},stage,detail.c_str());
}
// Verzeichnisse eines relativen Pfades unter root anlegen (mkdir -p), dann die Datei als Fetch-Eintrag.
void add_entry(const std::string& root,const std::string& relative,backend_t backend){
    std::string path=root;
    std::size_t start=0;
    for(std::size_t slash;(slash=relative.find('/',start))!=std::string::npos;start=slash+1){
        path=root+"/"+relative.substr(0,slash);
        if(mkdir(path.c_str(),0777)!=0&&errno!=EEXIST)throw std::runtime_error("cannot create "+path+": "+std::strerror(errno));
    }
    const std::string file=root+"/"+relative;
    const int fd=wasmfs_create_file(file.c_str(),0444,backend);
    if(fd<0)throw std::runtime_error("cannot create "+file);
    close(fd);
}
// Mountet base_url unter root und legt alle Eintraege aus der Dateiliste an. false: Liste fehlt (optional).
bool mount_fetch_tree(const char* root,const char* base_url,const char* listing,bool required){
    backend_t backend=wasmfs_create_fetch_backend(base_url,0);
    if(!backend)throw std::runtime_error(std::string("cannot create fetch backend for ")+base_url);
    if(wasmfs_create_directory(root,0777,backend)!=0)throw std::runtime_error(std::string("cannot mount ")+root);
    // Die Dateiliste selbst liegt im Fetch-Baum und wird ueber ihn gelesen.
    const std::string list_path=std::string(root)+"/"+listing;
    {const int fd=wasmfs_create_file(list_path.c_str(),0444,backend);if(fd<0)throw std::runtime_error("cannot create "+list_path);close(fd);}
    // Ein Fetch-Eintrag laesst sich immer oeffnen; ob der Server die Liste hat (404/403), zeigt erst das Lesen.
    // Darum die ganze Liste lesen und Lesefehler bzw. eine leere Liste als "fehlt" werten.
    std::string text;
    {std::ifstream in(list_path,std::ios::binary);
     if(in){std::ostringstream all;all<<in.rdbuf();if(!in.bad())text=all.str();}}
    if(text.empty()){
        unlink(list_path.c_str());rmdir(root);
        if(required)throw std::runtime_error(std::string("game data index missing or unreadable: ")+base_url+"/"+listing+
                                             " (run tools/web_site.py --index-only and mount data/; see docs/WEB.md)");
        return false;
    }
    std::size_t count=0;
    std::istringstream lines(text);
    for(std::string line;std::getline(lines,line);){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty()||line==listing)continue;
        if(line.front()=='/'||line.find("..")!=std::string::npos)throw std::runtime_error("invalid path in "+std::string(listing)+": "+line);
        add_entry(root,line,backend);++count;
    }
    status("data",std::string(root)+": "+std::to_string(count)+" files");
    return true;
}
void game_thread(){
    int code=1;bool reported=false;
    try{
        status("data","mounting game data");
        mount_fetch_tree("/data","data","web-files.txt",true);
        const bool mods=mount_fetch_tree("/mods","mods","mods-files.txt",false);
        backend_t opfs=wasmfs_create_opfs_backend();
        if(!opfs||wasmfs_create_directory("/user",0777,opfs)!=0)
            throw std::runtime_error("cannot open the browser's private storage (OPFS); use a current Firefox or Chrome over http://localhost or https");
        status("start","starting the game");
        std::vector<std::string> args{"recvx-runtime","--data","/data","--user-dir","/user"};
        if(mods){args.push_back("--mods");args.push_back("/mods");}
        // Diagnose-Argumente aus der Adresse (?args=--renderer,software): wie die Kommandozeile der Desktop-Runtime.
        if(char* extra=static_cast<char*>(MAIN_THREAD_EM_ASM_PTR({return Module.recvxArgs ? Module.recvxArgs() : 0;}))){
            std::string list(extra);free(extra);
            for(std::size_t start=0;start<=list.size();){const auto comma=list.find(',',start);const auto end=comma==std::string::npos?list.size():comma;
                if(end>start)args.push_back(list.substr(start,end-start));start=end+1;}
        }
        std::vector<char*> argv;for(auto& a:args)argv.push_back(a.data());argv.push_back(nullptr);
        code=recvx_web_runtime_main(int(args.size()),argv.data());
    }catch(const std::exception& e){
        std::fprintf(stderr,"Web startup error: %s\n",e.what());
        status("error",e.what());reported=true;
    }
    // Startfehler stehen schon konkret auf der Seite; sonst Spielende bzw. Laufzeitfehler (Meldung in der Konsole).
    if(!reported)status(code==0?"exit":"error",code==0?"the game has ended; reload the page to start again":"the game stopped with an error (see the browser console)");
}
}

int main(){
    recvx::web_sdl_bind_main_thread();
    std::thread(game_thread).detach();
    // Hauptthread frei fuer Browser-Ereignisse, WebGL2 und WebAudio; die Runtime lebt im Spielthread weiter.
    emscripten_exit_with_live_runtime();
    return 0;
}

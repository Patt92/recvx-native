#include "save_slots.h"
#include "typewriter.h"
#include "user_paths.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <chrono>
#include <system_error>
using namespace recvx;
// Schreibmaschinen-Spielstaende (bup_00.c ControlTypewriter / ps2_SaveScreen)
// als Host-Datei je Slot: Slotverwaltung, Slotauswahl und die gemeinsame
// Benutzerpfad-Aufloesung (AGENTS.md "Benutzerdaten-Ort").
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
class TempDirectory {
public:
    TempDirectory() {
        const auto base=std::filesystem::temp_directory_path();
        const auto nonce=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned attempt=0;;++attempt){
            path_=base/("recvx-save-slots-test-"+std::to_string(nonce)+"-"+std::to_string(attempt));
            if(std::filesystem::create_directory(path_))return;
        }
    }
    ~TempDirectory(){std::error_code ec;std::filesystem::remove_all(path_,ec);}
    const std::filesystem::path& path()const{return path_;}
private:
    std::filesystem::path path_;
};
class CurrentPathGuard {
public:
    explicit CurrentPathGuard(const std::filesystem::path& path):previous_(std::filesystem::current_path()){std::filesystem::current_path(path);}
    ~CurrentPathGuard(){std::error_code ec;std::filesystem::current_path(previous_,ec);}
private:
    std::filesystem::path previous_;
};
int main(){try{
    const TempDirectory temp;
    const auto& root=temp.path();
    std::filesystem::create_directories(root/"saves");
    const auto saves=root/"saves";
    // 15 Original-Slots, anfangs alle leer.
    auto slots=list_save_slots(saves);
    check(slots.size()==kSaveSlots,"Slotanzahl weicht von SelectFileInfo[15] ab");
    for(const auto& s:slots)check(!s.exists,"leerer Slot gilt als belegt");
    // Einen Spielstand (originale SAVEFILE-Struktur) schreiben und zuruecklesen.
    Snapshot snap;snap.save.ply_id=0;snap.save.stg_no=1;snap.save.rom_no=16;snap.save.ply_hp[0]=120;snap.save.stv_tm=4321;snap.host.room="RM_1100.RDX";snap.host.body=10;
    write_save_slot(saves,3,snap);
    check(std::filesystem::is_regular_file(save_slot_path(saves,3)),"Slot-3-Datei fehlt");
    slots=list_save_slots(saves);
    check(slots[2].exists&&slots[2].room=="RM_1100.RDX"&&slots[2].hp==120&&slots[2].play_time==4321,"Slot-3-Kopf falsch");
    const auto back=read_save_slot(saves,3);
    check(back.save.ply_hp[0]==120&&back.host.room=="RM_1100.RDX"&&back.save.stv_tm==4321,"Slot-Roundtrip verliert Daten");
    {bool threw=false;try{save_slot_path(saves,0);}catch(const std::exception&){threw=true;}check(threw,"Slot 0 akzeptiert");}
    {bool threw=false;try{save_slot_path(saves,kSaveSlots+1);}catch(const std::exception&){threw=true;}check(threw,"Slot 16 akzeptiert");}
    // Speicherbildschirm: Cursor auf dem ersten freien Slot; Laden auf dem belegten.
    Typewriter tw;tw.open(Typewriter::Mode::Save,saves);
    check(tw.active()&&tw.slots().size()==kSaveSlots&&tw.cursor()==0,"Speicherbildschirm startet nicht auf dem ersten freien Slot");
    tw.input(Typewriter::Key::Down);check(tw.cursor()==1,"Cursor faellt nicht");
    tw.input(Typewriter::Key::Up);check(tw.cursor()==0,"Cursor steigt nicht");
    tw.input(Typewriter::Key::Confirm);check(tw.done()&&tw.confirmed()&&tw.slot()==1,"Speichern bestaetigt nicht Slot 1");
    tw.reset();check(!tw.active(),"reset laesst den Bildschirm aktiv");
    tw.open(Typewriter::Mode::Load,saves);check(tw.cursor()==2,"Ladebildschirm waehlt nicht den belegten Slot 3");
    tw.input(Typewriter::Key::Confirm);check(tw.confirmed()&&tw.slot()==3,"Laden bestaetigt nicht Slot 3");
    tw.reset();tw.open(Typewriter::Mode::Load,saves);
    tw.input(Typewriter::Key::Up); // Slot 2 ist leer
    check(tw.cursor()==1&&!tw.slots()[1].exists,"Testannahme: Slot 2 ist leer");
    tw.input(Typewriter::Key::Confirm);check(!tw.done()&&!tw.confirmed(),"Laden aus einem leeren Slot wird nicht abgelehnt");
    tw.input(Typewriter::Key::Cancel);check(tw.done()&&tw.cancelled(),"Abbrechen beendet den Bildschirm nicht");
    // Benutzerpfade: explizit > Dev-Baum (Git-Wurzel) > Benutzerverzeichnis.
    const auto dev_root=root/"devtree";std::filesystem::create_directories(dev_root/"data");std::ofstream(dev_root/"CLAUDE.md")<<"x";
    const auto resolved=resolve_user_paths(dev_root/"data");
    check(resolved.dev&&resolved.root==dev_root,"Dev-Arbeitsbaum nicht erkannt");
    recvx::UserPaths relative;
    {CurrentPathGuard cwd(dev_root);relative=resolve_user_paths("data");}
    check(relative.dev&&std::filesystem::equivalent(relative.root,dev_root),"relativer Dev-Datenpfad verliert die Git-Wurzel"); // current_path() loest /tmp -> /private/tmp auf
    const auto release=root/"release";std::filesystem::create_directories(release/"data");
    const auto rel=resolve_user_paths(release/"data");
    check(!rel.dev&&rel.root==platform_user_data_dir(),"Release faellt nicht auf das Benutzerdatenverzeichnis");
    const auto explicit_root=root/"explicit";
    const auto ex=resolve_user_paths(dev_root/"data",explicit_root);
    check(!ex.dev&&ex.root==explicit_root,"expliziter Benutzerpfad hat keinen Vorrang");
    std::cout<<"save slots, typewriter screen and user path resolution passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

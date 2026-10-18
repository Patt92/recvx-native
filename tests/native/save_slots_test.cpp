#include "save_slots.h"
#include "typewriter.h"
#include "user_paths.h"
#include "ui.h"
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
    // Stage 1 Raum 8 = RoomNameSwitch(108) -> saveroom 2 (bup_00.c:124), Type_Space[1] = 29.
    Snapshot snap;snap.save.ply_id=0;snap.save.stg_no=1;snap.save.rom_no=8;snap.save.save_ct=1;snap.save.ply_hp[0]=120;snap.save.stv_tm=4321;snap.host.room="RM_1080.RDX";snap.host.body=10;
    write_save_slot(saves,3,snap);
    check(std::filesystem::is_regular_file(save_slot_path(saves,3)),"Slot-3-Datei fehlt");
    slots=list_save_slots(saves);
    check(slots[2].exists&&slots[2].room=="RM_1080.RDX"&&slots[2].hp==120&&slots[2].play_time==4321&&slots[2].save_room==2,"Slot-3-Kopf falsch");
    const auto back=read_save_slot(saves,3);
    check(back.save.ply_hp[0]==120&&back.host.room=="RM_1080.RDX"&&back.save.stv_tm==4321,"Slot-Roundtrip verliert Daten");
    {std::ofstream broken(save_slot_path(saves,5),std::ios::binary);broken<<"not a snapshot";}
    slots=list_save_slots(saves);
    check(!slots[4].exists&&slots[4].damaged,"beschaedigte Slotdatei wird nicht als defekt erkannt");
    {bool threw=false;try{save_slot_path(saves,0);}catch(const std::exception&){threw=true;}check(threw,"Slot 0 akzeptiert");}
    {bool threw=false;try{save_slot_path(saves,kSaveSlots+1);}catch(const std::exception&){threw=true;}check(threw,"Slot 16 akzeptiert");}
    using Phase=Typewriter::Phase;using Key=Typewriter::Key;
    // Speicherbildschirm, Originalzustaende ps2_SaveScreen.c. Die
    // Memory-Karten-Auswahl (SelectCard, 443–520) ist als Konsolenverhalten
    // entfernt; der Host-Ordner ist ein Speicherort, der Bildschirm startet
    // direkt im SelectFile (1069–1083, lSelectFileNumber 0, nicht der erste freie Slot).
    Typewriter tw;tw.open(Typewriter::Mode::Save,saves);
    check(tw.active()&&tw.slots().size()==kSaveSlots&&tw.phase()==Phase::Files&&tw.cursor()==0&&tw.top()==0,"Speicherbildschirm startet nicht in der Slotliste bei Slot 1");
    tw.input(Key::Down);check(tw.cursor()==1,"Cursor faellt nicht");
    tw.input(Key::Up);check(tw.cursor()==0,"Cursor steigt nicht");
    // Leerer Slot: SaveCursorNew (1270–1322), Meldung 482, Auswahl 0 = speichern.
    tw.input(Key::Confirm);check(tw.phase()==Phase::Confirm&&tw.message()==482&&tw.selection()==0,"leerer Slot fragt nicht mit 482/Ja");
    tw.input(Key::Down);tw.input(Key::Confirm);check(tw.phase()==Phase::Files&&tw.cursor()==0,"Nein kehrt nicht zur Slotliste zurueck");
    // Belegter Slot 3: SaveCursorOld (1215–1267), Meldung 483, Auswahl 1 = nicht ueberschreiben.
    tw.input(Key::Down);tw.input(Key::Down);check(tw.cursor()==2&&tw.slots()[2].exists,"Testannahme: Slot 3 ist belegt");
    tw.input(Key::Confirm);check(tw.phase()==Phase::Confirm&&tw.message()==483&&tw.selection()==1,"belegter Slot fragt nicht mit 483/Nein");
    tw.input(Key::Cancel);check(tw.phase()==Phase::Files&&tw.cursor()==2,"Cancel in der Rueckfrage verlaesst die Slotliste");
    tw.input(Key::Confirm);tw.input(Key::Up);check(tw.selection()==0,"Richtung schaltet die Rueckfrage nicht um");
    tw.input(Key::Confirm);check(tw.phase()==Phase::Io&&!tw.io_pending()&&tw.message()==484,"Ja startet nicht den Schreibzustand (484)");
    tw.control(0u,0u);check(tw.io_pending()&&tw.slot()==3,"Schreibzustand fordert die Host-Datei nicht an");
    check(!tw.take_save_complete(),"I/O-Anforderung meldet bereits den Speicherabschluss");
    tw.io_complete(true,saves);
    check(tw.phase()==Phase::Typing&&tw.confirmed()&&tw.write_count()==0,"erfolgreiches Schreiben startet die Schreibanimation nicht");
    check(!tw.take_save_complete(),"erfolgreiches Datei-I/O erneuert den Continue-Punkt vor der Tippanimation");
    // ps2_McSaveFile.c:571–584: Zeichen n im Pass 1+6(n-1) (sWaitMesTimer % 6), bei
    // slDispWriteMes >= Max im Folgepass Endton 0x80000244 und ++; mcGetStringEnd (849)
    // beendet bei > Max vor dem Zeichnen: Max 29 -> Erfolg im Pass 6*29-3 = 171.
    {unsigned passes=0;while(tw.phase()==Phase::Typing&&passes<600){
        tw.control(0u,0u);++passes;
        check(tw.take_save_complete()==(passes==6*29-3),"Continue-Punkt wird nicht genau beim Ende der Tippanimation angefordert");
        check(!tw.take_save_complete(),"Speicherabschluss wird mehrfach geliefert");}
     check(tw.phase()==Phase::Success&&tw.message()==485&&passes==6*29-3,"Schreibanimation endet nicht nach Type_Space[1] (29) Zeichen im Pass 171 mit Meldung 485");}
    for(unsigned i=0;i<20;++i){tw.control(0u,0u);check(tw.phase()==Phase::Success,"Erfolgsmeldung haelt keine 20 Bilder (SuccessCardWriteMessage)");check(!tw.take_save_complete(),"Erfolgsmeldung erneuert den Continue-Punkt wiederholt");}
    tw.control(0u,0u);check(tw.phase()==Phase::Exit&&!tw.done(),"Erfolg geht nicht in den Verlassen-Zustand");
    tw.control(0u,0u);check(tw.done()&&tw.confirmed()&&!tw.cancelled(),"Verlassen ohne ev_flg 56 beendet den Bildschirm nicht im naechsten Bild");
    tw.reset();check(!tw.active(),"reset laesst den Bildschirm aktiv");
    // Schreibfehler: ErrCardWrite (1443–1464), Meldung 486, OK/Cancel zurueck zur Slotliste.
    tw.open(Typewriter::Mode::Save,saves);tw.input(Key::Confirm);tw.input(Key::Confirm);tw.input(Key::Confirm);tw.control(0u,0u);
    tw.io_complete(false,saves);check(tw.phase()==Phase::Error&&tw.message()==486,"Schreibfehler zeigt nicht 486");
    check(!tw.take_save_complete(),"Schreibfehler erneuert den Continue-Punkt");
    tw.input(Key::Confirm);check(tw.phase()==Phase::Files&&tw.cursor()==0,"Fehler kehrt nicht zur Slotliste zurueck");
    // Verlassen mit ev_flg 56: SpecialSave (536–604), Meldung 253, Auswahl 1 = zurueck zur Slotliste.
    tw.input(Key::Cancel);check(tw.phase()==Phase::Exit&&tw.cancelled(),"Cancel in der Slotliste setzt den Exit-Zustand nicht");
    tw.control(0u,0u,true);check(tw.phase()==Phase::SpecialSave&&tw.selection()==1&&tw.message()==253&&!tw.done(),"ev_flg 56 oeffnet die Sonderspeicher-Rueckfrage nicht");
    tw.input(Key::Confirm);check(tw.phase()==Phase::Files&&!tw.done(),"Sonderspeicher-Rueckfrage 'zurueck' fuehrt nicht zur Slotliste");
    tw.input(Key::Cancel);tw.control(0u,0u,true);check(tw.phase()==Phase::SpecialSave,"Cancel in der Slotliste umgeht die Sonderspeicher-Rueckfrage");
    tw.input(Key::Cancel);check(tw.phase()==Phase::Files,"Cancel in der Sonderspeicher-Rueckfrage fuehrt nicht zur Slotliste");
    tw.input(Key::Cancel);tw.control(0u,0u,true);tw.input(Key::Up);check(tw.selection()==0,"Richtung schaltet die Sonderspeicher-Rueckfrage nicht um");
    tw.input(Key::Confirm);check(tw.done()&&tw.cancelled(),"Sonderspeicher-Rueckfrage 'verlassen' setzt usExitFlag nicht sofort");
    tw.reset();
    // Ladebildschirm, ps2_LoadScreen.c: SelectCard (334–411) entfällt als
    // Konsolenverhalten; DirCheck -> SelectFile (650–665, Slot 1), leerer Slot
    // -> NoSave (787–808), belegter -> LoadCursor (811–865).
    tw.open(Typewriter::Mode::Load,saves);check(tw.phase()==Phase::Files&&tw.cursor()==0,"Ladebildschirm startet nicht in der Slotliste bei Slot 1");
    tw.input(Key::Confirm);check(tw.phase()==Phase::NoSave&&tw.message()==458&&!tw.confirmed(),"Laden aus einem leeren Slot wird nicht mit 458 abgelehnt");
    tw.input(Key::Cancel);check(tw.phase()==Phase::Files&&tw.cursor()==0,"NoSave kehrt nicht zur Slotliste zurueck");
    tw.input(Key::Down);tw.input(Key::Down);tw.input(Key::Confirm);check(tw.phase()==Phase::Confirm&&tw.message()==459&&tw.selection()==0,"belegter Ladeslot fragt nicht mit 459/Ja");
    tw.input(Key::Confirm);tw.control(0u,0u);check(tw.io_pending()&&tw.slot()==3,"Laden fordert Slot 3 nicht an");
    tw.io_complete(true,saves);tw.control(0u,0u);check(tw.done()&&tw.confirmed()&&tw.slot()==3,"Laden bestaetigt nicht Slot 3");
    tw.reset();tw.open(Typewriter::Mode::Load,saves);tw.input(Key::Confirm);
    tw.input(Key::Cancel);check(tw.phase()==Phase::Files&&!tw.done(),"Cancel verlaesst die NoSave-Meldung nicht zur Slotliste (LoadScreen 743–747)");
    tw.input(Key::Cancel);check(tw.phase()==Phase::Exit&&!tw.done(),"Cancel in der Slotliste fuehrt nicht in den Verlassen-Zustand");
    tw.control(0u,0u);check(tw.done()&&tw.cancelled(),"Verlassen beendet den Ladebildschirm nicht (TitleExit)");
    // Ein defekter Slot sperrt nicht die ganze Liste: Laden meldet den Defekt
    // erst bei Auswahl; Speichern verlangt vor dem Ersetzen die uebliche 483-Abfrage.
    Typewriter damaged_load;damaged_load.open(Typewriter::Mode::Load,saves);
    check(damaged_load.phase()==Phase::Files&&damaged_load.slots()[4].damaged,"defekter Slot blockiert bereits beim Oeffnen");
    for(unsigned i=0;i<4;++i)damaged_load.input(Key::Down);
    damaged_load.input(Key::Confirm);
    check(damaged_load.phase()==Phase::Error&&damaged_load.message()==461,"Laden eines defekten Slots zeigt keinen Lesefehler");
    damaged_load.input(Key::Confirm);
    check(damaged_load.phase()==Phase::Files&&damaged_load.cursor()==4,"Lesefehler kehrt nicht zum betroffenen Slot zurueck");
    Typewriter damaged_save;damaged_save.open(Typewriter::Mode::Save,saves);
    for(unsigned i=0;i<4;++i)damaged_save.input(Key::Down);
    damaged_save.input(Key::Confirm);
    check(damaged_save.phase()==Phase::Confirm&&damaged_save.message()==483&&damaged_save.selection()==1,"defekter Slot wird beim Speichern nicht als belegt behandelt");
    damaged_save.input(Key::Confirm);
    check(damaged_save.phase()==Phase::Files&&damaged_save.cursor()==4,"Standard-Nein ueberschreibt den defekten Slot");
    damaged_save.input(Key::Confirm);damaged_save.input(Key::Up);damaged_save.input(Key::Confirm);
    check(damaged_save.phase()==Phase::Io,"bestaetigtes Ersetzen startet den Schreibvorgang nicht");
    damaged_save.control(0u,0u);write_save_slot(saves,5,snap);damaged_save.io_complete(true,saves);
    check(damaged_save.phase()==Phase::Typing&&damaged_save.slots()[4].exists&&!damaged_save.slots()[4].damaged,"defekter Slot wird nach erfolgreichem Ersatz nicht als gueltig erkannt");
    // SelectFile: ps2_SaveScreen.c:1086-1169 / ps2_LoadScreen.c:667-748.
    // Eigene Wiederholung bei gehaltenem Pad: Bilder 1, 8, 11; keine Host-Repeats.
    for(const auto mode:{Typewriter::Mode::Save,Typewriter::Mode::Load}){
        Typewriter pad;pad.open(mode,saves);
        check(pad.phase()==Phase::Files,"Bildschirm startet nicht in der Slotliste");
        const unsigned initial=pad.cursor();
        for(unsigned frame=1;frame<=11;++frame){
            pad.control(2u,0u);
            const unsigned moves=frame<8?1u:frame<11?2u:3u;
            check(pad.cursor()==(initial+moves)%kSaveSlots,"SelectFile KeyWait00/01 weicht vom Original ab");
        }
        pad.control(0u,0u);
        pad.control(1u,0u);
        check(pad.cursor()==(initial+2u)%kSaveSlots,"Loslassen setzt die Richtungszaehler nicht zurueck");
        pad.control(2u,0x1000u);
        check(pad.phase()==Phase::Exit&&pad.cursor()==(initial+2u)%kSaveSlots&&!pad.done(),"Cancel muss die Richtung im selben Bild sperren und in den Verlassen-Zustand fuehren");
    }
    {
        Typewriter pad;pad.open(Typewriter::Mode::Save,saves);
        pad.control(2u,0x1800u);
        check(pad.phase()==Phase::Confirm&&pad.cursor()==0,"OK hat Vorrang vor Cancel und Richtungsbewegung");
    }
    {
        Typewriter pad;pad.open(Typewriter::Mode::Load,saves);
        pad.control(1u,0u);
        pad.control(0u,0x800u);
        check(pad.phase()==Phase::NoSave&&!pad.confirmed(),"Pad-OK akzeptiert einen leeren Ladeslot");
        pad.control(0u,0x1000u);
        check(pad.phase()==Phase::Files&&!pad.done(),"Pad-Cancel verlaesst die NoSave-Meldung nicht zur Slotliste");
    }
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

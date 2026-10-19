// Laengen der Skriptbefehle, die der Port in diesem Stand nur ueberspringt oder neu
// bindet. Ein unbekannter Befehl wirft in EventVm und haelt den Raum an; eine falsche
// Laenge verschiebt dagegen den ganzen Strom dahinter. Beides faellt hier auf: der Test
// legt den Befehl an den Anfang eines Eventskripts, setzt dahinter ein bhSet (event.c:433,
// sechs Byte) und schliesst mit bhEnd. Nur wenn der Befehl genau seine Originallaenge
// ueberspringt, landet die Ausfuehrung auf dem bhSet und das Flag steht.
//
// Die Skriptbytes sind hier gebaut, nicht aus dem Raum gelesen: der Raum liefert nur ein
// gueltiges Skripttabellenlayout. Das ist eine Pruefvorrichtung, keine Aussage ueber den
// Inhalt von RM_0040.
#include "events.h"
#include "room.h"
#include "asset_io.h"
#include "binary_view.h"
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <memory>
#include <vector>
using namespace recvx;
static void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}

// Jede Zeile: Befehl, Originallaenge, Fundstelle.
struct Case {unsigned op;std::size_t length;const char* where;};
static const Case cases[]={
    {0x1e,6,  "event.c:1523 bhAdxTimeCk"},
    {0x2e,2,  "event.c:1827 bhMotionPauseSetPly"},
    {0x3a,2,  "event.c:3465 bhMutekiSetPl"},
    {0x40,8,  "event.c:3488 bhLookGsetPlStart"},
    {0x41,2,  "event.c:3539 bhLookGsetPlStop"},
    {0x46,8,  "event.c:2574 bhSetNextRoomBgm"},
    {0x47,10, "event.c:2612 bhSetNextRoomBgSe"},
    {0x6f,12, "event.c:5263 bhObjLinkSetObjPly"},
    {0x82,4,  "event.c:2421 bhSetDebugLoopEx"},
    {0x97,8,  "event.c:1986 bhEffectKokuenSet"},
    {0x98,12, "event.c:4110 bhEffectSandSet"},
    {0xaa,10, "event.c:7159 bhEffUVSet2"},
    {0xa1,2,  "event.c:6849 bhTrapDamageSet"},
    {0xaf,4,  "event.c:7378 bhPlyHandChange"},
    {0xc3,4,  "event.c:8577 bhEffectSandSetMatsumoto"},
    {0xd3,2,  "event.c:9049 bhRankingCall"},
};
int main(int argc,char** argv){try{
    if(argc<2)return 0;
    const std::filesystem::path data=argv[1];
    const auto original=read_asset_file(data/"rooms/RM_0040.RDT",64*1024*1024);
    const auto base=inspect_room(original).event_offset;
    check(base+8+4<original.size(),"RM_0040: Eventtabelle fehlt");
    const unsigned event=1;
    BinaryView probe(original);
    const auto script=base+probe.u32(base+8+event*4);
    check(script<original.size(),"RM_0040: Skript 1 ausserhalb des Raums");
    for(const auto& c:cases){
        check(script+c.length+7<original.size(),std::string(c.where)+": kein Platz fuer die Pruefvorrichtung");
        auto room=original;
        for(std::size_t i=0;i<c.length;++i)room[script+i]=0;
        room[script]=std::uint8_t(c.op);
        // 0x1E ist eine Bedingung: mit Modus 1 (Zeit <= Grenze) und der Stubzeit 0 liefert sie
        // wahr, sodass der Lauf das bhSet dahinter erreicht und die Laenge messbar bleibt.
        if(c.op==0x1e)room[script+4]=0x01;
        // bhSet Matrix 1, Bit 300, Modus 0 (setzen).
        const std::size_t at=script+c.length;
        room[at]=0x05;room[at+1]=0x01;room[at+2]=0x2C;room[at+3]=0x01;room[at+4]=0x00;room[at+5]=0x00;
        room[at+6]=0x00;                       // bhEnd
        Inventory inventory;MessageDialog dialog;EventVm vm(room,inventory,dialog);
        vm.adx_time=[](unsigned){return 0u;};
        const auto pending_before=vm.pending_bindings.size();
        vm.start(0,event);
        vm.tick();
        check(vm.flag(1,300),std::string(c.where)+": das bhSet dahinter wurde nicht erreicht (Laenge "+std::to_string(c.length)+" stimmt nicht)");
        static const unsigned bound[]={0x1e,0x2e,0x3a,0x40,0x41,0x46,0x47,0xa1,0x98,0xc3};
        bool is_bound=false;for(unsigned b:bound)if(b==c.op)is_bound=true;
        if(!is_bound)
            {bool reported=false;for(const auto& b:vm.pending_bindings)if(b.opcode==c.op)reported=true;
             check(reported,std::string(c.where)+": der Befehl muss als offene Bindung gemeldet werden");(void)pending_before;}
    }
    // Die drei Befehle, die direkt auf die Flagbaenke der Event-VM schreiben.
    {
        struct Run {Inventory inventory;MessageDialog dialog;std::unique_ptr<EventVm> vm;};
        const auto run=[&](unsigned op,std::uint8_t arg){
            auto room=original;
            room[script]=std::uint8_t(op);room[script+1]=arg;
            const std::size_t at=script+2;
            room[at]=0x05;room[at+1]=0x01;room[at+2]=0x2C;room[at+3]=0x01;room[at+4]=0x00;room[at+5]=0x00;room[at+6]=0x00;
            auto r=std::make_unique<Run>();
            r->vm=std::make_unique<EventVm>(room,r->inventory,r->dialog);
            r->vm->start(0,event);r->vm->tick();
            check(r->vm->flag(1,300),"Flagbefehl: das bhSet dahinter wurde nicht erreicht");
            return r;};
        // bhMotionPauseSetPly: sp_flg Bit 0x1 (Tor des Spielerpasses).
        {auto r=run(0x2e,1);check(r->vm->flags[6][0]&1u,"0x2e Wert 1 setzt sp_flg 0x1 nicht");}
        {auto r=run(0x2e,0);check(!(r->vm->flags[6][0]&1u),"0x2e Wert 0 loescht sp_flg 0x1 nicht");}
        // bhMutekiSetPl: plp->flg Bit 0x4, dazu die Spiegelung in player_flags.
        {auto r=run(0x3a,1);check((r->vm->flags[13][0]&4u)&&(r->vm->player_flags&4u),"0x3a Wert 1 setzt plp->flg 0x4 nicht");}
        {auto r=run(0x3a,0);check(!(r->vm->flags[13][0]&4u),"0x3a Wert 0 loescht plp->flg 0x4 nicht");}
        // bhTrapDamageSet: plp->stflg Bit 0x1000, im Original mit Wert 0 GESETZT.
        {auto r=run(0xa1,0);check(r->vm->flags[14][0]&0x1000u,"0xa1 Wert 0 setzt plp->stflg 0x1000 nicht");}
        {auto r=run(0xa1,1);check(!(r->vm->flags[14][0]&0x1000u),"0xa1 Wert 1 loescht plp->stflg 0x1000 nicht");}
    }
    {
        // sdfunc.c:90 NextSoundInfo: bhSetNextRoomBgm (0x46) und bhSetNextRoomBgSe (0x47)
        // schreiben Musik- und Raumtonvorgaben fuer den naechsten Raum. Die Struktur hat im
        // Original keinen Leser; geprueft wird deshalb nur, dass genau die Originalfelder aus
        // den richtigen Bytes entstehen.
        const auto run=[&](const std::vector<std::uint8_t>& command){
            auto room=original;
            for(std::size_t i=0;i<command.size();++i)room[script+i]=command[i];
            const std::size_t at=script+command.size();
            room[at]=0x05;room[at+1]=0x01;room[at+2]=0x2C;room[at+3]=0x01;room[at+4]=0x00;room[at+5]=0x00;room[at+6]=0x00;
            struct Run {Inventory inventory;MessageDialog dialog;std::unique_ptr<EventVm> vm;};
            auto r=std::make_unique<Run>();
            r->vm=std::make_unique<EventVm>(room,r->inventory,r->dialog);
            r->vm->start(0,event);r->vm->tick();
            check(r->vm->flag(1,300),"NextSoundInfo: das bhSet dahinter wurde nicht erreicht");
            return r;};
        // Byte 1 bleibt ungelesen (0xEE), danach Com 7, Point 9, Ofs 0x0123, Nummer 5, Fade 3*10.
        {auto r=run({0x46,0xEE,0x07,0x09,0x23,0x01,0x05,0x03});auto& n=r->vm->next_sound;
         check(n.bgm_com==7&&n.bgm_point==9&&n.bgm_offset==0x0123u&&n.bgm_number==5&&n.bgm_fade==30,
               "0x46: Felder von NextSoundInfo falsch belegt");}
        // Platz 1: Com 4, Point 6, Ofs 0x0210, Nummer 0x0405, Fade 2*10, Flag aus Byte 9 == 0.
        {auto r=run({0x47,0x01,0x04,0x06,0x10,0x02,0x05,0x04,0x02,0x00});auto& n=r->vm->next_sound;
         check(n.se_com[1]==4&&n.se_point[1]==6&&n.se_offset[1]==0x0210u&&n.se_number[1]==0x0405u&&n.se_fade[1]==20&&n.se_set[1]==1,
               "0x47 Platz 1: Felder falsch belegt");
         check(n.se_com[0]==0&&n.se_number[0]==0,"0x47: der andere Platz darf unberuehrt bleiben");}
        // Byte 9 ungleich 0 setzt das Flag auf 0.
        {auto r=run({0x47,0x00,0x04,0x06,0x10,0x02,0x05,0x04,0x02,0x01});
         check(r->vm->next_sound.se_set[0]==0,"0x47: Byte 9 ungleich 0 muss das Flag loeschen");}
        // Ein Platz ausserhalb der zwei Originalfelder wird gemeldet statt geschrieben.
        {auto r=run({0x47,0x05,0x04,0x06,0x10,0x02,0x05,0x04,0x02,0x00});
         bool reported=false;for(const auto& c:r->vm->pending_bindings)if(c.opcode==0x47)reported=true;
         check(reported,"0x47: Platz ausserhalb muss als offene Bindung gemeldet werden");
         check(r->vm->next_sound.se_com[0]==0&&r->vm->next_sound.se_com[1]==0,"0x47: ausserhalb darf nichts geschrieben werden");}
    }
    {
        // event.c:1523 bhAdxTimeCk: die sechs Vergleichsmodi. Der Befehl ist eine Bedingung;
        // der Ereignislauf macht nur bei einem Rueckgabewert ungleich 0 weiter (event.c:13046),
        // das bhSet dahinter laeuft also genau dann, wenn der Befehl wahr liefert. Die Modi 0..3
        // liefern 0, WENN die Bedingung zutrifft, die Modi 4 und 5 liefern 1 — beides zusammen
        // ergibt die Tabelle unten. Ein Modus ueber 5 gibt die rohe Zeit zurueck.
        const auto cond=[&](unsigned time,unsigned limit,unsigned mode){
            auto room=original;
            room[script]=0x1E;room[script+1]=0x01;
            room[script+2]=std::uint8_t(limit&0xFF);room[script+3]=std::uint8_t(limit>>8);
            room[script+4]=std::uint8_t(mode);room[script+5]=0x00;
            const std::size_t at=script+6;
            room[at]=0x05;room[at+1]=0x01;room[at+2]=0x2C;room[at+3]=0x01;room[at+4]=0x00;room[at+5]=0x00;room[at+6]=0x00;
            struct Run {Inventory inventory;MessageDialog dialog;std::unique_ptr<EventVm> vm;};
            Run r;r.vm=std::make_unique<EventVm>(room,r.inventory,r.dialog);
            unsigned asked=~0u;
            r.vm->adx_time=[&](unsigned slot){asked=slot;return time;};
            r.vm->start(0,event);r.vm->tick();
            check(asked==1,"0x1E: der Platz aus Byte 1 kommt nicht am Mischer an");
            return r.vm->flag(1,300);};
        //                                 T    L   Modus   erwartet wahr
        check( !cond(500,500,0),"0x1E Modus 0 bei T == L");
        check(  cond(500,400,0),"0x1E Modus 0 bei T != L");
        check(  cond(500,500,1),"0x1E Modus 1 bei T <= L");
        check( !cond(500,400,1),"0x1E Modus 1 bei T > L");
        check( !cond(500,500,2),"0x1E Modus 2 bei T == L");
        check(  cond(300,400,2),"0x1E Modus 2 bei T < L");
        check(  cond(500,500,3),"0x1E Modus 3 bei T >= L");
        check( !cond(300,400,3),"0x1E Modus 3 bei T < L");
        check( !cond(500,500,4),"0x1E Modus 4 bei T == L");
        check(  cond(500,400,4),"0x1E Modus 4 bei T > L");
        check(  cond(500,500,5),"0x1E Modus 5 bei T == L");
        check( !cond(500,400,5),"0x1E Modus 5 bei T != L");
        // Modus ueber 5: das Original hat keinen default und gibt die rohe Zeit zurueck.
        check(  cond(500,400,6),"0x1E Modus 6 mit Zeit ungleich 0 muss weiterlaufen");
        check( !cond(0,400,6),  "0x1E Modus 6 mit Zeit 0 muss abbrechen");
    }
    // Gegenprobe der Vorrichtung selbst: ein um ein Byte verschobenes bhSet darf das Flag
    // nicht setzen, sonst wuerde der Test jede Laenge durchwinken.
    {
        auto room=original;
        for(std::size_t i=0;i<2;++i)room[script+i]=0;
        room[script]=0x41;                      // zwei Byte
        const std::size_t at=script+3;          // bewusst ein Byte zu spaet
        room[script+2]=0x00;                    // bhEnd direkt hinter dem Befehl
        room[at]=0x05;room[at+1]=0x01;room[at+2]=0x2C;room[at+3]=0x01;room[at+4]=0x00;room[at+5]=0x00;room[at+6]=0x00;
        Inventory inventory;MessageDialog dialog;EventVm vm(room,inventory,dialog);
        vm.start(0,event);vm.tick();
        check(!vm.flag(1,300),"Pruefvorrichtung: ein verschobenes bhSet darf nicht ausgefuehrt werden");
    }
    std::cout<<"event opcode lengths ok\n";return 0;
}catch(const std::exception& e){std::cerr<<"event opcode lengths: "<<e.what()<<'\n';return 1;}}

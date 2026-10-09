#include "inspection.h"
#include "inventory.h"
#include "ninja_math.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
using namespace recvx;
// Inventarabdeckung: Anzeigeposen und Tabellen, die Untersuchung aller 156
// Items mit ihren Umwandlungen (D03), ItemUse-Klassen, alle zwölf
// Kombinationsselektoren, ItemSearch/EraseItem/ItemSort und die Dateitabellen.
// Alle Mechanismen liegen eigenständig nativ vor (native/inspection.cpp,
// native/inventory.cpp); dieser Test fordert ihre Originalwirkung ein.
static void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    _Make_SinTable();
    // Weiterhin portiert: DrawSubItem rotmat aus DSP_WORK, unabhängig
    // nachgerechnet, einschließlich der zusätzlichen Aktionsrotationen.
    for(unsigned item=0;item<156;++item){
        NJS_MATRIX expected,actual;njUnitMatrix(&expected);const auto& d=dsptbl[item];
        njRotateX(&expected,int(-d.ax*182.04445f)&65535);njRotateY(&expected,int(-d.ay*182.04445f)&65535);njRotateZ(&expected,int(-d.az*182.04445f)&65535);njTransposeMatrix(&expected);
        inspection_rotation(actual,inspection_default_angles(item));
        for(unsigned j=0;j<16;++j)check(std::abs(expected[j]-actual[j])<.002f,"item display matrix differs from original: "+std::to_string(item));
        const std::array<Angle,3> spin={7000,11000,3000};
        njRotateX(&expected,spin[0]);njRotateY(&expected,spin[1]);njRotateZ(&expected,spin[2]);
        inspection_rotation(actual,inspection_default_angles(item),spin);
        for(unsigned j=0;j<16;++j)check(std::abs(expected[j]-actual[j])<.002f,"item action rotations composed in wrong order: "+std::to_string(item));
        // Reine Tabellenabfragen: Aktionsindex und Modellnummer.
        check(inspection_action(item)<4,"item action index outside the original table: "+std::to_string(item));
        check(inspection_model_id(item)<160,"item model id outside the original table: "+std::to_string(item));
    }
    // D03 Untersuchung: Richtungsfenster, Dietrichsperre, Frage, Aktion, beide
    // Zoomphasen, Umwandlung und Ergebnismeldung für jedes Item mit einem
    // Untersuchungsfall. Erwartungswerte aus itemflg/c_a/mestbl/idsettbl/
    // getmestbl (reine Datentabellen).
    {unsigned inspections=0,transformations=0;
     auto fixture=[](unsigned item){Inventory inv;std::fill(inv.slots()+1,inv.slots()+12,0);inv.slots()[1]=Inventory::pack(55,1);inv.slots()[2]=Inventory::pack(item,itemdata[item].max);inv.slots()[3]=Inventory::pack(50,1);return inv;};
     for(unsigned item=0;item<156;++item){
        const auto group=unsigned(dsptbl[item].hide);const auto first=itemflg[group][4]&15,count=itemflg[group][4]>>4;
        if(!item)continue;
        for(unsigned direction=0;direction<std::max(1u,unsigned(count));++direction){
            std::array<Angle,3> angles{};if(first)for(unsigned a=0;a<3;++a)angles[a]=c_a[first+direction][a*2+1];
            auto inv=fixture(item);const auto before=inv.items;ItemInspection state;state.check(inv,2,angles);++inspections;
            if(state.message_pending())(void)state.take_message();
            if(state.question()){state.answer(inv,false);check(!state.running()&&inv.items==before,"No modifies item "+std::to_string(item));
                state.check(inv,2,angles);if(state.message_pending())(void)state.take_message();state.answer(inv,true);}
            if(state.running()){unsigned frames=0;
                while(state.running()&&frames<130){const auto unchanged=inv.items;const auto phase=state.phase();state.tick(inv);
                    if(state.running())check(inv.items==unchanged,"early inspection commit "+std::to_string(item)+" phase "+std::to_string(phase));++frames;}
                check(!state.running(),"inspection stalls for item "+std::to_string(item));
                const auto replacement=idsettbl[itemflg[group][6]];
                check(Inventory::id(inv.slots()[2])==replacement&&Inventory::quantity(inv.slots()[2])==itemdata[replacement].max,"wrong transformation "+std::to_string(item));
                check(inv.contains(50),"transformation consumes lockpick");
                check(state.message_pending()&&state.take_message()==getmestbl[itemflg[group][6]],"wrong result message "+std::to_string(item));++transformations;}
        }
     }
     check(inspections>=156&&transformations==11,"inspection/transformation coverage incomplete: "+std::to_string(inspections)+"/"+std::to_string(transformations));}
    // Dietrichsperre: ohne Item 50 meldet der Behälter 188 und startet nichts.
    for(unsigned item:{63u,128u,129u}){Inventory inv;std::fill(inv.slots()+1,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(item,1);
     const auto before=inv.items;ItemInspection state;state.check(inv,2,{});
     check(state.message_pending()&&state.take_message()==188&&!state.running()&&!state.question()&&inv.items==before,"lockpick gate failed for "+std::to_string(item));}
    // Item 106 startet direkt ohne Frage und ohne Meldung.
    {Inventory inv;std::fill(inv.slots()+1,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(106,1);inv.slots()[3]=Inventory::pack(50,1);
     ItemInspection state;state.check(inv,2,{0,16384,0});
     check(state.running()&&!state.question()&&!state.message_pending(),"security file must start directly, without question");}
    // Item 72: Abbrechen identifiziert nichts, die Untersuchung schon.
    {Inventory inv;std::fill(inv.slots()+1,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(72,1);
     ItemInspection state;state.cancel(inv,2);check(inv.slots()[2]==Inventory::pack(72,1),"cancel identifies unknown object");
     state.check(inv,2,{});check(inventory_name_id(inv.slots()[2])==158,"inspection does not reveal original name");}
    // Item 30: Abbrechen normalisiert die Magnummunition auf Item 13.
    {Inventory inv;std::fill(inv.slots()+1,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(30,6);
     ItemInspection state;state.cancel(inv,2);check(inv.slots()[2]==Inventory::pack(13,6),"magnum case ammo exit failed");}
    // Namensauswahl des Statusbildschirms.
    check(inventory_name_id(Inventory::pack(82,0))==149&&inventory_name_id(Inventory::pack(82,1))==82&&inventory_name_id(Inventory::pack(145,1))==159,"dynamic inventory names failed");
    // Sichtbarkeitsmaske und Aktionspose brauchen ein echtes Paket; ohne
    // Datenpfad bleibt die reine Eingangsprüfung.
    {bool rejected=false;try{(void)inspection_geometry(std::vector<std::uint8_t>(64,0),63,22);}catch(const std::exception&){rejected=true;}
     check(rejected,"geometry must reject a packet without a model");}
    // Dateitabellen: 24 Dateien, 167 Seiten, Hintergrund und Zuordnung.
    {unsigned pages=0;for(unsigned file=0;file<24;++file){pages+=original_file_pages(file);
        check(original_file_wallpaper(file)>=146&&original_file_wallpaper(file)<=159,"file wallpaper outside the original ITEM range");
        for(unsigned page=0;page<original_file_pages(file);++page)check(original_file_message(file,page)>=280,"file page message below the original base");
        bool rejected=false;try{(void)original_file_message(file,original_file_pages(file));}catch(const std::exception&){rejected=true;}
        check(rejected,"file page beyond the original count was accepted");}
     check(pages==167,"original file page total differs: "+std::to_string(pages));}
    check(original_file_number(85)==2&&original_file_number(48,0,7)==8&&original_file_number(47,1,8)==10&&original_file_number(133,9,18)==23,"contextual file-number mapping failed");
    check(original_file_number(132,6,4)==17&&original_file_number(0,5,51)==11,"room-bound file mapping failed");
    check(!original_file_number(48,0,0),"file number invented outside its room");
    // Reine Raummodelle einer Datei kommen nicht ins Inventar; TG-01 und das
    // Handbuch schon.
    for(unsigned item=1;item<156;++item){Inventory inv;const auto before=inv.items;const auto mask=original_file_model_mask(item);
     if(mask)check(!inv.debug_acquire(item,1)&&inv.items==before,"file-only room model inserted by debug catalog: "+std::to_string(item));
     else if(item<=145)check(inv.debug_acquire(item,1),"original item type rejected by debug catalog: "+std::to_string(item));}
    check(!original_file_model_mask(85)&&!original_file_model_mask(145),"TG-01/manual must stay inventory items");
    check(original_file_model_mask(132)==((1u<<17)|(1u<<1)|(1u<<4)|(1u<<7)),"item 132 file set differs");
    {Inventory inv;inv.items[383]=0;for(unsigned file=0;file<24;++file){const auto before=inv.items;check(inv.debug_acquire(256+file,999),"file catalog grant failed");
        for(unsigned i=0;i<383;++i)check(inv.items[i]==before[i],"file grant consumed an inventory/box slot");}
     check(inv.items[383]==0xffffff,"catalog does not cover every original file");}
    // Ab hier: positive Originalwirkung des Inventarpakets. Jeder Abschnitt
    // läuft einzeln, Fehlschläge werden gesammelt.
    std::vector<std::string> failures;
    auto section=[&](const char* name,const std::function<void()>& body){try{body();}catch(const std::exception& e){failures.push_back(std::string(name)+": "+e.what());}};
    auto clean=[]{Inventory inv;std::fill(inv.slots(),inv.slots()+12,0);return inv;};
    // Ist-Zustand pip[0..11] für die Fehlermeldung.
    auto dump=[](const Inventory& inv){std::string out=" [pip";char b[16];for(unsigned i=0;i<12;++i){std::snprintf(b,sizeof b," %x",inv.slots()[i]);out+=b;}return out+"]";};
    // D07 ItemUse: Klasse itemdata.type & 0x5F. Use_02 (Typ 0x4, Item 19) meldet
    // 161 und verbraucht nichts; Use_05 (Waffe Typ 0x1) ohne Bodentrigger 161.
    section("D07 Use_02/Use_05 ohne Trigger",[&]{
        for(unsigned item:{19u,5u,84u}){auto inv=clean();inv.slots()[2]=Inventory::pack(item,1);const auto before=inv.items;unsigned control=0;
            const auto u=inv.use_item(2,nullptr,control,0,0,0);
            check(u.message==161&&u.useon==2&&!u.closing&&inv.items==before,"ItemUse without a floor trigger differs (161, useon 2, no change): "+std::to_string(item));
}});
    // Use_01 (Typ 0x2, Munition/Dokument): ohne Trigger 160, Bogen 18/36 197;
    // Trigger ohne Treffer: prm0 59/85 -> 174, sonst 160/197; Treffer ->
    // StatusCancel (closing), kein Verbrauch.
    section("D07 Use_01",[&]{
        const std::array<unsigned,4> other{90,91,92,93},tg01{59,0,0,0},hit{0,12,0,0};
        for(unsigned item:{12u,18u,36u,145u}){const unsigned plain=(item==18||item==36)?197u:160u;
            {auto inv=clean();inv.slots()[2]=Inventory::pack(item,1);unsigned c=0;const auto u=inv.use_item(2,nullptr,c,0,0,0);check(u.message==plain&&!u.closing,"Use_01 without trigger message: "+std::to_string(item));}
            {auto inv=clean();inv.slots()[2]=Inventory::pack(item,1);unsigned c=0;const auto u=inv.use_item(2,&other,c,0,0,0);check(u.message==plain&&!u.closing,"Use_01 unmatched trigger message: "+std::to_string(item));}
            {auto inv=clean();inv.slots()[2]=Inventory::pack(item,1);unsigned c=0;const auto u=inv.use_item(2,&tg01,c,0,0,0);check(u.message==174&&!u.closing,"Use_01 on the 59/85 machine needs 174: "+std::to_string(item));}}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(12,30);const auto before=inv.items;unsigned c=0;const auto u=inv.use_item(2,&hit,c,0,0,0);
            check(u.closing&&!u.message&&inv.items==before&&(c&0x600)==0x600,"Use_01 matching trigger must close (StatusCancel) without consuming and keep cb_flg 0x400");}});
    // Use_05 (Typ 0x40/0x1) am Bodentrigger: Treffer schließt ohne Verbrauch
    // (UseItemCheck im Raumskript entscheidet); 82 mit Inhalt wird geleert
    // (ItemUse ok=1, Menge 1 -> 0, ID bleibt, kein Typ 8); 82 leer in Raum 908
    // -> 195; Item 4 legt sein Low-Byte in itm[224]. Ohne Treffer: prm0 59 ->
    // 175 (Item 85) / 174, prm0 85 -> 176 (Item 59) / 174, sonst 161.
    section("D07 Use_05",[&]{
        {auto inv=clean();inv.slots()[2]=Inventory::pack(84,1);const auto before=inv.items;unsigned c=0;const std::array<unsigned,4> p{84,0,0,0};const auto u=inv.use_item(2,&p,c,0,5,0);
            check(u.closing&&!u.message&&u.useon==2&&inv.items==before&&(c&0x400),"key on its floor must close without consuming");}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(82,1);inv.slots()[0]=3;inv.slots()[3]=Inventory::pack(5,10);unsigned c=0;const std::array<unsigned,4> p{0,0,82,0};const auto u=inv.use_item(2,&p,c,0,5,0);
            check(u.closing&&inv.slots()[2]==Inventory::pack(82,0)&&inv.slots()[0]==3,"full extinguisher is not emptied in place (sb_id 82 keeps pip[0])");}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(82,0);const auto before=inv.items;unsigned c=0;const std::array<unsigned,4> p{82,0,0,0};
            auto u=inv.use_item(2,&p,c,9,8,0);check(u.message==195&&!u.closing&&inv.items==before,"empty extinguisher in room 908 lacks 195");
            u=inv.use_item(2,&p,c,0,5,0);check(u.closing&&!u.message&&inv.items==before,"empty extinguisher elsewhere must close");}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(4,0x1234&0xffff);inv.items[224]=0xabcd0000u;unsigned c=0;const std::array<unsigned,4> p{4,0,0,0};const auto u=inv.use_item(2,&p,c,0,5,0);
            check(u.closing&&inv.items[224]==0x34u&&Inventory::id(inv.slots()[2])==4,"item 4 must store its low byte in itm[224]");}
        auto miss=[&](unsigned item,unsigned prm0,unsigned expected){auto inv=clean();inv.slots()[2]=Inventory::pack(item,1);unsigned c=0;const std::array<unsigned,4> p{prm0,0,0,0};const auto u=inv.use_item(2,&p,c,0,5,0);
            check(u.message==expected&&!u.closing,"Use_05 unmatched trigger message item="+std::to_string(item)+" prm0="+std::to_string(prm0));};
        miss(85,59,175);miss(84,59,174);miss(59,85,176);miss(84,85,174);miss(84,90,161);miss(5,90,161);});
    // Meldungssperre st_flg 0x200: keine neue Meldung, Wirkung bleibt.
    section("D07 Meldungssperre",[&]{
        auto inv=clean();inv.slots()[2]=Inventory::pack(19,1);unsigned c=0;const auto u=inv.use_item(2,nullptr,c,0,0,0,true);check(!u.message&&u.useon==2,"st_flg 0x200 does not suppress bhSetMessage");});
    // D01 Kombination: je Selektor ein von Hand aus sub1.c abgeleiteter Fall.
    // first = listcsr_0 (Cursor, moto), second = listcsr_1 (Quelle, aite).
    section("D01 Selektor 0 Combi_00 volles Magazin -> 156",[&]{
        auto inv=clean();inv.slots()[2]=Inventory::pack(12,30);inv.slots()[3]=Inventory::pack(5,18);const auto before=inv.items;unsigned m=0,s=0;
        check(inv.combination_rule(2,3)==21,"rule lookup 12 -> 5 differs");check(!inv.combine(2,3,&m,&s)&&m==156&&inv.items==before,"full magazine must answer 156 without change");});
    section("D01 Selektor 0 Combi_00 Teil 19 + ausgerüstete Bogenwaffe 9 -> 10",[&]{
        auto inv=clean();inv.slots()[2]=Inventory::pack(19,1);inv.slots()[3]=Inventory::pack(9,5);inv.slots()[0]=3;unsigned m=0,s=0;
        check(inv.combine(2,3,&m,&s),"19 + 9 refused");check(inv.slots()[2]==Inventory::pack(10,5)&&!inv.slots()[3]&&inv.slots()[0]==2&&inv.weapon()==5,"Combi_00 19 path / WeaponSet (ok 129) / pip[0]-- / ItemSort differs"+dump(inv)+" weapon="+std::to_string(inv.weapon()));
        const auto change=inv.take_weapon_change();check(change.changed&&change.wpnr_no==5,"Combi_00 ok 0x80 does not run WeaponSet");});
    section("D01 Selektor 1 Combi_00 umgekehrt",[&]{
        {auto inv=clean();inv.slots()[2]=Inventory::pack(5,7);inv.slots()[3]=Inventory::pack(12,30);unsigned m=0,s=0;check(inv.combination_rule(2,3)==59&&inv.combine(2,3,&m,&s),"5 <- 12 refused");
            check(inv.slots()[2]==Inventory::pack(5,18)&&inv.slots()[3]==Inventory::pack(12,19),"reverse reload remainder differs");}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(12,10);inv.slots()[3]=Inventory::pack(95,1);inv.slots()[4]=Inventory::pack(5,7);inv.slots()[0]=4;unsigned m=0,s=0;check(inv.combine(4,2,&m,&s),"5 <- 12 (equipped) refused");
            check(inv.slots()[2]==Inventory::pack(95,1)&&inv.slots()[3]==Inventory::pack(5,17)&&inv.slots()[0]==3&&inv.weapon()==3,"consumed reserve before the equipped gun must lower pip[0] (subcsr 3)");}});
    section("D01 Selektor 2 Combi_01 Stapeln",[&]{
        {auto inv=clean();inv.slots()[2]=Inventory::pack(12,20);inv.slots()[3]=Inventory::pack(12,25);unsigned m=0,s=0;check(inv.combine(2,3,&m,&s),"12 + 12 refused");check(inv.slots()[2]==Inventory::pack(12,45)&&!inv.slots()[3],"ammo stacking differs");}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(12,990);inv.slots()[3]=Inventory::pack(12,15);unsigned m=0,s=0;check(inv.combine(2,3,&m,&s),"12 + 12 overflow refused");check(inv.slots()[2]==Inventory::pack(12,6)&&inv.slots()[3]==Inventory::pack(12,999),"Combi_01 999 limit/remainder differs");}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(15,6);inv.slots()[3]=Inventory::pack(6,4);unsigned m=0,s=0;check(inv.combination_rule(2,3)==25&&inv.combine(2,3,&m,&s),"15 -> launcher refused");check(inv.slots()[2]==Inventory::pack(6,10)&&!inv.slots()[3],"Combi_01 launcher (bltmax 999) differs");}});
    section("D01 Selektor 3 Combi_02 Munitionswechsel",[&]{
        {auto inv=clean();inv.slots()[2]=Inventory::pack(16,10);inv.slots()[3]=Inventory::pack(6,4);unsigned m=0,s=0;check(inv.combination_rule(2,3)==28&&inv.combine(2,3,&m,&s),"16 -> launcher refused");
            check(inv.slots()[2]==Inventory::pack(15,4)&&inv.slots()[3]==Inventory::pack(6|0x1000,10),"Combi_02 swap (old rounds back as 15, launcher 0x1000) differs");}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(16,10);inv.slots()[3]=Inventory::pack(6,4);inv.slots()[0]=3;unsigned m=0,s=0;check(inv.combine(2,3,&m,&s),"16 -> equipped launcher refused");
            check(inv.slots()[3]==Inventory::pack(6|0x1000,10)&&inv.slots()[0]==3&&inv.weapon()==15,"Combi_02 ok 128 must re-run WeaponSet on the launcher"+dump(inv)+" weapon="+std::to_string(inv.weapon()));
            const auto change=inv.take_weapon_change();check(change.changed&&change.wpnr_no==15,"Combi_02 WeaponSet not reported");}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(16,10);inv.slots()[3]=Inventory::pack(6,0);unsigned m=0,s=0;check(inv.combine(2,3,&m,&s),"16 -> empty launcher refused");
            check(inv.slots()[2]==Inventory::pack(6|0x1000,10)&&!inv.slots()[3],"Combi_02 with empty launcher must free the source slot");}});
    section("D01 Selektor 4 Combi_02 umgekehrt",[&]{
        auto inv=clean();inv.slots()[2]=Inventory::pack(6|0x1000,5);inv.slots()[3]=Inventory::pack(15,6);unsigned m=0,s=0;check(inv.combination_rule(2,3)==92&&inv.combine(2,3,&m,&s),"launcher(152) <- 15 refused");
        check(inv.slots()[2]==Inventory::pack(6,6)&&inv.slots()[3]==Inventory::pack(16,5),"reverse Combi_02 swap differs");});
    section("D01 Selektor 5 Combi_01 umgekehrt",[&]{
        auto inv=clean();inv.slots()[2]=Inventory::pack(6,4);inv.slots()[3]=Inventory::pack(15,6);unsigned m=0,s=0;check(inv.combination_rule(2,3)==89&&inv.combine(2,3,&m,&s),"launcher <- 15 refused");
        check(inv.slots()[2]==Inventory::pack(6,10)&&!inv.slots()[3],"reverse Combi_01 differs");});
    section("D01 Selektor 6 Combi_03 Kräuter",[&]{
        const unsigned recipes[][3]={{21,21,24},{21,22,25},{21,23,26},{21,24,28},{21,26,27},{22,26,29},{23,24,27},{23,25,29}};
        for(const auto& r:recipes)for(unsigned reverse=0;reverse<2;++reverse){auto inv=clean();inv.slots()[2]=Inventory::pack(r[reverse?1:0],1);inv.slots()[3]=Inventory::pack(r[reverse?0:1],1);unsigned m=0,s=0;
            check(inv.combine(2,3,&m,&s)&&inv.occupied()==1&&inv.slots()[2]==Inventory::pack(r[2],1)&&s==2,"herb recipe/result cursor differs: "+std::to_string(r[0])+"+"+std::to_string(r[1]));}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(22,1);inv.slots()[3]=Inventory::pack(23,1);const auto before=inv.items;unsigned m=0,s=0;check(inv.combination_rule(2,3)<0&&!inv.combine(2,3,&m,&s)&&inv.items==before,"invalid herb pair combined");}});
    section("D01 Selektor 7/8 Combi_04 Pulver + Pfeile",[&]{
        for(const auto counts:{std::pair<unsigned,unsigned>{4,10},{10,4},{4,4}})for(unsigned reverse=0;reverse<2;++reverse){auto inv=clean();inv.slots()[2]=Inventory::pack(35,counts.first);inv.slots()[3]=Inventory::pack(18,counts.second);unsigned result=0;
            check(inv.combine(reverse?3:2,reverse?2:3,nullptr,&result),"partial bowgun powder conversion refused");
            check(Inventory::id(inv.slots()[result])==36&&Inventory::quantity(inv.slots()[result])==std::min(counts.first,counts.second),"conversion loses explosive arrow count/result cursor");
            check(inv.contains(18)==(counts.second>counts.first)&&inv.contains(35)==(counts.first>counts.second),"conversion loses leftover arrows/powder");
            unsigned rest=0;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==18||Inventory::id(inv.slots()[i])==35)rest+=Inventory::quantity(inv.slots()[i]);
            check(rest==std::max(counts.first,counts.second)-std::min(counts.first,counts.second),"conversion leftover count differs");}});
    section("D01 Selektor 9 Combi_03 ohne Frage",[&]{
        auto inv=clean();inv.slots()[2]=Inventory::pack(144,1);inv.slots()[3]=Inventory::pack(56,1);unsigned m=0,s=0;check(inv.combination_rule(2,3)==391&&inv.combine(2,3,&m,&s)&&!m,"144 + 56 refused or asked");
        check(inv.slots()[2]==Inventory::pack(81,getbulletmax[81][0])&&!inv.slots()[3],"Combi_03 product differs");});
    section("D01 Selektor 10/11 Combi_05 Libelle",[&]{
        const unsigned bodies[]={79,122,123,124},products[]={122,123,124,66};
        for(unsigned step=0;step<4;++step)for(unsigned reverse=0;reverse<2;++reverse){auto inv=clean();inv.slots()[2]=Inventory::pack(reverse?bodies[step]:120,1);inv.slots()[3]=Inventory::pack(reverse?120:bodies[step],1);unsigned m=0,s=0;
            check(inv.combine(2,3,&m,&s)&&inv.occupied()==1&&Inventory::id(inv.slots()[2])==products[step],"gold dragonfly wing assembly differs, body "+std::to_string(bodies[step]));}
        // dbwtbl[chgid][menge-1]: drei Flügel auf einmal am Körper 79 -> 124.
        {auto inv=clean();inv.slots()[2]=Inventory::pack(120,3);inv.slots()[3]=Inventory::pack(79,1);unsigned m=0,s=0;check(inv.combine(2,3,&m,&s)&&inv.slots()[2]==Inventory::pack(124,getbulletmax[124][0]),"Combi_05 dbwtbl by wing count differs");}});
    // D02 ItemSearch/EraseItem: EraseItem senkt pip[0] nur, wenn der Slot vor
    // dem ausgerüsteten liegt, und sortiert nur dann; sonst endet es sofort.
    section("D02 EraseItem über das Handbuch",[&]{
        for(unsigned owner=0;owner<4;++owner){Inventory inv;inv.player=owner;std::fill(inv.slots(),inv.slots()+12,0);inv.slots()[1]=Inventory::pack(50,1);inv.slots()[2]=Inventory::pack(145,1);inv.slots()[3]=Inventory::pack(1,1);inv.slots()[0]=3;
            inv.erase_read_manual(true);check(!inv.contains(145)&&inv.slots()[0]==2&&Inventory::id(inv.slots()[2])==1,"manual removal failed original EraseItem equipment decrement/sort");
            unsigned flags=0x4010;inv.pickup(12,flags,0,7);check(inv.slots()[0]==2&&inv.weapon()==20&&Inventory::id(inv.slots()[3])==12,"ammo pickup equipped the old knife slot");}
        {Inventory inv;inv.slots()[1]=Inventory::pack(145,1);inv.slots()[2]=Inventory::pack(1,1);inv.slots()[0]=2;inv.erase_read_manual(true);check(inv.slots()[1]==0&&Inventory::id(inv.slots()[2])==1&&inv.slots()[0]==1,"special slot must retain the original EraseItem/ItemSort semantics");}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(5,1);inv.slots()[3]=Inventory::pack(145,1);inv.slots()[4]=Inventory::pack(95,1);inv.slots()[0]=2;inv.erase_read_manual(true);
            check(inv.slots()[0]==2&&inv.slots()[2]==Inventory::pack(5,1)&&!inv.slots()[3]&&inv.slots()[4]==Inventory::pack(95,1),"EraseItem behind pip[0] must return before ItemSort"+dump(inv));}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(145,1);inv.slots()[3]=Inventory::pack(95,1);inv.erase_read_manual(true);
            check(!inv.slots()[2]&&inv.slots()[3]==Inventory::pack(95,1)&&!inv.slots()[0],"EraseItem without equipment must not sort"+dump(inv));}});
    // D01 B: WeaponSet vergleicht mit plp->wpnr_no von vor der Mutation. Die
    // Kombination ruft WeaponSet erst nach dem Tausch; die ausgerüstete Waffe
    // bleibt dadurch ausgerüstet, statt abgelegt zu werden.
    section("D01 WeaponSet nach Kombination behält die Ausrüstung",[&]{
        {auto inv=clean();inv.slots()[2]=Inventory::pack(19,1);inv.slots()[3]=Inventory::pack(9,5);inv.slots()[0]=3;unsigned m=0,s=0;
         check(inv.combine(2,3,&m,&s),"19 + 9 refused");
         check(inv.slots()[2]==Inventory::pack(10,5)&&inv.slots()[0]==2&&inv.weapon()==5,"Combi_00 ok 129 must keep the upgraded bowgun equipped"+dump(inv));
         const auto change=inv.take_weapon_change();check(change.changed&&change.wpnr_no==5,"WeaponSet result not reported to the weapon owner");}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(16,10);inv.slots()[3]=Inventory::pack(6,4);inv.slots()[0]=3;unsigned m=0,s=0;
         check(inv.combine(2,3,&m,&s),"16 -> equipped launcher refused");
         check(inv.slots()[3]==Inventory::pack(6|0x1000,10)&&inv.slots()[0]==3&&inv.weapon()==15,"Combi_02 ok 128 must re-equip the launcher with its new round type"+dump(inv));
         const auto change=inv.take_weapon_change();check(change.changed&&change.wpnr_no==15,"Combi_02 WeaponSet not reported");}
        // Gleiche Waffe erneut am selben Slot: WeaponSet rüstet ab.
        {auto inv=clean();inv.slots()[2]=Inventory::pack(5,18);check(inv.equip(2)&&inv.slots()[0]==2&&inv.weapon()==3,"equip failed");
         check(inv.equip(2)&&!inv.slots()[0]&&!inv.weapon(),"equipping the same slot twice must unequip");}});
    // D02 C: EraseItem endet bei wp >= pip + pip[0] ohne pip[0]-Änderung und
    // ohne ItemSort; nur davor rückt pip[0] nach und es wird sortiert.
    section("D02 EraseItem Abbruch und Sortierreihenfolge",[&]{
        {auto inv=clean();inv.slots()[2]=Inventory::pack(5,1);inv.slots()[3]=Inventory::pack(145,1);inv.slots()[4]=Inventory::pack(95,1);inv.slots()[0]=2;inv.erase_read_manual(true);
         check(inv.slots()[0]==2&&inv.slots()[2]==Inventory::pack(5,1)&&!inv.slots()[3]&&inv.slots()[4]==Inventory::pack(95,1),"EraseItem behind pip[0] must return before ItemSort"+dump(inv));}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(145,1);inv.slots()[3]=Inventory::pack(95,1);inv.erase_read_manual(true);
         check(!inv.slots()[2]&&inv.slots()[3]==Inventory::pack(95,1)&&!inv.slots()[0],"EraseItem without equipment must not sort"+dump(inv));}
        {auto inv=clean();inv.slots()[2]=Inventory::pack(145,1);inv.slots()[3]=Inventory::pack(95,1);inv.slots()[4]=Inventory::pack(5,1);inv.slots()[0]=4;inv.erase_read_manual(true);
         check(inv.slots()[0]==3&&inv.slots()[2]==Inventory::pack(95,1)&&inv.slots()[3]==Inventory::pack(5,1)&&!inv.slots()[4],"EraseItem before pip[0] must lower it and sort"+dump(inv));}});
    section("D02 ItemSearch",[&]{
        auto inv=clean();inv.slots()[1]=Inventory::pack(55,1);inv.slots()[4]=Inventory::pack(21,1)|0x08000000u;inv.slots()[6]=Inventory::pack(21,1);
        check(inv.search(55)==1&&inv.search(21)==4&&inv.search(22)==0,"ItemSearch range pip[1..11] / first hit / low byte differs");});
    // Host-Erwerb (Debugkatalog) nutzt ItemSort: breite Waffe vor schmalen.
    section("D02 ItemSort beim Host-Erwerb",[&]{
        auto inv=clean();check(inv.acquire(95,1)&&inv.acquire(3,0),"host acquire path broken");
        check(Inventory::id(inv.slots()[2])==3&&Inventory::id(inv.slots()[3])==95&&inv.wide_boundary()==3,"ItemSort must place the wide weapon first (sakai 3)");});
    if(!failures.empty()){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<failures.size()<<" inventory engine section(s) failed.\n";return 1;}
    std::cout<<"Item display poses, all 156 inspection cases with their 11 transformations, file tables (167 pages) and name selection passed; ItemUse classes, all twelve combination selectors, ItemSearch/EraseItem and ItemSort passed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

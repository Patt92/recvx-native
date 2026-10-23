#include "inventory.h"
#include "inspection.h"
#include "status_pulse.h"
#include <algorithm>
#include <stdexcept>
#include <tuple>
namespace recvx {
// Inventarmechanik, eigenständig nativ auf diesem einzigen Bestandsbesitzer.
// Semantikreferenz: sub1.c (GetItem, ItemSort, ItemSearch, EraseItem,
// bhSearchBullet, Combi_99, ItemUse, Use_00/01/02/04/05, Combi_00..05,
// ItemBoxChange, WeaponSet) und weapon.c (bhCountBullet). Feld-/Flagvertrag:
// work/native_port_inventory_contract.md. Kein Funktionskörper übernommen,
// kein Originalkern, keine zweite Work.
namespace {
constexpr unsigned kWide=0x100,kStack=0x200,kEndless=0x800;
unsigned type_of(unsigned item){return item<156?itemdata[item].type:0;}
unsigned flags_of(std::uint32_t entry){return (entry>>16)&0xff00;}
// Kapazitätseinheiten der Tasche nach GetItem/ItemBoxChange: 8 bzw. 10, breite
// Items zählen doppelt.
unsigned free_units(const std::uint32_t* pip,unsigned max){
    int units=int(max);
    for(unsigned i=0;i<max;++i){const auto item=Inventory::id(pip[i+2]);if(type_of(item)&kWide)units-=2;else if(item)--units;}
    return unsigned(std::max(0,units));
}
// sub1.c standard[ply_id]: Grundwaffen, die im Sonderslot pip[1] leben.
constexpr int kStandard[3][2]={{55,50},{55,-1},{138,-1}};
bool standard_weapon(unsigned player,unsigned item){
    if(player>=3||!item)return false;
    return int(item)==kStandard[player][0]||int(item)==kStandard[player][1];
}
}
unsigned Inventory::wide_boundary()const{
    // ItemSort: sakai = 2 + Anzahl führender breiter Einträge.
    unsigned i=0;while(i<10&&(type_of(id(slots()[i+2]))&kWide))++i;
    return i+2;
}
void Inventory::sort(){
    // sub1.c ItemSort (sub1.c:2266): läuft in SpriteOnOff während des
    // Statusbildschirms in JEDEM Frame (sub1.c:8296 `if (st->subcsr != 3)`),
    // also auch nach jedem Entfernen. Der Ausrüstungsindex pip[0] wird vom
    // Original nicht angepasst; compact() lässt ihn wie ItemSort unberührt.
    compact();
}
void Inventory::compact(){
    // ItemSort (sub1.c:2266-2343): zuerst alle breiten, dann alle übrigen
    // belegten Einträge in ursprünglicher Reihenfolge; leere fallen ans Ende.
    // pip[0] und die Menücursor werden vom Original nicht nachgeführt (die
    // Aufrufer korrigieren pip[0] selbst). Die flgchk-0x10/0x20-Markierung
    // (sub1.c:2290/2307/2312) entfällt: keine Quelle setzt flgchk 0x10/0x20,
    // gesetzt werden nur 0x2/0x8 (sub1.c:4378/7432).
    std::array<std::uint32_t,10> sorted{};unsigned n=0;
    for(unsigned pass=0;pass<2;++pass)for(unsigned i=0;i<10;++i){
        const auto entry=slots()[i+2];const auto item=id(entry);
        const bool wide=type_of(item)&kWide;
        if(pass==0?wide:(!wide&&item))sorted[n++]=entry;
    }
    for(unsigned i=0;i<10;++i)slots()[i+2]=sorted[i];
}
unsigned Inventory::search(unsigned item)const{
    // ItemSearch: pip[1..11], Vergleich nur auf die Item-ID (Low-Byte).
    for(unsigned i=1;i<12;++i)if(id(slots()[i])==item)return i;
    return 0;
}
void Inventory::erase(unsigned slot){
    // EraseItem: Eintrag löschen; liegt er auf oder hinter dem ausgerüsteten
    // Slot (Zeigervergleich wp >= pip + pip[0], bei pip[0] == 0 immer), endet
    // die Routine dort — ohne pip[0]-Änderung und ohne ItemSort. Nur davor
    // rückt pip[0] um eins nach und sortiert.
    if(!slot||slot>=12)return;
    slots()[slot]=0;
    if(slot>=slots()[0])return;
    --slots()[0];
    compact();
}
unsigned Inventory::weapon_number(std::uint32_t entry,unsigned previous){
    const auto item=id(entry);const auto flags=(entry>>16)&0x7000;
    switch(item){
    case 0:return 0;case 1:return 20;case 2:return 12;case 3:return 13;case 4:return 11;case 5:return 3;
    case 6:return flags==0x1000?15:flags==0x2000?16:flags==0x4000?17:14;
    case 7:return (flags&0x2000)?19:10;
    case 8:return 2;case 9:return 4;case 10:return 5;case 11:return 18;
    case 32:return 6;case 33:return 7;case 34:return 8;case 55:return 1;
    case 131:return 3;case 142:return 9;
    default:return previous; // keine eigene Nummer: plp->wpnr_no bleibt
    }
}
void Inventory::weapon_set(unsigned slot,unsigned lt_c,unsigned previous){
    // WeaponSet: Waffennummer aus der ID (Sorte für 6/7); ergibt sie dieselbe
    // Nummer, die der Spieler schon trägt, rüstet derselbe Slot ab. `previous`
    // ist plp->wpnr_no vor dieser Mutation — der Aufrufer nimmt es mit weapon()
    // ab, bevor er Slots verändert (Kombinationen rufen WeaponSet erst nach dem
    // Tausch, plp->wpnr_no steht dann noch auf der alten Waffe). IDs ohne eigene
    // Nummer lassen sie unverändert. Die Inventory meldet den Wechsel über
    // take_weapon_change(); der Kampfbesitzer setzt plp->wpnr_no/mode3.
    const auto entry=slots()[slot];
    unsigned next=weapon_number(entry,previous);bool special=id(entry)==131;
    if(next==previous&&slots()[0]==lt_c){next=0;slots()[0]=0;special=false;}
    else slots()[0]=lt_c;
    wpnr_no=next; // sub1.c:4471-4572: plp->wpnr_no im switch, beim Abruesten 0
    weapon_change_={true,next,special};request_weapon_reload();
}
PickupResult Inventory::pickup(unsigned item,unsigned& control,unsigned stage,unsigned room,unsigned mode,int answer,bool busy){
    // GetItem. `control` ist sys->cb_flg. Modus 0 entscheidet Frage/Bestätigung,
    // Modus 1/4 verarbeiten die Antwort 100 (Ja) / 101 (Nein). Die Einfügung
    // läuft, sobald cb_flg 0x800 gesetzt und noch nicht vollzogen ist.
    if(!item||item>=156||player>=4||game_mode>=4||mode>5||answer < -1||answer>101)throw std::runtime_error("invalid original pickup state");
    if(item==130)item=35;
    PickupResult result;result.mode=mode;
    const unsigned max=side_pack?10:8;
    // plp->wpnr_no vor der Einfügung (GetItem ruft WeaponSet erst danach).
    const unsigned wpnr_before=weapon();
    auto set_message=[&](unsigned index){result.message=index;};
    // testmode 0: Kapazität der Tasche (breite Items doppelt) und Stapelplatz.
    // Ein Stapel unter 999, der die Aufnahme vollständig fasst, macht die Tasche
    // für diese Entscheidung "frei" (num2 = max).
    auto decide=[&](){
        unsigned room_left=max,rest=0;
        for(unsigned i=0;i<max;++i){
            const auto entry=slots()[i+2];const auto have=id(entry);
            if(type_of(have)&kWide)room_left-=2;else if(have)--room_left;
            if(have==item&&(type_of(item)&kStack)&&quantity(entry)<999){
                // GetItem bullet1 ist unsigned short (Addition modulo 65536).
                const unsigned amount=std::uint16_t(quantity(entry)+(rest?rest:getbulletmax[item][game_mode]));
                rest=amount>999?amount-999:0;
                if(!rest){room_left=max;break;}
            }
        }
        unsigned next;
        if(control&0x1000000)next=2;
        else if(type_of(item)&kWide)next=room_left>=2?((control&0x4000)?2u:1u):3u;
        else if(room_left>0)next=(control&0x4000)?2u:1u;
        else if((item>19&&item<22)||item==23)next=4;
        else next=3;
        if(next==2)control|=0x800;
        control&=~0x4000u;
        return next;
    };
    if(mode==0){
        const unsigned next=decide();result.mode=next;
        if(next==1)set_message(157);else if(next==3)set_message(154);else if(next==4)set_message(153);
        if(next!=2)return result;
    }else if(mode==1){
        // testmode 1: die Antwort löscht cb_flg 0x1000 und st_flg 0x200. Ja
        // setzt 0x800 und geht nach testmode 2, Nein direkt nach 3. Das
        // Original prüft die Kapazität hier nicht erneut.
        if(busy||answer<0)return result; // Einfahrt läuft oder keine Antwort
        control&=~0x1000u;
        if(answer!=100){result.mode=3;return result;}
        control|=0x800;result.mode=2;
    }else if(mode==2){
        // testmode 2: eigener Durchlauf, meldet 158 und geht nach 3. Die
        // Einfügung lief bereits im Durchlauf davor (der Block unter dem
        // switch, sobald cb_flg 0x800 gesetzt war).
        if(busy)return result;
        set_message(158);result.mode=3;return result;
    }else if(mode==4){
        if(busy||answer<0)return result;
        control&=~0x1000u;result.mode=3;
        if(answer==100){
            control|=0x800|0x400;
            // Sofortgebrauch: Use_00 auf denselben Bestand.
            const auto use=use_00(item,StatusPulse::condition(hp,(player_stflg()&0x280000u)!=0),false);
            result.used=true;result.pulse=use.pulse;if(use.message)result.message=use.message; // sub1.c:3939 -> Use_00 :6408 bhSetMessage(1,198) (st_flg 0x200 eben geloescht)
        }
        return result;
    }else{
        return result; // testmode 2/3/5 tragen hier nichts bei
    }
    if(!(control&0x1000000)&&!(control&0x400)){
        // Einfügung (GetItem nach dem switch). roomno = rom_no + stg_no*100.
        // `count` ist num2: Fortschritt in Kapazitätseinheiten; ein vorhandenes
        // breites Item zählt doppelt. Ein Rest über 999 läuft weiter.
        const unsigned roomno=room+stage*100;
        unsigned rest=0;bool done=false;
        for(unsigned slot=2,count=1;slot<12&&!done;++slot,++count){
            const auto entry=slots()[slot];const auto have=id(entry);unsigned amount=quantity(entry);
            if(have==item){
                if((type_of(item)&kStack)&&amount<999){
                    if(!(flags_of(entry)&kEndless)){
                        if(rest)amount+=rest;
                        else if(item==31&&roomno==1)amount+=1;
                        else amount+=getbulletmax[item][game_mode];
                        amount=std::uint16_t(amount); // bullet1: unsigned short
                        rest=amount>999?amount-999:0;if(rest)amount=999;
                        slots()[slot]=pack(have,amount);
                        if(!rest){result.granted=true;done=true;}
                    }else{
                        slots()[slot]=pack(have|kEndless,amount);result.granted=true;done=true;
                    }
                }
            }else if(count==max||!have){
                if(type_of(item)&kWide){
                    // (sb_id << 16) | *gb: *gb ist 32 Bit und wird unverändert
                    // verodert (getbulletmax[33][3] trägt Flagbits).
                    slots()[slot]=(std::uint32_t(item)<<16)|getbulletmax[item][game_mode];
                    if((item==33||item==34)&&player==0)slots()[slot]&=0xffff0000u;
                    // Ein neues breites Item vor einer ausgerüsteten schmalen
                    // Waffe verschiebt deren Index nach ItemSort um eins.
                    const auto equipped=slots()[0];
                    if(equipped>1&&equipped<12&&!(type_of(id(slots()[equipped]))&kWide))++slots()[0];
                }else if(rest){
                    slots()[slot]=pack(item,rest);
                }else if(item==4&&(roomno==926||roomno==706)){
                    slots()[slot]=pack(item,items[224]&0xff);
                }else if(item==55){
                    slots()[1]=(std::uint32_t(item)<<16)|getbulletmax[item][game_mode];
                }else{
                    slots()[slot]=(std::uint32_t(item)<<16)|getbulletmax[item][game_mode];
                    if(item==31&&roomno==1)slots()[slot]=(slots()[slot]&0xffff0000u)|1u;
                    if(item==37&&roomno==551)slots()[slot]|=0x08000000u;
                    if(item==11)slots()[slot]|=0x08000000u;
                    if(control&0x8000){if(item==9)slots()[slot]-=3;else if(item==3)slots()[slot]-=2;}
                }
                if(control&0x8000){if(item==9)weapon_set(slot,slot,wpnr_before);control&=~0x8000u;}
                result.granted=true;done=true;
            }
            if(type_of(have)&kWide)++count;
        }
        // Die Meldung 158 kommt erst im folgenden Durchlauf (testmode 2);
        // dieser endet mit der Einfügung. GetItem selbst sortiert nicht: ein
        // neues breites Item kommt erst durch das ItemSort desselben
        // Statusframes nach vorn (SpriteOnOff, sub1.c:8296).
    }else if(control&0x1000000){
        // Sonderslot: pip[1] erhält das Item; lag die Ausrüstung dort, wird sie
        // gelöst (plp->mode3/wpnr_no = 0, mn_mode0 = 3 beim Kampfbesitzer).
        slots()[1]=(std::uint32_t(item)<<16)|getbulletmax[item][game_mode];
        if(slots()[0]==1){slots()[0]=0;wpnr_no=0;weapon_change_={true,0,false};request_weapon_reload();} /* sub1.c:4153 */
        control&=~0x1000000u;result.granted=true;
    }
    return result;
}
Inventory::HerbUse Inventory::use_00(unsigned item,unsigned condition,bool message_active){
    // Use_00: curedata[sb_id-20] = Heilstufe (Low-Nibble) | Giftheilung (0x10).
    // gm_mode 2 verdoppelt Heilung und Obergrenze (320 statt 160).
    HerbUse out;out.condition=condition;
    if(item<20||item>=30)return out;
    const unsigned cure=curedata[item-20];const unsigned heal=cure&0xf,poison=cure&0xf0;
    const bool hard=game_mode==2;const int cap=hard?320:160;
    if(item==20)++spray_count;
    switch(heal){
    case 1:hp+=hard?100:50;break;
    case 2:hp+=hard?200:100;break;
    case 3:case 4:hp=cap;break;
    default:break;
    }
    if(hp>cap)hp=cap;
    bool poison2=false;
    if(poison==0x10){poisoned=false;if(infected&&!heal)poison2=true;}
    // Zustand neu, solange keine Vergiftung (stflg 0x280000) mehr anliegt.
    if(!poisoned&&!infected)out.condition=hp>=120?0:hp>=60?1:hp>=30?2:3;
    if(poison2&&!message_active)out.message=198;
    if(heal){out.pulse=0x40;out.consumed=true;}
    else if(poison){if(!infected){out.pulse=0x80;out.consumed=true;}}
    else if(!message_active)out.message=161;
    // herbuse bleibt im Original bei poison2 ohne Heilung uninitialisiert;
    // laut Disassembly ist es 0 (AGENTS.md "Item-Box-Kiste und Use_00"):
    // das Kraut wird dann nicht verbraucht.
    return out;
}
ItemUseResult Inventory::use_item(unsigned slot,const std::array<unsigned,4>* parameters,unsigned& control,unsigned stage,unsigned room_number,unsigned condition,bool message_active){
    // ItemUse: Klasse type & 0x5F wählt Use_00/01/02/04/05. Verbrauch nur bei
    // ok und gepackter Menge > 0; Menge 0 eines Krauts löscht den Eintrag.
    if(player>=4||!slot||slot>=12||!id(slots()[slot])||id(slots()[slot])>=156||condition>4)throw std::runtime_error("invalid original ItemUse slot");
    ItemUseResult result;result.condition=condition;
    const auto entry=slots()[slot];const unsigned item=id(entry);unsigned bullet=entry&0xffff;
    control|=0x400; // sys->cb_flg 0x400: ItemUse läuft (sb_id = item)
    // cb_flg 0x200 (aktiver Bodentrigger) samt rom->flrp[flr_idx]: der Aufrufer
    // übergibt die Triggerparameter genau dann, wenn das Bit gesetzt ist.
    if(parameters)control|=0x200;else control&=~0x200u;
    const bool trigger=parameters!=nullptr;
    auto matches=[&](){return trigger&&((*parameters)[0]==item||(*parameters)[1]==item||(*parameters)[2]==item||(*parameters)[3]==item);};
    auto say=[&](unsigned index){if(!message_active)result.message=index;};
    unsigned ok=0;
    switch(itemdata[item].type&0x5f){
    case 0x8:{const auto herb=use_00(item,condition,message_active);ok=herb.consumed;result.message=herb.message;result.pulse=herb.pulse;result.condition=herb.condition;result.useon=2;break;}
    case 0x2: // Use_01: Munition an einem Bodentrigger
        if(trigger){
            if(matches())result.closing=true;
            else if((*parameters)[0]!=59&&(*parameters)[0]!=85)say((item==36||item==18)?197:160);
            else say(174);
        }else say((item==36||item==18)?197:160);
        result.useon=2;break;
    case 0x4:case 0x10:say(161);result.useon=2;break; // Use_02
    case 0x20:result.useon=2;break; // Use_04
    case 0x40:case 0x1: // Use_05: Schlüssel/Waffe am Bodentrigger
        if(trigger){
            if(matches()){
                if(item==82){
                    if(bullet&0xff){result.closing=true;ok=1;}
                    else if(stage==9&&room_number==8)result.message=195; // sub1.c:6551 bhSetMessage(1,195) ohne st_flg-0x200-Pruefung
                    else result.closing=true;
                }else{
                    if(item==4)items[224]=bullet&0xff;
                    result.closing=true;
                }
            }else if((*parameters)[0]==59)say(item==85?175:174);
            else if((*parameters)[0]==85)say(item==59?176:174);
            else say(161);
        }else say(161);
        result.useon=2;break;
    default:break;
    }
    // ItemUse: bei ok und Menge > 0 wird die lokale Menge verringert; nur wenn
    // sie dadurch 0 wird, schreibt das Original den Eintrag als
    // (st->itemid << 16) | 0 zurück — Kräuter (type & 8) mit itemid 0, also
    // gelöscht, alles andere als nackte ID ohne Flags. Eine Menge > 1 bleibt
    // im Bestand unverändert (der Dekrement wirkt nur auf die Kopie).
    if(bullet&&ok){
        --bullet;
        if(!bullet){
            const unsigned kept=(itemdata[item].type&0x8)?0u:item;
            slots()[slot]=(kept<<16);
            if(item!=82&&slot<slots()[0]&&slots()[0]!=0)--slots()[0];
        }
    }
    // StatusMain löscht cb_flg 0x400 im nächsten Durchlauf; nur StatusCancel
    // lässt das benutzte Item für die Event-VM stehen.
    if(!result.closing)control&=~0x400u;
    // ItemUse sortiert nicht; die Lücke eines verbrauchten Eintrags schließt
    // das ItemSort desselben Statusframes (SpriteOnOff, sub1.c:8296).
    return result;
}
bool Inventory::count_bullet(){
    // bhCountBullet: signierter 16-Bit-Zähler; unendlich (0x800) oder at_flg
    // 0x40000000 verbrauchen nichts. Bei Erreichen von 0 setzt das Original
    // gm_flg 0x40000 (Magazin leer).
    const auto slot=slots()[0];if(slot>=12)return false;
    auto& entry=slots()[slot];
    const std::int16_t count=std::int16_t(entry&0xffff);
    if((flags_of(entry)&kEndless)||unlimited_attack)return true;
    if(count<1)return false;
    const std::int16_t next=std::int16_t(count-1);
    if(next<1)chamber_empty=true;
    entry=(entry&0xffff0000u)|std::uint16_t(next);
    return true;
}
bool Inventory::reload(){
    // bhSearchBullet (Grenze gm_flg 0x8000000 laut Disassembly, siehe Vertrag).
    const unsigned max=side_pack?10:8;
    const auto equipped=slots()[0];if(equipped>=12)return false;
    unsigned wpn=id(slots()[equipped]);
    if(wpn==7&&(flags_of(slots()[equipped])&0x2000))wpn=155;
    for(unsigned i=0;i<max;++i){
        const unsigned ammo=id(slots()[i+2]);
        if(!(type_of(ammo)&0x2)||itemdata[ammo].cmb<0)continue;
        unsigned pos=unsigned(itemdata[ammo].cmb);unsigned count=unsigned(combidata[pos]);++pos;
        for(;count;--count,pos+=3){
            if(unsigned(combidata[pos])!=wpn||!(combidata[pos+2]&0xf000))continue;
            // Combi_99: Munition (moto=i+2) in die ausgerüstete Waffe (aite=pip[0]).
            auto& moto=slots()[i+2];auto& aite=slots()[equipped];
            unsigned id1=id(moto),id2=id(aite);
            const unsigned bt=flags_of(moto),bullettype=flags_of(aite);
            unsigned b1=quantity(moto),b2=quantity(aite);unsigned ok;
            // Combi_99: bullet1/2 sind unsigned short, *gb 32 Bit.
            if(bt&kEndless){b2=std::uint16_t(getbulletmax[id2][game_mode]);ok=2;}
            else{
                b2=std::uint16_t(b2+b1);const unsigned cap=getbulletmax[id2][game_mode];
                if(cap<b2){ok=2;b1=std::uint16_t(b2-cap);b2=std::uint16_t(cap);}else{id1=0;b1=0;ok=1;}
            }
            if(bullettype&0x2000)id2|=0x2000;
            moto=std::uint32_t(b1&0xffff)|((id1|(bt&kEndless))<<16);
            aite=std::uint32_t(b2&0xffff)|((id2|(bullettype&kEndless))<<16);
            if(ok==1&&slots()[0]>i+2)--slots()[0];
            compact();
            return true;
        }
    }
    return false;
}
Inventory::Inventory(){
    // AllItemInit, normal new game, no bonus unlocks.
    items[1]=0x370001;items[2]=0x910001;
    items[18]=0x80001;items[19]=0x50012;items[20]=0xc001e;items[21]=0x140001;
    items[32]=2;items[33]=0x8a0001;items[34]=0x220190;items[383]=1;
    // system.c:483-489: every player starts at the game-mode HP.
    const int initial=max_hp;for(auto& v:ply_hp)v=initial;
}
unsigned Inventory::switch_player(unsigned target,std::uint32_t* stflg){
    if(target>=ply_hp.size())throw std::runtime_error("bhPlayerChangeSet target outside the original player table");
    const unsigned previous=player;
    if(previous==target)return previous;
    // system.c:1729/1746/1747: the outgoing work keeps its persistent state,
    // keyed by the still-active ply_id.
    // system.c:1727-1729: plp->stflg &= 0x78280000; ply_stflg[ply_id] = plp->stflg.
    ply_stflg[previous]=(stflg?*stflg:(player_stflg()|0x40000000u))&0x78280000u;
    ply_hp[previous]=hp;
    partner_weapon[previous]=std::uint8_t(wpnr_no); // system.c:1747 ply_wno[ply_id] = plp->wpnr_no
    // sys->ply_id = sys->cng_pid; bhResetPlayer then restores the new player.
    player=target;
    hp=ply_hp[player];
    set_player_stflg(ply_stflg[player]);
    if(stflg)*stflg=ply_stflg[player]|0x40000000u; // player.c:993-994 bhResetPlayer
    wpnr_no=partner_weapon[player];request_weapon_reload(); // system.c:1787 plp->wpnr_no = ply_wno[ply_id] (danach bhReadWeaponData)
    return previous;
}
unsigned Inventory::occupied()const{unsigned n=0;for(unsigned i=2;i<12;++i)if(auto item=id(slots()[i]))n+=(itemdata[item].type&0x100)?2:1;return n;}
bool Inventory::contains(unsigned item)const{for(unsigned i=1;i<12;++i)if(id(slots()[i])==item)return true;return false;}

bool Inventory::equip(unsigned slot){
    // ItemCommand "Ausrüsten" -> WeaponSet(slot, slot): erneutes Ausrüsten
    // derselben Waffe am selben Slot rüstet ab.
    if(!slot||slot>=12||!id(slots()[slot])||!(itemdata[id(slots()[slot])].type&1))return false;
    weapon_set(slot,slot,weapon());return true;
}
bool Inventory::acquire(unsigned item,unsigned count,unsigned flags){
    if(!item||item>=156||count>65535||flags&~0xff00u)return false;
    // Original unloaded firearms and knives are valid pickups with zero rounds.
    if(!count){if(!(itemdata[item].type&1)||occupied()+((itemdata[item].type&0x100)?2:1)>capacity())return false;unsigned slot=2;while(slot<12&&id(slots()[slot]))++slot;if(slot==12)return false;slots()[slot]=pack(item|flags,0);compact();return true;}
    auto saved=items;
    // Stack ammo using the original per-item maxima, never partially accepting a pickup.
    if(itemdata[item].type&0x200)for(unsigned i=2;i<12&&count;++i)if((slots()[i]>>16)==(item|flags)){
        const auto max=item==42?300u:999u,used=quantity(slots()[i]);const auto take=std::min(count,max-std::min(max,used));slots()[i]+=take;count-=take;
    }
    while(count){if(occupied()+((itemdata[item].type&0x100)?2:1)>capacity()){items=saved;return false;}
        unsigned slot=2;while(slot<12&&id(slots()[slot]))++slot;if(slot==12){items=saved;return false;}
        unsigned take=count;if(itemdata[item].type&0x200)take=std::min(count,item==42?300u:999u);
        else take=std::min(count,std::max(1u,unsigned(itemdata[item].max)));
        slots()[slot]=pack(item|flags,take);count-=take;
    }
    compact();return true;
}
bool Inventory::debug_acquire(unsigned choice,unsigned count){
    if(choice>=256&&choice<280){items[383]|=1u<<(choice-256);return true;}
    if(!choice||choice>=156||original_file_model_mask(choice))return false;
    return acquire(choice,(itemdata[choice].type&1)&&!itemdata[choice].max?0:count);
}
unsigned Inventory::repair_owned_file_items(){
    const auto owner=player;unsigned removed=0;
    for(player=0;player<4;++player){bool changed=false;
        for(unsigned slot=1;slot<12;++slot){const auto item=id(slots()[slot]);
            // Entfernt wird über dieselbe Regel wie EraseItem, damit der
            // Ausrüstungsindex genauso nachrückt wie bei jeder originalen
            // Entnahme; nur die Auswahl der Einträge ist Host-Kompatibilität.
            if(original_file_model_mask(item)&items[383]){
                if(slots()[0]==slot)slots()[0]=0;
                erase(slot);++removed;changed=true;}}
        if(changed)compact();
    }
    player=owner;return removed;
}
void Inventory::erase_read_manual(bool automatic){
    // FileExit01 (fileview.c) mit statusflg 0x40000000: ItemSearch(145) ->
    // EraseItem -> ItemSort, dann ic_flg[15] 0x80000000 (Ereignisseite).
    // Der nicht-automatische Zweig ist der Host-Ausstieg aus dem Leser und
    // entfernt das Handbuch an seiner bisherigen Stelle im Gesamtbestand.
    if(automatic){if(const auto slot=search(145))erase(slot);return;}
    for(unsigned i=0;i<192;++i)if(items[i]==0x910001){items[i]=0;break;}
}
std::string Inventory::condition()const{return poisoned?"Poison":hp>=120?"Fine":hp>=60?"Caution":hp>=30?"Caution":"Danger";}
int Inventory::combination_rule(unsigned first,unsigned second)const{
    if(first<1||second<1||first>=12||second>=12||first==second)return -1;
    auto effective=[](std::uint32_t v){auto item=id(v);const auto flags=(v>>16)&0x7000;if(item==6)return flags==0x1000?152u:(flags==0x2000?153u:(flags==0x4000?154u:6u));if(item==7&&flags==0x2000)return 155u;return item;};
    const auto a=effective(slots()[first]),b=effective(slots()[second]);if(!a||!b||a>=156||b>=156||itemdata[b].cmb<0)return -1;
    unsigned pos=unsigned(itemdata[b].cmb),count=unsigned(combidata[pos++]);bool found=false;
    for(unsigned i=0;i<count&&pos+2<410;++i,pos+=3)if(unsigned(combidata[pos])==a){found=true;break;}
    if(!found)return -1;return int(pos);
}
namespace {
// Die sechs Kombinationsmechaniken (sub1.c Combi_00..05) auf gepackten
// Einträgen. `ps` ist die Regelposition (st->cmbitm, Index der passenden ID in
// combidata); combidata[ps+1] ist das Produkt, combidata[ps+2]&0xff die Klasse.
// Rückgabe ist das Original-ok: 1 moto verbraucht, 2/3 Rest bleibt, 4 voll
// (Meldung 156), Bit 0x80 verlangt WeaponSet auf dem Ziel, 128/129 wie Original.
struct ComboContext {unsigned game_mode;unsigned equipped;unsigned listcsr_0,listcsr_1;bool message_active;unsigned message=0;};
unsigned combo_cap(unsigned item,unsigned mode){return getbulletmax[item][mode];}
// Combi_00: Munition in Waffe bzw. Munition zusammenlegen.
unsigned combi_00(ComboContext& c,unsigned ps,std::uint32_t& moto,std::uint32_t& aite,const std::uint32_t* pip){
    unsigned ok=0;unsigned id1=Inventory::id(moto),id2=Inventory::id(aite);
    unsigned b1=moto&0xffff,b2=aite&0xffff;const unsigned t0=flags_of(moto),t=flags_of(aite);
    unsigned cap=(id1==42||id2==42)?300u:combo_cap(id2,c.game_mode);
    if(b2==cap&&id1!=19&&!(t&kEndless)){
        if(!c.message_active){c.message=156;ok=4;}
    }else{
        if(id1!=19)b2=(b2+b1)&0xffff;
        id2=unsigned(combidata[ps+1]);
        cap=(id1==42||id2==42)?300u:combo_cap(id2,c.game_mode);
        if(b2>cap&&!(t&kEndless)){
            if(!(t0&kEndless)){b1=(b2-cap)&0xffff;b2=cap;ok=2;}
            else{b2=cap;ok=2;}
        }else{
            if(t&kEndless)b2=cap;
            if(id1!=19&&!(t0&kEndless)){id1=0;b1=0;ok=1;}
        }
    }
    if(id1==19){id1=0;b1=0;ok=1;if(Inventory::id(pip[pip[0]])==9)ok=129;}
    id1|=t0&kEndless;id2|=t;
    moto=(id1<<16)|b1;aite=(id2<<16)|b2;
    return ok;
}
// Munitionssorte von Item 6/7 aus den Flags 0x7000/0x2000.
unsigned shell_kind(unsigned id2,unsigned flags){
    if(id2==6)switch(flags&0x7000){case 0x1000:return 16;case 0x2000:return 17;case 0x4000:return 37;default:return 15;}
    return (flags&0x2000)?36:18;
}
unsigned shell_flag(unsigned id2,unsigned id1){
    if(id2==6)switch(id1){case 16:return 0x1000;case 17:return 0x2000;case 37:return 0x4000;default:return 0;}
    if(id2==7)return id1==36?0x2000:0;
    return 0;
}
// Combi_01: Munitionssorte in Waffe 6/7 tauschen (Grenze 999 bzw. 300).
unsigned combi_01(unsigned ps,std::uint32_t& moto,std::uint32_t& aite){
    unsigned id1=Inventory::id(moto),id2=Inventory::id(aite),id3=0;
    unsigned b1=moto&0xffff,b2=aite&0xffff;const unsigned t0=flags_of(moto),t1=flags_of(aite);
    const unsigned cap=(id1==42||id2==42)?300u:999u;
    if(id2==6||id2==7){id3=shell_flag(id2,id1);id1=shell_kind(id2,t1);}
    b2=(b2+b1)&0xffff;
    id3=id3|unsigned(combidata[ps+1])|(t0&kEndless)|(t1&kEndless);
    unsigned ok;
    if(cap<b2){b1=(b2-cap)&0xffff;b2=cap;ok=3;}else{id1=0;b1=0;ok=1;}
    moto=(id1<<16)|b1;aite=(id3<<16)|b2;
    return ok;
}
// Combi_02: geladene Munition gegen andere Sorte tauschen (inkl. Schiesspulver).
unsigned combi_02(const ComboContext& c,unsigned ps,std::uint32_t& moto,std::uint32_t& aite){
    unsigned ok=0;bool kayaku=false;
    unsigned id1=Inventory::id(moto),id2=Inventory::id(aite),id3=0;
    unsigned b1=moto&0xffff,b2=aite&0xffff;const unsigned t0=flags_of(moto),t=flags_of(aite);
    if(id2==6||id2==7){
        if(c.equipped==c.listcsr_0||c.equipped==c.listcsr_1)ok=128;
        if(id2==7&&(id1==35||id1==130)){kayaku=true;id3=0x2000;}
        else id3=shell_flag(id2,id1);
        id1=shell_kind(id2,t);
    }
    id1|=t0&kEndless;
    if(id2==7&&kayaku&&!b2)return 0; // "sine": nichts verändert
    id3=id3|unsigned(combidata[ps+1])|(t&kEndless)|(t0&kEndless);
    if(kayaku){const unsigned b3=b1;if(b2<b3){b1=(b1-b2)&0xffff;id1=35;}else{b1=(b2-b1)&0xffff;b2=b3;}}
    else std::swap(b1,b2);
    if(b1==0){id1=0;ok|=1;}else ok|=3;
    moto=(id1<<16)|b1;aite=(id3<<16)|b2;
    return ok;
}
// Combi_03: Bauteile ergeben ein neues Item mit Originalmenge.
unsigned combi_03(const ComboContext& c,unsigned ps,std::uint32_t& moto,std::uint32_t& aite){
    const unsigned product=unsigned(combidata[ps+1]);
    moto=0;aite=(product<<16)|(combo_cap(product,c.game_mode)&0xffff);
    return 1;
}
// Combi_04: Pulver + Bogenpfeile (Menge des einen verbraucht die des anderen).
unsigned combi_04(unsigned ps,std::uint32_t& moto,std::uint32_t& aite){
    unsigned ok=0;unsigned b1=moto&0xffff,b2=aite&0xffff;
    unsigned id1=Inventory::id(moto),id2=Inventory::id(aite);
    if(b2<=b1){b1-=b2;id2=unsigned(combidata[ps+1]);ok=128;}
    else{id1=unsigned(combidata[ps+1]);b2-=b1;}
    if(!b1){id1=0;ok|=1;}
    if(!b2){id2=0;ok|=2;}
    moto=(id1<<16)|b1;aite=(id2<<16)|b2;
    return ok;
}
// Combi_05: Batterie-/Akkustufen (dbwtbl).
unsigned combi_05(const ComboContext& c,std::uint32_t& moto,std::uint32_t& aite){
    static constexpr unsigned short table[4][4]={{122,123,124,66},{123,124,66,0},{124,66,0,0},{66,0,0,0}};
    const unsigned b1=moto&0xffff;unsigned row=0;
    switch(Inventory::id(aite)){case 79:row=0;break;case 122:row=1;break;case 123:row=2;break;case 124:row=3;break;default:break;}
    const unsigned product=table[row][(b1-1)&3];
    moto=0;aite=(product<<16)|(combo_cap(product,c.game_mode)&0xffff);
    return 1;
}
}
Inventory::CombineResult Inventory::combine_items(int rule,unsigned first,unsigned second,bool message_active){
    // ItemCombination subcsr 2 (sub1.c:5578-5933). `first` ist listcsr_0
    // (moto), `second` listcsr_1 (aite). Die Klasse combidata[ps+2] entscheidet
    // Mechanik und Richtung. Klasse 6 (Frage 155) beantwortet der Aufrufer vor
    // dem Aufruf mit Ja (mes_sel 100 -> Combi_03); ein Nein ruft nicht auf.
    // Combi_xx schreibt pip sofort, auch bei ok 4 (Combi_00 schreibt moto und
    // aite immer zurück, sub1.c:6744-6745); kein Zurückrollen.
    CombineResult r;
    if(first<1||second<1||first>=12||second>=12||first==second||rule<0)return r;
    // st->cmbitm = num1: Index der passenden ID; Produkt ps+1, Klasse ps+2.
    const unsigned ps=unsigned(rule);
    const unsigned kind=unsigned(combidata[ps+2])&0xff;
    // plp->wpnr_no steht beim WeaponSet-Aufruf nach dem Tausch noch auf der
    // Waffe von vorher; der Wert wird deshalb vor jeder Mutation abgenommen.
    const unsigned wpnr_before=weapon();
    ComboContext c{game_mode,slots()[0],first,second,message_active};
    auto& moto=slots()[first];auto& aite=slots()[second];
    unsigned ok=0;bool movedir=false;
    auto equip_from=[&](unsigned slot){weapon_set(slot,slot,wpnr_before);};
    switch(kind){
    case 0:ok=combi_00(c,ps,moto,aite,slots());if(ok&0x80){equip_from(second);ok&=~0x80u;}
        if(first<slots()[0]){if(ok==0)ok=1;}else if(ok!=4)ok=3;break;
    case 1:ok=combi_00(c,ps,aite,moto,slots());if(ok&0x80){equip_from(first);ok&=~0x80u;}
        if(second<slots()[0]){if(ok==0)ok=1;}else if(ok!=4)ok=3;movedir=true;break;
    case 2:ok=combi_01(ps,moto,aite);if(first<slots()[0]){if(ok!=3)ok=1;}else ok=3;break;
    case 3:ok=combi_02(c,ps,moto,aite);
        if(ok){if(ok&0x80){equip_from(second);ok&=~0x80u;}if(!(first<slots()[0]))ok=3;}else ok=5;break;
    case 4:ok=combi_02(c,ps,aite,moto);
        if(ok){if(ok&0x80){equip_from(first);ok&=~0x80u;}if(!(second<slots()[0]))ok=3;movedir=true;}else ok=5;break;
    case 5:ok=combi_01(ps,aite,moto);ok=second<slots()[0]?1:3;movedir=true;break;
    case 6:case 9:ok=combi_03(c,ps,moto,aite);ok=first<slots()[0]?1:3;break;
    case 7:ok=combi_04(ps,moto,aite);
        switch(ok&0x7f){case 0:if(!(ok&0x80))movedir=true;ok=3;break;case 1:ok=first<slots()[0]?1:3;break;case 2:ok=second<slots()[0]?1:3;break;default:break;}break;
    case 8:ok=combi_04(ps,aite,moto);
        switch(ok&0x7f){case 0:if(ok&0x80)movedir=true;ok=3;break;case 1:movedir=true;ok=second<slots()[0]?1:3;break;case 2:ok=first<slots()[0]?1:3;break;default:break;}break;
    case 10:ok=combi_05(c,moto,aite);ok=first<slots()[0]?1:3;break;
    case 11:ok=combi_05(c,aite,moto);ok=second<slots()[0]?1:3;movedir=true;break;
    default:throw std::runtime_error("combination class outside the original ItemCombination switch");
    }
    r.ok=ok;r.movedir=movedir;r.message=c.message;
    return r;
}
void Inventory::combine_finish(unsigned ok,unsigned first,unsigned second){
    // sub1.c:5963-5966 nach der Bewegung: ok 1 und pip[0] hinter einem der
    // beiden Cursor senkt pip[0] um eins (der verbrauchte Eintrag rückt durch
    // das folgende ItemSort nach).
    if(ok==1&&(slots()[0]>first||slots()[0]>second)&&slots()[0]>1)--slots()[0];
}
bool Inventory::combine(unsigned first,unsigned second,unsigned* out,unsigned* result_slot){
    if(out)*out=0;if(result_slot)*result_slot=0;
    const int rule=combination_rule(first,second);
    if(rule<0)return false;
    const auto r=combine_items(rule,first,second);
    if(out)*out=r.message;
    // ok 4 (voll) und 5 (nichts) bleiben in subcsr 1 (sub1.c:5613-5616/5925-5937).
    if(!r.ok||r.ok==4||r.ok==5)return false;
    combine_finish(r.ok,first,second);
    // subcsr 3 -> 4: movedir 0 setzt listcsr_0 = listcsr_1 (sub1.c:5980-5984).
    if(result_slot)*result_slot=r.movedir?first:second;
    sort(); // SpriteOnOff des folgenden Statusframes (sub1.c:8293-8296, subcsr != 3)
    return true;
}
// Event-Inventarmutationen (event.c). Sie arbeiten absichtlich roh auf
// sys->itm, ohne ItemSort; das ist das Originalverhalten.
void Inventory::script_item_lost(unsigned item){
    // bhPlItemLost (0x31): erster Treffer in pip[2..11]. Die Nachführung
    // vergleicht pip[0] mit dem Slotindex, das Lösen aber mit dem absoluten
    // itm-Index (ply_id*16 + slot). Für Spieler 0 sind beide gleich, der
    // ausgerüstete Slot wird dort gelöst; für Spieler 1..3 trifft der zweite
    // Vergleich nie, pip[0] rückt dann nur um eins nach. Danach rücken die
    // Einträge um eins auf.
    auto* pip=slots();
    for(unsigned cnt=2;cnt<12;++cnt){
        if(id(pip[cnt])!=item)continue;
        pip[cnt]=0;
        if(cnt<=pip[0]&&pip[0]){if(pip[0]==player*16+cnt)pip[0]=0;else --pip[0];}
        // Original kopiert bis einschließlich pip[12] (erster Eintrag des
        // nächsten Blocks) in pip[11]; innerhalb von itm[384] gültig.
        for(;cnt<12;++cnt)pip[cnt]=items[player*16+cnt+1];
        return;
    }
}
void Inventory::script_item_give(unsigned owner,unsigned item,unsigned mode){
    // bhItemGetGet (0xb2): itm[owner*16 + 2] wird überschrieben; Item 34 erhält
    // 500 Schuss. Modus 0 rüstet aus, Modus 2 zusätzlich unendlich.
    //
    // Das Original prüft den Operanden nicht und greift direkt in sys->itm
    // (384 Einträge). Besitzer 4..23 landen dort in den Kisten-/Sonderbereichen
    // — dieselbe Wirkung wird hier übernommen. Ab 24 läge der Zugriff hinter
    // itm; das meldet die VM als blockierten Befehl, statt Speicher zu
    // überschreiben. sys->ply_wno hat vier Einträge, deshalb gilt die
    // Partnerwaffe nur für 0..3.
    if(owner*16+2>=items.size())throw std::runtime_error("script item gift owner outside the original itm array");
    auto* pip=items.data()+owner*16;
    pip[2]=std::uint32_t(item&0xff)<<16;
    if(item==34)pip[2]+=500;
    auto set_partner=[&]{if(item!=34)return;if(owner>=partner_weapon.size())throw std::runtime_error("script item gift owner outside the original ply_wno array");partner_weapon[owner]=8;};
    if(mode==0){pip[0]=2;set_partner();}
    else if(mode==2){pip[2]|=0x08000000u;pip[0]=2;set_partner();}
}
void Inventory::script_item_lost_ex(unsigned item,unsigned box_slot){
    // bhPlItemLostEx (0xbf): wie bhPlItemLost, legt den Eintrag aber vorher in
    // itm[224 + box_slot] (zweite Sonderbox) ab. Der Vergleich mit pip[0] ist
    // hier `pip[0] >= cnt` — identisch zu `cnt <= pip[0]` oben; das Lösen
    // vergleicht ebenso den absoluten itm-Index.
    auto* pip=slots();
    for(unsigned cnt=2;cnt<12;++cnt){
        if(id(pip[cnt])!=item)continue;
        if(224+box_slot<items.size())items[224+box_slot]=pip[cnt];
        pip[cnt]=0;
        if(pip[0]>=cnt&&pip[0]){if(pip[0]==player*16+cnt)pip[0]=0;else --pip[0];}
        for(;cnt<12;++cnt)pip[cnt]=items[player*16+cnt+1];
        return;
    }
}
void Inventory::script_arms_set(unsigned item){
    // bhArmsItemSet (0xc1): erster Slot mit der ID in den ersten 8 bzw. 10
    // Taschenslots wird ausgerüstet; kein Treffer lässt pip[0] unverändert.
    const unsigned max=side_pack?10:8;
    for(unsigned i=0;i<max;++i)if(id(slots()[i+2])==item){slots()[0]=i+2;return;}
}
void Inventory::extra_game_item_init(unsigned cng_pid,unsigned costume,bool ssd_special){
    // sub1.c:8553-8604 ExtraGameItemInit: Zeile aus sys->costume (!= 0 -> 4) oder sys->cng_pid; die Bank
    // ist immer itm[cng_pid*16]. ssd_flg 0x80000000 (sub1.c:8579) setzt pip[2] = 0x80B0001 vor und
    // beginnt bei i = 1; in den Originalquellen schreibt dieses Bit niemand. Die Eintraege sind
    // (itemid << 16) | getbulletmax[(unsigned char)itemid][gm_mode] in 32 Bit, mit den Hochbytes der
    // Tabellenwerte (z. B. 10247 = 0x2807 -> Id 7 mit Flagbits 0x28).
    static const int itemset[5][16]={
        {8,8,9,10247,2060,29,29,29,29,0,0,0,0,0,0,0},
        {6,8,4,2080,2062,29,29,0,0,0,0,0,0,0,0,0},
        {5,8,2081,2082,29,29,0,0,0,0,0,0,0,0,0,0},
        {4,8,29,29,29,0,0,0,0,0,0,0,0,0,0,0},
        {7,8,6,2050,29,2063,2064,2065,0,0,0,0,0,0,0,0}};
    const unsigned row=costume!=0?4u:cng_pid;
    if(row>=5)throw std::runtime_error("ExtraGameItemInit: cng_pid outside itemset[5]");
    if(cng_pid*16+16>items.size())throw std::runtime_error("ExtraGameItemInit: cng_pid bank outside itm[384]");
    auto* pip=items.data()+cng_pid*16;
    unsigned i=0;
    if(ssd_special){i=1;pip[2]=0x80B0001u;}
    for(unsigned j=0;j<unsigned(itemset[row][0]);++i,++j){
        const int itemid=itemset[row][j+1];
        pip[i+2]=(std::uint32_t(itemid)<<16)|getbulletmax[std::uint8_t(itemid)][game_mode];
    }
}
void Inventory::script_item_give_ex(unsigned owner,unsigned item,unsigned equip,unsigned amount){
    // bhItemGetGetEx (0xc2): erster freier Slot (exakt 0) in den ersten 8/10
    // Slots des Besitzers erhält (item << 16) | amount. Item 33 rückt pip[0]
    // um eins weiter; equip 0 rüstet den neuen Slot aus. Kein freier Slot:
    // nichts geschieht.
    // Wie bei bhItemGetGet prüft das Original den Besitzer nicht; der Zugriff
    // bleibt innerhalb von itm, solange owner*16+11 dort liegt.
    if(owner*16+11>=items.size())throw std::runtime_error("script item gift owner outside the original itm array");
    auto* pip=items.data()+owner*16;
    const unsigned max=side_pack?10:8;
    for(unsigned i=0;i<max;++i)if(pip[i+2]==0){
        pip[i+2]=(std::uint32_t(item&0xff)<<16)|(amount&0xffff);
        if(item==33)++pip[0];
        if(equip==0)pip[0]=i+2;
        return;
    }
}
void Inventory::script_item_change(unsigned item,unsigned replacement){
    // bhPlItemChangeM (0xc7): erster Treffer wird durch die nackte neue ID
    // ersetzt (Menge und Flags 0).
    for(unsigned cnt=2;cnt<12;++cnt)if(id(slots()[cnt])==item){slots()[cnt]=std::uint32_t(replacement&0xff)<<16;return;}
}
void Inventory::script_item_amount(unsigned item,unsigned amount){
    // bhPlItemTamaSet (0xc9): erster Treffer erhält (item << 16) + amount;
    // vorhandene Flags gehen dabei verloren.
    for(unsigned cnt=2;cnt<12;++cnt)if(id(slots()[cnt])==item){slots()[cnt]=(std::uint32_t(item&0xff)<<16)+(amount&0xffff);return;}
}
bool Inventory::transform(unsigned slot,unsigned expected,unsigned replacement){
    if(slot<1||slot>=12||replacement>=156||id(slots()[slot])!=expected)return false;const auto saved=items;
    slots()[slot]=replacement?pack(replacement,itemdata[replacement].max):0;
    if(slots()[0]==slot&&(!replacement||!(itemdata[replacement].type&1)))slots()[0]=0;
    if(occupied()>capacity()){items=saved;return false;}return true;
}
bool Inventory::deposit(unsigned slot,unsigned box){if(slot<2||slot>=12||box!=64||!id(slots()[slot]))return false;unsigned i=0;while(i<128&&items[box+i])++i;if(i==128)return false;items[box+i]=slots()[slot];slots()[slot]=0;if(slots()[0]==slot){slots()[0]=0;wpnr_no=0;request_weapon_reload();} /* wie ItemBoxChange sub1.c:7929-7931 */compact();return true;}
bool Inventory::withdraw(unsigned index,unsigned box){if(box!=64||index>=128||!items[box+index])return false;auto item=items[box+index];if(!acquire(id(item),quantity(item),(item>>16)&0xff00))return false;items[box+index]=0;return true;}
bool Inventory::box_change(unsigned slot,unsigned index,unsigned box,unsigned stage,unsigned room,unsigned* out,unsigned* cursor_slot,unsigned cursor_cell){
    // ItemBoxChange nach dem Original-Maschinencode (0x002A53E0; die Decomp ist
    // nur 67,97 % matching, Ablauf belegt in work/native_port_inventory_contract.md).
    // `slot` ist listcsr_0, `index` ibcsr, st->bxp = itm[box].
    if(out)*out=0;const unsigned count=box==64?128:(box==192||box==224)?32:0;
    if(!count||slot<1||slot>=capacity()+2||index>=count||player>=3)return false;
    std::uint32_t* const bxp=items.data()+box;
    const unsigned lt_c=slot,sakai=wide_boundary(); // letzter ItemSort-Stand vor dem Tausch
    unsigned lt_c2=lt_c,lt_c3=0,chk=0;
    const std::uint32_t pocket=slots()[lt_c];   // $t3
    std::uint32_t incoming=bxp[index];           // $t4 (itemid3)
    std::uint32_t parked=0;                      // $v1: Wert für pip[lt_c3]
    const unsigned id1=id(pocket),id2=id(incoming);
    const unsigned arms=id(slots()[slots()[0]]);
    if(player==0){
        if((type_of(id2)&0x40)&&stage==9&&room==26){if(out)*out=151;return false;}
        if((type_of(id1)&0x40)&&(id1==105||id1==106||id1==121)){if(out)*out=150;return false;}
        if(!(type_of(id1)&0x400)&&id1&&stage==0&&room==9){if(out)*out=200;return false;}
    }
    const unsigned max=side_pack?10:8;const unsigned free=free_units(slots(),max);
    // Sonderslot: das Boxitem wird in pip[max+1] abgelegt, pip[1] wird leer;
    // die anschließende Sortierung holt es nach vorn.
    auto park=[&](){lt_c3=max+1;parked=incoming;lt_c2=lt_c;incoming=0;chk=1;};
    if(type_of(id2)&kWide){
        if(lt_c==1){if(free>=2)park();}
        else if(type_of(id1)&kWide)chk=1;
        else if(id1){if(free>=1)chk=1;}
        else if(free>=2)chk=1;
    }else if(lt_c==1){
        if(standard_weapon(player,id2))chk=1;
        else if(free>=1)park();
        else if(!id2)chk=1;
    }else{
        chk=1;
        // Grundwaffe aus der Box in einen Taschenslot: sie gehört nach pip[1];
        // das bisherige pip[1] fällt in den gewählten Slot (pip[lt_c] = 0 aus $v1).
        if(standard_weapon(player,id2)){lt_c3=lt_c;lt_c2=1;parked=0;}
    }
    unsigned stack_slot=0;
    if(type_of(id2)&kStack){
        if(id1==id2){chk=2;stack_slot=lt_c;}
        else if(!id1)for(unsigned x=0;x<max;++x)
            if(id(slots()[x+2])==id2&&quantity(slots()[x+2])<999){chk=2;stack_slot=x+2;break;}
    }
    if(chk==1){
        if(slots()[0]==lt_c){
            // Ausgerüstete Waffe verlässt die Tasche: pip[0] = 0, plp->mode3 =
            // plp->wpnr_no = 0, sys->mn_mode0 = 3 (Kampfbesitzer über weapon_change).
            slots()[0]=0;wpnr_no=0;weapon_change_={true,0,false};request_weapon_reload(); /* sub1.c:7929-7931 */
        }
        bxp[index]=pocket;
        slots()[lt_c2]=incoming;
        if(lt_c3)slots()[lt_c3]=parked;
        auto& eq=slots()[0];
        if(eq>=2){
            if(lt_c==1){
                if((type_of(id2)&kWide)&&!(type_of(id(slots()[eq]))&kWide))++eq;
            }else if(!id2||standard_weapon(player,id2)){
                if(lt_c<eq)--eq;
            }else if(id1){
                if(type_of(id2)&kWide){if(!(type_of(id1)&kWide)&&!(type_of(arms)&kWide)&&lt_c<eq)++eq;}
                else if((type_of(id1)&kWide)&&eq<lt_c&&(type_of(arms)&kWide))--eq;
            }else if((type_of(id2)&kWide)&&!(type_of(arms)&kWide))++eq;
        }
        // ELF 0x2A5C5C-0x2A5D40 / sub1.c:7984-7994: sakai gilt noch VOR ItemSort. Der
        // Slotindex listcsr_0 aendert sich beim Breitenwechsel, die Rasterzelle listcsr_2 nicht.
        if(cursor_slot&&lt_c!=1&&(lt_c>sakai||((cursor_cell&1u)&&lt_c==sakai-1))){
            if((type_of(id1)&kWide)&&!(type_of(id2)&kWide))*cursor_slot=lt_c+1;
            else if((type_of(id2)&kWide)&&!(type_of(id1)&kWide))*cursor_slot=lt_c-1;
        }
        // ItemBoxChange ruft kein ItemSort (Disassembly 0x002A53E0: nur
        // bhSetMessage/Combi_01); den geparkten Eintrag holt das ItemSort
        // desselben Statusframes nach vorn (SpriteOnOff, sub1.c:8296).
        return true;
    }
    if(chk==2){
        // Stapeln: erste Regel des Boxitems mit der Taschen-ID -> Combi_01.
        if(itemdata[id2].cmb>=0){
            const unsigned pos=unsigned(itemdata[id2].cmb);unsigned n=unsigned(combidata[pos]);
            for(unsigned x=pos+1;n;--n,x+=3)if(unsigned(combidata[x])==id1){
                combi_01(x,bxp[index],slots()[stack_slot]);break;
            }
        }
        return true;
    }
    if(out)*out=159;
    return false;
}

}

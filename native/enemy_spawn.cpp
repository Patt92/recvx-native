#include "enemy_spawn.h"
#include "ninja_math.h"
#include <cmath>
namespace recvx {
namespace {
// bhEne01.c:1345 / en04.c ang_tbl — the four stair/direction angles, indexed by
// an area's prm2. 0°, 270°, 180°, 90° in the 16-bit Ninja angle unit.
const int kaidan_ang[4]={0,49152,32768,16384};

// bhEne_PosCheck (zonzon.c:402), verbatim: x<=px && !(x+w<px) && z<=pz &&
// !(z+h<pz) — both bounds inclusive. This is the documented shared service
// recvx_t3_pos_check (native/en03_lib.h:103); its body is pending from the
// parallel T3 work and not yet in the build, so this local copy mirrors the
// original exactly and is the connection point once that body lands.
bool pos_check(float x,float z,float px,float pz,float w,float d){
    return px<=x && !(px+w<x) && pz<=z && !(pz+d<z);
}

// bhEne_EnemyAtariCheck (zonzon.c:373): the first live type-2 area that
// contains pos on `floor`, whose prm0 names this enemy id and whose prm1 equals
// the requested trigger type. The original walks rom->flrp then sys->mflrp; the
// host hands both over as one already-concatenated `areas` vector (world.cpp:113
// reads the room floor table, and the mobile-terrain tail is appended by the
// same contract), so a single forward scan reproduces the first-match order.
int atari_check(const std::vector<EnemyArea>& areas,const NJS_POINT3& pos,int floor,unsigned char id,unsigned char type){
    for(std::size_t i=0;i<areas.size();++i){const auto& fp=areas[i];
        if((fp.flg&1u)&&fp.type==2&&pos_check(pos.x,pos.z,fp.px,fp.pz,fp.w,fp.d)&&fp.flr_no==floor&&fp.prm0==id&&fp.prm1==type)
            return int(i);
    }
    return -1;
}

// bhEne04_EnemyAtariCheck2 (en04.c:1340): like atari_check but keyed on prm3
// instead of a position — the reposition target for a given trigger index.
int atari_check2(const std::vector<EnemyArea>& areas,int floor,unsigned char id,unsigned char type,unsigned char ip){
    for(std::size_t i=0;i<areas.size();++i){const auto& fp=areas[i];
        if((fp.flg&1u)&&fp.type==2&&fp.flr_no==floor&&fp.prm0==id&&fp.prm1==type&&fp.prm3==ip)
            return int(i);
    }
    return -1;
}

// bhEne04_MVType01 (en04.c:1117): a type-1 waiting dog wakes when the player
// stands in a prm1=1 area on the player's floor AND the dog itself stands in a
// prm1=2 area on the dog's floor whose prm3 matches the player's area. No
// reposition, no random, no room flag. On a hit the dog becomes type 0.
bool activate_type1(EnemySpawn& enemy,const NJS_POINT3& player,int player_floor,const std::vector<EnemyArea>& areas){
    const int first=atari_check(areas,player,player_floor,enemy.id,1);
    if(first<0)return false;
    const unsigned char no=areas[first].prm3;
    const int second=atari_check(areas,{enemy.px,enemy.py,enemy.pz},enemy.flr_no,enemy.id,2);
    if(second<0||areas[second].prm3!=no)return false;
    enemy.type=0;
    return true;
}

// bhEne04_AtariCheck (en04.c:1187): the first contact for a prm1=6 trigger.
// Marks the trigger's prm3 bit in the shared en04_atari_flg (context.areas),
// then — when it is the last unseen trigger or a coin flip says so — looks up
// the matching prm1=7 reposition target, moves the dog onto it, faces it down
// kaidan_ang, raises the room flag and sets en04_contact_flg (context.contact).
// rand() is drawn only when another unseen trigger still exists, matching the
// original's short-circuit `atr_num == 0 || rand() % 2 == 0`.
bool en04_atari_check(EnemySpawn& enemy,const NJS_POINT3& player,int player_floor,std::vector<EnemyArea>& areas,EnemySpawnContext& context){
    const int hp=atari_check(areas,player,player_floor,enemy.id,6);
    if(hp<0)return false;
    const unsigned char ip=areas[hp].prm3;
    if(32<ip)return false;
    if(context.areas&(1u<<ip))return false;
    context.areas|=1u<<ip;
    unsigned atr_num=0;
    for(std::size_t i=0;i<areas.size();++i){const auto& fp=areas[i];
        if((fp.flg&1u)&&fp.type==2&&int(i)!=hp&&fp.flr_no==enemy.flr_no&&fp.prm0==enemy.id&&fp.prm1==6&&!(context.areas&(1u<<fp.prm3)))
            ++atr_num;
    }
    if(atr_num!=0&&context.rand()%2!=0)return false;
    const int hp2=atari_check2(areas,enemy.flr_no,enemy.id,7,ip);
    if(hp2<0)return false;
    enemy.px=areas[hp2].px+areas[hp2].w/2.0f;
    enemy.pz=areas[hp2].pz+areas[hp2].d/2.0f;
    enemy.ay=kaidan_ang[areas[hp2].prm2&3];
    context.room_flags|=1u<<(areas[hp2].prm3+16);
    context.contact|=1u;
    return true;
}

// bhEne04_AtariCheck2 (en04.c:1263): once any dog has made contact
// (context.contact bit 0), a still-waiting dog repositions instead. It gathers
// every live prm1=6 trigger on the dog's floor (not excluding the player's
// current one), picks one at random when several exist, and repositions onto the
// matching prm1=7 target. The room flag is raised only on a one-in-three draw.
// rand() is drawn for the pick only when more than one trigger exists, and for
// the flag only when a target was actually found.
bool en04_atari_check2(EnemySpawn& enemy,const NJS_POINT3& player,int player_floor,const std::vector<EnemyArea>& areas,EnemySpawnContext& context){
    const int hp=atari_check(areas,player,player_floor,enemy.id,6);
    if(hp<0)return false;
    if(32<areas[hp].prm3)return false;
    int work[32];unsigned atr_num=0;
    for(std::size_t i=0;i<areas.size();++i){const auto& fp=areas[i];
        if((fp.flg&1u)&&fp.type==2&&int(i)!=hp&&fp.flr_no==enemy.flr_no&&fp.prm0==enemy.id&&fp.prm1==6){
            if(atr_num<32)work[atr_num]=int(i);++atr_num;
        }
    }
    if(atr_num==0)return false;
    const int pick=atr_num==1?work[0]:work[context.rand()%int(atr_num)];
    const int hp2=atari_check2(areas,enemy.flr_no,enemy.id,7,areas[pick].prm3);
    if(hp2<0)return false;
    enemy.px=areas[hp2].px+areas[hp2].w/2.0f;
    enemy.pz=areas[hp2].pz+areas[hp2].d/2.0f;
    enemy.ay=kaidan_ang[areas[hp2].prm2&3];
    if(context.rand()%3==0)context.room_flags|=1u<<(areas[hp2].prm3+16);
    return true;
}

// bhEne04_MVType02 (en04.c:1152): a type-2 waiting dog wakes on contact. If any
// dog already made contact it repositions (AtariCheck2); otherwise it tries the
// first-contact path (AtariCheck). Either success turns the dog into type 0.
bool activate_type2(EnemySpawn& enemy,const NJS_POINT3& player,int player_floor,std::vector<EnemyArea>& areas,EnemySpawnContext& context){
    if(context.contact&1u){
        en04_atari_check2(enemy,player,player_floor,areas,context);
        enemy.type=0;
        return true;
    }
    if(en04_atari_check(enemy,player,player_floor,areas,context)){enemy.type=0;return true;}
    return false;
}
}

// bhEne04_MVType00 (en04.c:1048): the type-4 door sequence. An already-active
// dog (type 0) in a running/attacking mode (mode2 2 or 3) opens the door whose
// area lies six units behind it along its facing. The probe point is
// px-6*sin(ay), pz-6*cos(ay) on the dog's floor; a prm1=4 hit whose prm2 names
// an idle object (obwp[prm2].mode1==0, fed as door_open==false) sets the door's
// angle from ang_tbl[prm3] and plays sound 70415. The original writes
// sys->obwp[prm2] (ayp/mode3/flg2) and calls bhEne04_SePlay; the host owns both,
// so the activation reports them through context.door_* instead.
bool trigger_type4_door(const EnemySpawn& enemy,const std::vector<EnemyArea>& areas,EnemySpawnContext& context){
    if(enemy.mode2!=2&&enemy.mode2!=3)return false;
    NJS_POINT3 probe{enemy.px-6.0f*njSin(enemy.ay),0.0f,enemy.pz-6.0f*njCos(enemy.ay)};
    const int hp=atari_check(areas,probe,enemy.flr_no,enemy.id,4);
    if(hp<0||areas[hp].prm1!=4||context.door_open)return false;
    context.door_open=true;
    context.door_angle=kaidan_ang[areas[hp].prm3&3];
    context.door_sound=70415;
    return true;
}

// PORTIERUNG (Policy 2026-10-09, Auftrag C01): Die Fallenaktivierung ist jetzt
// aus den Originalquellen (zonzon.c bhEne_EnemyAtariCheck/bhEne_PosCheck und
// en04.c bhEne04_MVType00/MVType01/MVType02/AtariCheck/AtariCheck2/
// EnemyAtariCheck2) eigenständig nachgebaut — ohne Generator, ohne Originalkern
// und ohne zweites Werk. Der Host ruft sie produktiv nur für wartende EN04-Hunde
// auf (world.cpp:768: identity==4, e.type!=0); die Aktivierung ist der einmalige
// Übergang Typ 1/2 → Typ 0 mit originaler Platzierung und Richtung.
//
// Offene Abhängigkeiten, bewusst nicht durch No-ops ersetzt:
//  - Die prm1=4-Türsequenz (Zweig unten) und die prm1=0-Nähe schreiben
//    sys->obwp[prm2] (ayp/mode3/flg2) und EXP0-Flags und rufen bhEne04_SePlay.
//    obwp ist im Vertrag nicht enthalten; der Zweig meldet Türwinkel und Sound
//    über context.door_*, und der Host muss enemy.mode2 (C04) sowie die
//    obwp-Übergabe anschließen. Solange der Host nur Typ 1/2 durchreicht, ist
//    der Typ-0-Zweig korrekt inaktiv.
//  - EN01 Typ 1 (Zombie) wird vom Host noch nicht durchgereicht (identity!=4
//    übersprungen). Sein Originalzweig ist keine einmalige Aktivierung, sondern
//    ein fortlaufendes Nähe-Flag (EXP0_I(0x40) |= 0x20000000, en01.c:4864). Der
//    Zweig unten bildet dieses Flag auf den Eintrittszeitpunkt ab; die laufende
//    Zombie-KI folgt in C03.
bool activate_enemy_spawn(EnemySpawn& enemy,NJS_POINT3 player,int floor,std::vector<EnemyArea>& areas,EnemySpawnContext& context){
    if(enemy.id==4){
        if(enemy.type==1)return activate_type1(enemy,player,floor,areas);
        if(enemy.type==2)return activate_type2(enemy,player,floor,areas,context);
        if(enemy.type==0)return trigger_type4_door(enemy,areas,context);
        return false;
    }
    if(enemy.id==1&&enemy.type==1){
        // en01.c:4864 bhEne01_MVType00: player inside a prm1=0 area on the
        // player's floor raises the proximity flag. Reported once per entry via
        // context.contact bit 1 so a caller can act on the transition.
        const bool inside=atari_check(areas,player,floor,enemy.id,0)>=0;
        const bool was=context.contact&2u;
        if(inside&&!was){context.contact|=2u;return true;}
        if(!inside)context.contact&=~2u;
        return false;
    }
    return false;
}
}

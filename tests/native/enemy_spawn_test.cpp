#include "enemy_spawn.h"
#include "ninja_math.h"
#include <iostream>
using namespace recvx;
// PORTIERUNG (Policy 2026-10-09, Auftrag C01): Positive Prüfungen der
// Fallenaktivierung gegen die Originalwerte aus zonzon.c (bhEne_EnemyAtariCheck,
// bhEne_PosCheck) und en04.c (bhEne04_MVType01/MVType02/AtariCheck/AtariCheck2/
// EnemyAtariCheck2). Geprüft werden echte Nachschlagen: Feldänderung (type→0),
// Bereichsverbrauch (context.areas), Kontaktbit (context.contact), Raumflag
// (context.room_flags) und RNG-Fortschritt — einschließlich Grenz- und
// Nichttrefferfällen, die keine unbelegte Mutation auslösen dürfen.
static int failures=0;
static void expect(bool ok,const char* what){if(!ok){std::cerr<<"FAIL: "<<what<<"\n";++failures;}}
// A live type-2 area on floor `flr`, rectangle [px,px+w]x[pz,pz+d], keyed by
// (prm0=id, prm1=type, prm2, prm3).
static EnemyArea area(unsigned char id,unsigned char prm1,int flr,float px,float pz,float w,float d,unsigned char prm2=0,unsigned char prm3=0){
    EnemyArea a;a.flg=1;a.type=2;a.flr_no=flr;a.px=px;a.pz=pz;a.w=w;a.d=d;a.prm0=id;a.prm1=prm1;a.prm2=prm2;a.prm3=prm3;return a;
}
int main(){
    _Make_SinTable(); // njSin/njCos for the type-4 probe point
    // --- bhEne_PosCheck inclusive bounds (zonzon.c:402) ---
    // A point exactly on the far edge is inside; just past it is outside.
    {
        std::vector<EnemyArea> areas{area(4,1,0,0,0,10,10,0,7)};
        EnemySpawnContext ctx;EnemySpawn dog;dog.id=4;dog.type=1;dog.flr_no=0;
        // Player on the far corner (10,10) is inside → type-1 first check hits.
        // Second check needs the dog inside a prm1=2 area with matching prm3.
        areas.push_back(area(4,2,0,50,50,10,10,0,7));
        dog.px=55;dog.pz=55;
        NJS_POINT3 player{10,0,10};
        expect(activate_enemy_spawn(dog,player,0,areas,ctx),"type1 inclusive far edge activates");
        expect(dog.type==0,"type1 activation sets type 0");
    }
    // --- Type-1 (MVType01): both areas must match on prm3 ---
    {
        std::vector<EnemyArea> areas{area(4,1,0,0,0,10,10,0,3),area(4,2,0,20,20,10,10,0,3)};
        EnemySpawnContext ctx;EnemySpawn dog;dog.id=4;dog.type=1;dog.flr_no=0;dog.px=25;dog.pz=25;
        NJS_POINT3 player{5,0,5};
        expect(activate_enemy_spawn(dog,player,0,areas,ctx),"type1 matching prm3 activates");
        expect(dog.type==0,"type1 sets type 0");
        expect(ctx.room_flags==0,"type1 raises no room flag");
        expect(ctx.contact==0,"type1 sets no contact bit");
    }
    {
        // Mismatched prm3 → no activation, no mutation.
        std::vector<EnemyArea> areas{area(4,1,0,0,0,10,10,0,3),area(4,2,0,20,20,10,10,0,4)};
        EnemySpawnContext ctx;EnemySpawn dog;dog.id=4;dog.type=1;dog.flr_no=0;dog.px=25;dog.pz=25;
        expect(!activate_enemy_spawn(dog,{5,0,5},0,areas,ctx),"type1 prm3 mismatch does not activate");
        expect(dog.type==1,"type1 mismatch leaves type 1");
    }
    {
        // Player outside the prm1=1 area → no activation.
        std::vector<EnemyArea> areas{area(4,1,0,0,0,10,10,0,3),area(4,2,0,20,20,10,10,0,3)};
        EnemySpawnContext ctx;EnemySpawn dog;dog.id=4;dog.type=1;dog.flr_no=0;dog.px=25;dog.pz=25;
        expect(!activate_enemy_spawn(dog,{100,0,100},0,areas,ctx),"type1 player outside does not activate");
    }
    // --- Type-2 first contact (AtariCheck): reposition, ay, room flag, contact ---
    {
        // Player in a prm1=6 trigger (prm3=2). Dog on floor 0. A prm1=7 target
        // with prm3=2, prm2=1 (kaidan 49152) repositions the dog.
        std::vector<EnemyArea> areas{area(4,6,0,0,0,10,10,0,2),area(4,7,0,100,100,20,20,1,2)};
        EnemySpawnContext ctx;ctx.rand=[]{return 0;}; // rand()%2==0 → proceed
        EnemySpawn dog;dog.id=4;dog.type=2;dog.flr_no=0;dog.px=5;dog.pz=5;dog.ay=0;
        expect(activate_enemy_spawn(dog,{5,0,5},0,areas,ctx),"type2 first contact activates");
        expect(dog.type==0,"type2 sets type 0");
        expect(dog.px==110&&dog.pz==110,"type2 repositions to target center");
        expect(dog.ay==49152,"type2 faces kaidan_ang[prm2]");
        expect((ctx.areas&(1u<<2))!=0,"type2 consumes the trigger prm3 bit");
        expect((ctx.room_flags&(1u<<(2+16)))!=0,"type2 raises the room flag");
        expect((ctx.contact&1u)!=0,"type2 sets the contact bit");
    }
    {
        // Second trigger already consumed → AtariCheck returns 0 (bit set), no
        // re-activation, no further mutation.
        std::vector<EnemyArea> areas{area(4,6,0,0,0,10,10,0,2),area(4,7,0,100,100,20,20,1,2)};
        EnemySpawnContext ctx;ctx.areas=1u<<2;ctx.rand=[]{return 0;};
        EnemySpawn dog;dog.id=4;dog.type=2;dog.flr_no=0;
        expect(!activate_enemy_spawn(dog,{5,0,5},0,areas,ctx),"type2 already-consumed trigger does not activate");
        expect(dog.type==2,"type2 consumed trigger leaves type 2");
    }
    {
        // rand()%2!=0 with another unseen trigger → skip reposition, no flag.
        // Two prm1=6 triggers (prm3=2 and prm3=5); player in the prm3=2 one.
        std::vector<EnemyArea> areas{area(4,6,0,0,0,10,10,0,2),area(4,6,0,50,50,10,10,0,5),area(4,7,0,100,100,20,20,1,2)};
        EnemySpawnContext ctx;ctx.rand=[]{return 1;}; // rand()%2==1 → skip
        EnemySpawn dog;dog.id=4;dog.type=2;dog.flr_no=0;
        expect(!activate_enemy_spawn(dog,{5,0,5},0,areas,ctx),"type2 odd rand with unseen trigger does not activate");
        expect((ctx.areas&(1u<<2))!=0,"type2 still consumes the entered trigger bit");
        expect(ctx.room_flags==0,"type2 skipped reposition raises no room flag");
    }
    // --- Type-2 reposition (AtariCheck2) once contact already made ---
    {
        // contact bit set → MVType02 takes the AtariCheck2 branch. The original
        // excludes the player's own trigger (fp != hp) when gathering candidates,
        // so repositioning needs a second trigger (prm3=4) with its own target.
        // Single candidate → no rand for the pick; rand()%3==0 raises the flag.
        std::vector<EnemyArea> areas{area(4,6,0,0,0,10,10,0,2),area(4,6,0,40,40,10,10,0,4),area(4,7,0,200,200,10,10,2,4)};
        EnemySpawnContext ctx;ctx.contact=1;ctx.rand=[]{return 0;};
        EnemySpawn dog;dog.id=4;dog.type=2;dog.flr_no=0;
        expect(activate_enemy_spawn(dog,{5,0,5},0,areas,ctx),"type2 reposition branch activates");
        expect(dog.type==0,"type2 reposition sets type 0");
        expect(dog.px==205&&dog.pz==205,"type2 reposition uses target center");
        expect(dog.ay==32768,"type2 reposition faces kaidan_ang[2]");
        expect((ctx.room_flags&(1u<<(4+16)))!=0,"type2 reposition raises room flag on rand%3==0");
    }
    {
        // rand()%3!=0 → no room flag, but still repositioned and activated.
        std::vector<EnemyArea> areas{area(4,6,0,0,0,10,10,0,2),area(4,6,0,40,40,10,10,0,4),area(4,7,0,200,200,10,10,2,4)};
        EnemySpawnContext ctx;ctx.contact=1;ctx.rand=[]{return 1;}; // 1%3==1
        EnemySpawn dog;dog.id=4;dog.type=2;dog.flr_no=0;
        expect(activate_enemy_spawn(dog,{5,0,5},0,areas,ctx),"type2 reposition activates regardless of flag draw");
        expect(ctx.room_flags==0,"type2 reposition skips room flag on rand%3!=0");
    }
    {
        // No prm1=7 target for the picked trigger → AtariCheck2 finds none, but
        // MVType02 still activates (the original sets type 0 unconditionally in
        // the contact branch). No reposition, no flag.
        std::vector<EnemyArea> areas{area(4,6,0,0,0,10,10,0,4)};
        EnemySpawnContext ctx;ctx.contact=1;ctx.rand=[]{return 0;};
        EnemySpawn dog;dog.id=4;dog.type=2;dog.flr_no=0;dog.px=5;dog.pz=5;
        expect(activate_enemy_spawn(dog,{5,0,5},0,areas,ctx),"type2 reposition activates with no target");
        expect(dog.px==5&&dog.pz==5,"type2 no target leaves position");
        expect(ctx.room_flags==0,"type2 no target raises no flag");
    }
    // --- Non-matching id/type: no mutation ---
    {
        std::vector<EnemyArea> areas{area(4,6,0,0,0,10,10,0,2),area(4,7,0,100,100,20,20,1,2)};
        EnemySpawnContext ctx;EnemySpawn dog;dog.id=4;dog.type=0; // already active
        expect(!activate_enemy_spawn(dog,{5,0,5},0,areas,ctx),"active dog (type 0) is not re-activated");
    }
    // --- prm3 > 32 guard (AtariCheck) ---
    {
        std::vector<EnemyArea> areas{area(4,6,0,0,0,10,10,0,33)};
        EnemySpawnContext ctx;EnemySpawn dog;dog.id=4;dog.type=2;dog.flr_no=0;
        expect(!activate_enemy_spawn(dog,{5,0,5},0,areas,ctx),"type2 prm3>32 does not activate");
    }
    // --- Type-4 door sequence (MVType00): active dog, running mode, probe behind ---
    {
        // ay=0 → sin 0, cos 1 → probe = (px, pz-6). A prm1=4 area at (100,44)
        // size 10x10 contains (100,44). prm2 names the object, prm3 the angle.
        std::vector<EnemyArea> areas{area(4,4,0,100,44,10,10,3,1)};
        EnemySpawnContext ctx;EnemySpawn dog;dog.id=4;dog.type=0;dog.flr_no=0;dog.px=100;dog.pz=50;dog.ay=0;dog.mode2=2;
        expect(activate_enemy_spawn(dog,{0,0,0},0,areas,ctx),"type4 door triggers in running mode");
        expect(ctx.door_open,"type4 raises the door-open request");
        expect(ctx.door_angle==49152,"type4 door angle from ang_tbl[prm3]");
        expect(ctx.door_sound==70415,"type4 plays the door sound");
    }
    {
        // mode2 not 2/3 → the door branch is inert (dog still walking/idle).
        std::vector<EnemyArea> areas{area(4,4,0,100,44,10,10,3,1)};
        EnemySpawnContext ctx;EnemySpawn dog;dog.id=4;dog.type=0;dog.flr_no=0;dog.px=100;dog.pz=50;dog.ay=0;dog.mode2=0;
        expect(!activate_enemy_spawn(dog,{0,0,0},0,areas,ctx),"type4 inert outside running modes");
        expect(!ctx.door_open,"type4 inert raises no door");
    }
    {
        // Probe point outside the area (dog facing away) → no trigger.
        std::vector<EnemyArea> areas{area(4,4,0,100,44,10,10,3,1)};
        EnemySpawnContext ctx;EnemySpawn dog;dog.id=4;dog.type=0;dog.flr_no=0;dog.px=100;dog.pz=50;dog.ay=32768;dog.mode2=3;
        // ay=180° → sin 0, cos -1 → probe = (100, 56), outside [44,54].
        expect(!activate_enemy_spawn(dog,{0,0,0},0,areas,ctx),"type4 probe outside the area does not trigger");
    }
    {
        // Door already moving (door_open pre-set) → no re-trigger.
        std::vector<EnemyArea> areas{area(4,4,0,100,44,10,10,3,1)};
        EnemySpawnContext ctx;ctx.door_open=true;EnemySpawn dog;dog.id=4;dog.type=0;dog.flr_no=0;dog.px=100;dog.pz=50;dog.ay=0;dog.mode2=2;
        expect(!activate_enemy_spawn(dog,{0,0,0},0,areas,ctx),"type4 does not re-trigger a moving door");
    }
    if(failures){std::cerr<<failures<<" enemy spawn activation checks failed\n";return 1;}
    std::cout<<"Enemy spawn trap activation matches the original en04.c/zonzon.c.\n";return 0;
}

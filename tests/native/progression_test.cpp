#include "world.h"
#include "asset_io.h"
#include "binary_view.h"
#include "room.h"
#include "box_menu.h"
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static EventCommand command(unsigned op,std::initializer_list<std::uint8_t> operands){EventCommand c;c.opcode=op;c.bytes=operands;return c;}
static std::vector<std::uint8_t> event_fixture(const std::vector<std::uint8_t>& script){std::vector<std::uint8_t> b(1600);auto put=[&](unsigned p,unsigned n){for(unsigned i=0;i<4;++i)b[p+i]=std::uint8_t(n>>(i*8));};float version=1.73f;std::memcpy(b.data(),&version,4);put(16,64);put(28,1100);put(64+0x38,1400);put(1100,12);put(1104,14);put(1108,80);std::copy(script.begin(),script.end(),b.begin()+1180);return b;}
int main(int argc,char** argv){try{
    _Make_SinTable();
    // Scene composition must reuse decoded pixels and mip data; edits detach.
    Scene scene;scene.background=0xff202020;scene.textures={{2,2,{0xff404040,0xff404040,0xff404040,0xff404040}}};
    SceneTriangle triangle;triangle.vertices={SceneVertex{{-4,-4,2},0,0},SceneVertex{{4,-4,2},1,0},SceneVertex{{0,4,2},.5f,1}};triangle.color=0xffffffff;triangle.world_lit=true;triangle.textured=true;scene.triangles={triangle};
    auto frame=scene;const auto& source=scene.textures[0];check(static_cast<const SceneTexture&>(frame.textures[0]).pixels.data()==source.pixels.data(),"frame copies duplicate texture pixels");
    SoftwareRenderer renderer(32,32);renderer.set_supersampling(1);NJS_MATRIX view;njUnitMatrix(&view);renderer.draw_view(frame,view,16,16);auto cache=source.filter_cache;
    check(cache->ready&&frame.textures[0].filter_cache==cache,"mip data not shared back to resource");renderer.draw_view(scene,view,16,16);check(source.filter_cache==cache,"unchanged resource rebuilds mips");
    frame.textures[0].pixels[0]=0xff808080;check(source.pixels[0]==0xff404040,"mod edit corrupts another frame");renderer.draw_view(frame,view,16,16);check(frame.textures[0].filter_cache!=cache&&source.filter_cache==cache,"edited resource invalidates another texture cache");
    {SceneTexture editable{8,8,std::vector<std::uint32_t>(64,0xff404040)};scene.textures[0]=editable;renderer.draw_view(scene,view,16,16);const auto old=scene.textures[0].filter_cache;scene.textures[0].pixels[23]=0xffaaaaaa;renderer.draw_view(scene,view,16,16);check(scene.textures[0].filter_cache!=old,"unsampled in-place edit leaves stale mip cache");scene.textures[0]={2,2,{0xff404040,0xff404040,0xff404040,0xff404040}};}
    // Additive sprites blend without writing depth, and respect opaque occluders.
    scene.triangles[0].blend=SceneBlend::Additive;scene.triangles[0].depth_write=false;
    renderer.draw_view(scene,view,16,16);check(renderer.pixels()[16*32+16]==0xff606060&&renderer.covered_pixels()==0,"additive sprite blend or depth write incorrect");
    {auto quad=scene;auto a=quad.triangles[0],b=a;a.vertices={SceneVertex{{-4,-4,2},0,0},SceneVertex{{4,-4,2},1,0},SceneVertex{{4,4,2},1,1}};b.vertices={a.vertices[0],a.vertices[2],SceneVertex{{-4,4,2},0,1}};quad.triangles={a,b};renderer.draw_view(quad,view,16,16);check(std::all_of(renderer.pixels().begin(),renderer.pixels().end(),[](auto pixel){return pixel==0xff606060;}),"additive quad has a doubled or missing shared diagonal");}
    auto occluder=triangle;for(auto& v:occluder.vertices)v.position.z=1;occluder.textured=false;occluder.color=0xff101010;scene.triangles.insert(scene.triangles.begin(),occluder);renderer.draw_view(scene,view,16,16);check(renderer.pixels()[16*32+16]==0xff101010,"sprite appears through an opaque wall");
    // PORTIERUNGSLÜCKE (Policy 2026-10-09): Hier wurden die originalen
    // Effektprozeduren geprüft (bhEff218 Bodenflamme mit Atlas 77 und
    // Bodenanker, bhEff201 ruhender bzw. über mode1 ausgelöster Funkenflug,
    // Atlasvorschub je Tick und das Abschalten über flg 0x1000000). Sie kamen
    // aus effect_sprites.inc; Generator und Include sind Quarantäne, siehe
    // native/effect_sprites.cpp. Eff009 ist separat positiv portiert/geprüft;
    // für die hier verbleibenden Effekt-IDs ist weiterhin keine
    // Prozedur gebunden ist, nichts in die Zeichenliste eingereiht wird und
    // die Effektarbeit unverändert bleibt - kein geratener Atlas, kein
    // geratener Zustand. 122/218/230 sind seit dem committeten Stand
    // (2a488f7) positiv portiert und laufen nicht mehr als Lücke mit.
    // 200/201/232 sind seit 2026-10-14 von Hand portiert und laufen nicht mehr als Luecke mit.
    // 24 und 221 sind portiert (Gruppen 4 und 6, 2026-10-14); 229 (Muendungsfeuer der
    // Doppelwaffe, effsub5.c) ebenfalls. Nur noch wirklich ungebundene Kennungen laufen hier.
    check(supported_effect_sprite(229u),"229 (Doppelwaffe-Muendungsfeuer) meldet sich ungebunden");
    // 21 (bhEff021) ist seit 2026-10-17 portiert (Prolog RM_0000), 26 (bhEff026) seit 2026-10-19;
    // 6 (bhEff006) ist seit 2026-10-20 portiert; 367 (bhEff_Sub367, effsub4.c:2007) bleibt ungebunden,
    // weil im Original niemand exp0 (EFFSUB367_WORK) und op->func setzt (effect.c:37 bhJumpEffect4[17]).
    check(supported_effect_sprite(26u),"26 (bhEff026) meldet sich ungebunden");
    for(const unsigned id:{367u}){EffectSprite sprite;sprite.id=id;sprite.type=14;sprite.mode1=1;bool queued=true;
        check(!supported_effect_sprite(id),"effect gap reports a bound procedure");
        check(!step_effect_sprite(sprite,&queued)&&!queued,"effect gap executed a procedure or queued a draw");
        check(sprite.tex_id==-1&&!sprite.ct0&&!sprite.ani_ct&&!sprite.flg&&sprite.type==14&&sprite.tv[0].y==-1,"effect gap changed the sprite work");}
    set_explosion({0,0,0},{0,0,-1});set_explosion_effect({0,0,0},{0,0,-1});
    {const auto out=take_effect_output();
     check(out.spawns.size()==7&&out.sounds==std::vector<unsigned>{534u}&&out.bombs.empty()&&out.light.has_value(),
           "bhSetExplosion/bhSetExplosionEffect requests differ from weapon.c:1706/1812");}
    // PCM loop boundaries preserve the intro and loop subsection, not the outro.
    AudioMixer audio;auto pcm=std::make_shared<PcmClip>();pcm->rate=48000;pcm->channels=1;pcm->samples={100,200,300,400,500};pcm->loop_start=1;pcm->loop_end=4;audio.play_pcm(0,pcm,1,true);const auto sound=audio.mix(8);const int expected[]={100,200,300,400,200,300,400,200};for(unsigned i=0;i<8;++i)check(sound[i*2]==expected[i],"PCM loop loses intro or uses whole file");
    // Item checks include empty slots and inspected-ID state, as event.c does.
    for(unsigned id:{0u,72u,5u}){auto bytes=event_fixture({1,10,0x11,std::uint8_t(id),5,4,12,0,0,0,3,0,0xff,0});Inventory inv;inv.slots()[2]=0x80480001;MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.start(0,0);vm.tick();check(vm.errors.empty()&&vm.flag(4,12)==(id!=5),"original empty/inspected item condition missing");check(bool(vm.flags[4][0]&0x10000)==(id==72),"inspected item state written to wrong work flags");}
    for(bool bag:{false,true})for(bool wide:{false,true})for(unsigned mode=0;mode<6;++mode){Inventory inv;inv.side_pack=bag;std::fill(inv.slots()+2,inv.slots()+12,0);if(wide)inv.slots()[2]=Inventory::pack(1,1);const unsigned count=inv.capacity()-(wide?2:0),operand=7;const bool expected[]={count==operand,count>operand,count>=operand,count<operand,count<=operand,count!=operand};auto bytes=event_fixture({1,12,0x4d,std::uint8_t(mode),std::uint8_t(operand),0xa5,5,4,12,0,0,0,3,0,0xff,0});MessageDialog dialog;EventVm vm(bytes,inv,dialog);vm.start(0,0);vm.tick();check(vm.errors.empty()&&vm.flag(4,12)==expected[mode],"inventory block comparison/width or instruction padding differs");}
    {Inventory inv;inv.player=1;inv.slots()[0]=3;inv.slots()[1]=Inventory::pack(18,1);inv.slots()[2]=Inventory::pack(5,18);inv.slots()[3]=0x80480001;const auto personal=inv.slots()[1];inv.items[200]=123;MessageDialog dialog;auto bytes=event_fixture({0xb7,1,0xff,0});EventVm vm(bytes,inv,dialog);vm.start(0,0);vm.tick();check(vm.errors.empty()&&inv.slots()[1]==personal&&inv.items[192]==Inventory::pack(5,18)&&inv.items[193]==0x80480001&&inv.items[200]==0&&inv.slots()[2]==0,"security deposit loses player selection/personal item/raw item state");auto transfer=event_fixture({0xb8,0,0xff,0});EventVm box(transfer,inv,dialog);box.start(0,0);box.tick();check(box.errors.empty()&&inv.items[64]==Inventory::pack(5,18)&&inv.items[65]==0x80480001&&!inv.items[192],"security-to-item-box transfer wrong range/order");}
    {Inventory inv;std::fill(inv.items.begin()+64,inv.items.begin()+192,Inventory::pack(20,1));inv.items[70]=0;inv.items[192]=Inventory::pack(5,18);inv.items[193]=Inventory::pack(12,30);const auto saved=inv.items;MessageDialog dialog;auto bytes=event_fixture({0xb8,0,0xff,0});EventVm vm(bytes,inv,dialog);vm.start(0,0);vm.tick();check(!vm.errors.empty()&&inv.items==saved,"overflow transfer partially loses security inventory");}
    // D02 ItemBoxChange (sub1.c; tragende Zweige nach Disassembly, siehe
    // docs/INVENTORY_CONTRACT.md): Einlagern/Entnehmen, Lösen der
    // Ausrüstung, Restriktionen 150/151/200, Stapeln über Combi_01, Kapazität
    // 159, pip[0]-Nachführung und Sonderslot. Abschnitte einzeln gesammelt.
    std::vector<std::string> failures;
    auto section=[&](const char* name,const std::function<void()>& body){try{body();}catch(const std::exception& e){failures.push_back(std::string(name)+": "+e.what());}};
    auto pocket=[]{Inventory inv;std::fill(inv.slots(),inv.slots()+12,0);return inv;};
    section("D02 ItemBoxChange Sicherheitsbox",[&]{
        auto inv=pocket();inv.slots()[2]=Inventory::pack(5,18);inv.slots()[0]=2;unsigned message=0;
        check(inv.box_change(2,0,192,0,9,&message)&&inv.items[192]==Inventory::pack(5,18)&&inv.slots()[0]==0&&!inv.contains(5),"security deposit/equipment not updated by original routine");
        const auto change=inv.take_weapon_change();check(change.changed&&change.wpnr_no==0,"depositing the equipped gun did not clear plp->wpnr_no");
        check(inv.box_change(2,0,192,0,9,&message)&&inv.contains(5)&&!inv.items[192],"security withdrawal failed");
        inv.slots()[2]=Inventory::pack(145,1);const auto saved=inv.items;message=0;check(!inv.box_change(2,1,192,0,9,&message)&&message==200&&inv.items==saved,"nonmetal item accepted by security box (200)");});
    section("D02 ItemBoxChange Restriktionen 150/151",[&]{
        {auto inv=pocket();inv.slots()[2]=Inventory::pack(105,1);const auto saved=inv.items;unsigned message=0;check(!inv.box_change(2,1,64,0,0,&message)&&message==150&&inv.items==saved,"original key restriction 150 lost");}
        {auto inv=pocket();inv.slots()[2]=Inventory::pack(95,1);inv.items[64]=Inventory::pack(84,1);const auto saved=inv.items;unsigned message=0;check(!inv.box_change(2,0,64,9,26,&message)&&message==151&&inv.items==saved,"original 926 restriction 151 lost");}
        {auto inv=pocket();inv.player=1;inv.slots()[2]=Inventory::pack(105,1);unsigned message=0;check(inv.box_change(2,1,64,0,0,&message)&&!message&&inv.items[65]==Inventory::pack(105,1),"restrictions must only apply to ply_id 0");}});
    section("D02 ItemBoxChange Stapeln",[&]{
        {auto inv=pocket();inv.slots()[2]=Inventory::pack(12,30);inv.items[64]=Inventory::pack(12,25);unsigned message=0;check(inv.box_change(2,0,64,0,0,&message)&&Inventory::quantity(inv.slots()[2])==55&&!inv.items[64],"original ammo stacking in box missing");}
        {auto inv=pocket();inv.slots()[2]=Inventory::pack(12,990);inv.items[64]=Inventory::pack(12,25);unsigned message=0;check(inv.box_change(2,0,64,0,0,&message)&&inv.slots()[2]==Inventory::pack(12,999)&&inv.items[64]==Inventory::pack(12,16),"Combi_01 999 remainder must stay in the box");}
        // Leerer Cursor bei vorhandenem Stapel: chk2 2 mit num1 = Stapelslot,
        // aber Combi_01 sucht in cmb(id2) eine Regel mit combidata == id1 = 0;
        // keine existiert, also geschieht nichts (auch keine Meldung 159).
        {auto inv=pocket();inv.slots()[2]=Inventory::pack(95,1);inv.slots()[3]=Inventory::pack(12,10);inv.items[64]=Inventory::pack(12,25);const auto saved=inv.items;unsigned message=0;inv.box_change(4,0,64,0,0,&message);
            check(inv.items==saved&&!message,"empty cursor with a matching pocket stack must leave both unchanged (no rule for id1 0)");}});
    section("D02 ItemBoxChange Tausch und Ausrüstung",[&]{
        {auto inv=pocket();inv.slots()[2]=Inventory::pack(95,1);inv.items[64]=Inventory::pack(21,1);unsigned message=0;check(inv.box_change(2,0,64,0,0,&message)&&inv.slots()[2]==Inventory::pack(21,1)&&inv.items[64]==Inventory::pack(95,1),"narrow swap differs");}
        {auto inv=pocket();inv.slots()[2]=Inventory::pack(5,18);inv.slots()[3]=Inventory::pack(95,1);inv.slots()[0]=2;inv.items[64]=Inventory::pack(21,1);unsigned message=0;
            check(inv.box_change(2,0,64,0,0,&message)&&inv.items[64]==Inventory::pack(5,18)&&inv.slots()[0]==0&&inv.contains(21),"swapping out the equipped gun must clear pip[0]");}
        // Item vor der ausgerüsteten Waffe einlagern: pip[0] sinkt, ItemSort.
        {auto inv=pocket();inv.slots()[2]=Inventory::pack(95,1);inv.slots()[3]=Inventory::pack(21,1);inv.slots()[4]=Inventory::pack(5,18);inv.slots()[0]=4;inv.wpnr_no=3;unsigned message=0;
            // ItemBoxChange sortiert nicht (Disassembly 0x002A53E0); die Lücke bleibt bis SpriteOnOff (sub1.c:8296).
            check(inv.box_change(2,0,64,0,0,&message)&&inv.items[64]==Inventory::pack(95,1)&&!inv.slots()[2]&&inv.slots()[0]==3,"deposit before the equipped gun must lower pip[0] without sorting");
            inv.sort();check(inv.slots()[2]==Inventory::pack(21,1)&&inv.slots()[3]==Inventory::pack(5,18)&&inv.slots()[0]==3&&inv.weapon()==3,"deposit before the equipped gun must lower pip[0] and sort");}});
    section("D02 ItemBoxChange Kapazität (breit) 159",[&]{
        {auto inv=pocket();for(unsigned i=2;i<10;++i)inv.slots()[i]=Inventory::pack(95,1);inv.items[64]=Inventory::pack(3,7);const auto saved=inv.items;unsigned message=0;
            check(!inv.box_change(2,0,64,0,0,&message)&&message==159&&inv.items==saved,"wide box item into a full pocket must answer 159");}
        {auto inv=pocket();for(unsigned i=2;i<9;++i)inv.slots()[i]=Inventory::pack(95,1);inv.items[64]=Inventory::pack(3,7);unsigned message=0;
            check(inv.box_change(2,0,64,0,0,&message)&&inv.slots()[2]==Inventory::pack(3,7)&&inv.items[64]==Inventory::pack(95,1)&&inv.occupied()==8,"wide box item with one free unit must swap and sort first");}});
    section("D02 ItemBoxChange Sonderslot pip[1]",[&]{
        // Grundwaffe aus der Kiste in den leeren Sonderslot: normaler Tausch.
        {auto inv=pocket();inv.items[64]=Inventory::pack(55,1);unsigned message=0;check(inv.box_change(1,0,64,0,0,&message)&&inv.slots()[1]==Inventory::pack(55,1)&&!inv.items[64],"standard weapon into pip[1] differs");}
        // Keine Grundwaffe am Sonderslot (Disassembly 0x2A57A8/0x2A592C): der
        // Boxeintrag landet in pip[max+1] (kein Wert 2), pip[1] wird 0, die Kiste
        // erhält das Messer; ItemSort holt das Item nach vorn.
        {auto inv=pocket();inv.slots()[1]=Inventory::pack(55,1);inv.slots()[2]=Inventory::pack(95,1);inv.slots()[3]=Inventory::pack(95,1);inv.items[64]=Inventory::pack(21,1);unsigned message=0;
            check(inv.box_change(1,0,64,0,0,&message)&&inv.items[64]==Inventory::pack(55,1)&&!inv.slots()[1]&&inv.slots()[9]==Inventory::pack(21,1)&&!inv.slots()[4],"non-standard box item at pip[1] must go to pip[max+1] (no ItemSort in ItemBoxChange)");
            inv.sort();check(inv.slots()[4]==Inventory::pack(21,1)&&!inv.slots()[9]&&inv.occupied()==3,"non-standard box item at pip[1] must go to pip[max+1] and sort in");}
        // Grundwaffe aus der Kiste auf einen Taschenslot (Disassembly 0x2A5834:
        // chk2 bleibt 1, lt_c3 = lt_c, lt_c2 = 1, $v1 = 0): die Kiste erhält das
        // Taschenitem, das Messer geht nach pip[1], der Taschenslot wird leer.
        // Die Decomp setzt hier fälschlich chk2 = 0 (Ablehnung).
        {auto inv=pocket();inv.slots()[2]=Inventory::pack(145,1);inv.slots()[3]=Inventory::pack(5,1);inv.items[224]=Inventory::pack(55,1);unsigned message=0;
            check(inv.box_change(3,0,224,0,9,&message)&&!message&&inv.items[224]==Inventory::pack(5,1)&&inv.slots()[1]==Inventory::pack(55,1)&&!inv.contains(5),"standard weapon from the box onto a pocket slot must swap via pip[1] (0x2A5834)");}});
    // Modaler Sicherheitsbox-Weg: Einfahren sperrt die Eingabe, danach lagert
    // Bestätigen über ItemBoxChange ein; Schließen fährt aus und gibt die
    // Skript-/Menüflags frei.
    section("D02 ItemBoxChange über das modale Boxmenü",[&]{
        Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);inv.slots()[2]=Inventory::pack(5,18);MessageDialog dialog;auto bytes=event_fixture({0xff,0});EventVm vm(bytes,inv,dialog);StatusMenu menu;menu.active=true;menu.stage=0;menu.room=9;vm.flags[10][0]|=0x80000;BoxMenu box;box.synchronize(inv,menu,&vm);check(box.active()&&box.base()==192&&box.moving(),"security script did not enter modal box");box.input(MenuInput::Confirm,inv,menu,&vm,audio);check(!inv.items[192],"pad accepted while the box slides in");for(int i=0;i<9&&box.moving();++i)box.synchronize(inv,menu,&vm);check(!box.moving(),"box slide-in did not finish after nine ticks");box.input(MenuInput::Confirm,inv,menu,&vm,audio);box.input(MenuInput::Confirm,inv,menu,&vm,audio);
        check(inv.items[192]==Inventory::pack(5,18),"modal security confirm did not deposit");
        box.input(MenuInput::Cancel,inv,menu,&vm,audio);check(box.active()&&box.moving()&&menu.active&&!(vm.flags[12][0]&0x200),"security close skipped the slide-out");for(int i=0;i<9;++i)box.synchronize(inv,menu,&vm);check(!box.active()&&!menu.active&&!(vm.flags[10][0]&0x1c0000)&&vm.flags[12][0]&0x200,"security close leaves original script/menu flags stuck");});
    // ItemBoxChange listcsr_0 (ELF 0x2A5C5C-0x2A5D40, sub1.c:7984-7994): bei Breitenwechsel
    // bleibt listcsr_2 stehen; nur der Slot wird hinter sakai bzw. auf dessen letzter ungerader Zelle angepasst.
    section("ItemBox Breitenwechsel-Cursor",[&]{
        for(bool bag:{false,true})for(bool incoming_wide:{false,true}){
            auto inv=pocket();inv.side_pack=bag;inv.slots()[1]=Inventory::pack(55,1);
            if(incoming_wide){inv.slots()[2]=Inventory::pack(95,1);inv.slots()[3]=Inventory::pack(21,1);inv.slots()[4]=Inventory::pack(22,1);inv.items[64]=Inventory::pack(3,7);}
            else{inv.slots()[2]=Inventory::pack(3,7);inv.slots()[3]=Inventory::pack(95,1);inv.items[64]=Inventory::pack(21,1);}
            MessageDialog dialog;auto bytes=event_fixture({0xff,0});EventVm vm(bytes,inv,dialog);StatusMenu menu;menu.active=true;vm.flags[10][0]|=0x40000;
            BoxMenu box;box.synchronize(inv,menu,&vm);for(int i=0;i<9&&box.moving();++i)box.synchronize(inv,menu,&vm);
            if(incoming_wide){box.input(MenuInput::Right,inv,menu,&vm,audio);box.input(MenuInput::Right,inv,menu,&vm,audio);}
            else{box.input(MenuInput::Right,inv,menu,&vm,audio);box.input(MenuInput::Left,inv,menu,&vm,audio);} // von Slot 3/Zelle 4 zur rechten Haelfte des breiten Items: Slot 2/Zelle 3
            check(menu.selected==(incoming_wide?4u:2u)&&box.cursor_cell()==(incoming_wide?4u:3u),"cursor fixture did not reach its original cell");
            const auto cell=box.cursor_cell();
            box.input(MenuInput::Confirm,inv,menu,&vm,audio);box.input(MenuInput::Confirm,inv,menu,&vm,audio);
            check(menu.selected==3&&box.cursor_cell()==cell,"width swap must correct listcsr_0 only, retaining listcsr_2");
            menu.sprite_on_off(inv); // ItemSort desselben Statusframes, nicht Teil von ItemBoxChange
            check(menu.selected==3&&box.cursor_cell()==cell&&Inventory::id(inv.slots()[3])==95,"ItemSort changed the box cursor or failed to retain its cell");
        }
    });
    // sub1.c ItemBox testmode 2 (ItemBoxIconSet's persistent parts_20b pos[1]
    // mutation and the tail at 7688-7751): a one-row request displaces every row
    // by `number` (+-19) per frame for 38/|number| frames; the direction flag
    // adds the full 38 px in the frame that advances the box index
    // (ips1/ipsdisp), so the accumulated shift returns to zero. The five-entry
    // shoulder scroll keeps the original keywait>10 / boxwait>3 repeat counters.
    section("ItemBox Listen-Scroll (testmode 2)",[&]{
        Inventory inv;std::fill(inv.items.begin()+64,inv.items.begin()+192,0);MessageDialog dialog;auto bytes=event_fixture({0xff,0});EventVm vm(bytes,inv,dialog);StatusMenu menu;menu.active=true;menu.stage=0;menu.room=9;vm.flags[10][0]|=0x40000;BoxMenu box;box.synchronize(inv,menu,&vm);
        for(int i=0;i<9&&box.moving();++i)box.synchronize(inv,menu,&vm);
        check(box.active()&&box.base()==64&&!box.moving()&&box.box_index()==0,"item box list did not settle at ips1 0");
        box.tick(0x800,0x800,inv,menu,&vm,audio);   // testmode 2
        box.tick(0x2,0x2,inv,menu,&vm,audio);
        check(box.arrows()==2,"down request must light parts_15b[21] (anim 19)");
        check(box.row_shift()==-19.f&&box.box_index()==0,"request frame must displace the rows by -19 in the same ItemBox pass");
        box.tick(0,0,inv,menu,&vm,audio);check(box.row_shift()==-38.f&&box.box_index()==0,"second scroll frame must displace the rows by -38");
        box.tick(0,0,inv,menu,&vm,audio);check(box.row_shift()==0.f&&box.box_index()==1&&box.arrows()==3,"direction frame must advance ips1, restore the rows and set both parts_15b arrows to the neutral 18");
        // sub1.c:7606: Confirm/Cancel stehen hinter `st->wait == 0`; waehrend der
        // laufenden Scroll-Animation wird keine der beiden Tasten verarbeitet.
        {Inventory gate;std::fill(gate.items.begin()+64,gate.items.begin()+192,0);gate.items[64]=Inventory::pack(12,3);gate.items[65]=Inventory::pack(12,5);gate.slots()[2]=Inventory::pack(12,10);
         MessageDialog d;auto b=event_fixture({0xff,0});EventVm v(b,gate,d);StatusMenu m;m.active=true;m.selected=2;m.stage=0;m.room=9;v.flags[10][0]|=0x40000;BoxMenu bo;bo.synchronize(gate,m,&v);
         for(int i=0;i<9&&bo.moving();++i)bo.synchronize(gate,m,&v);
         bo.tick(0x800,0x800,gate,m,&v,audio);       // testmode 2
         bo.tick(0x2,0x2,gate,m,&v,audio);           // Tail: wait++ = 1, Scroll laeuft
         bo.tick(0x800,0x800,gate,m,&v,audio);
         check(gate.items[65]==Inventory::pack(12,5)&&gate.slots()[2]==Inventory::pack(12,10),"ItemBox 7606: confirm during the pending scroll must be ignored");
         bo.tick(0,0,gate,m,&v,audio);
         bo.tick(0x800,0x800,gate,m,&v,audio);
         check(gate.slots()[2]==Inventory::pack(12,15)&&!gate.items[65]&&gate.items[64]==Inventory::pack(12,3),"ItemBox deposit after the settled scroll uses the advanced box entry (ips1 1), not entry 0");}
        box.tick(0x1,0x1,inv,menu,&vm,audio);
        check(box.arrows()==1,"up request must light parts_15b[20] (anim 19)");
        for(int i=0;i<2;++i)box.tick(0,0,inv,menu,&vm,audio);
        check(box.box_index()==0&&box.row_shift()==0.f,"up request must return to the previous box entry");
        box.pad(0,0,inv,menu,&vm,audio);
        box.pad(0x10,0x10,inv,menu,&vm,audio);check(box.box_index()==5,"first five-entry scroll must step immediately (keywait 1)");
        box.pad(0x10,0,inv,menu,&vm,audio);check(box.box_index()==5,"second held frame must only count keywait");
        for(int i=0;i<8;++i)box.pad(0x10,0,inv,menu,&vm,audio);
        check(box.box_index()==5,"keywait<=10 must not step again");
        box.pad(0x10,0,inv,menu,&vm,audio);
        check(box.box_index()==5,"keywait==11 alone must not step");
        box.pad(0x10,0,inv,menu,&vm,audio);
        check(box.box_index()==10,"boxwait>3 must step five more entries after the initial hold delay");
        for(int i=0;i<3;++i)box.pad(0x10,0,inv,menu,&vm,audio);
        check(box.box_index()==10,"boxwait 1..3 must not step yet");
        box.pad(0x10,0,inv,menu,&vm,audio);
        check(box.box_index()==15,"boxwait>3 must repeat every four held frames");
        for(const unsigned direction:{0x8u,0x4u})for(int i=0;i<3;++i){
            box.pad(0,0,inv,menu,&vm,audio);
            box.pad(direction,direction,inv,menu,&vm,audio);
            check(box.box_index()==15,"horizontal D-pad must not act as an ItemBox shoulder");
        }
        box.pad(0,0,inv,menu,&vm,audio);
        box.pad(0x80,0x80,inv,menu,&vm,audio);
        check(box.box_index()==10,"original left shoulder must jump five entries");
        box.pad(0x100,0x100,inv,menu,&vm,audio);
        box.pad(0x180,0x80,inv,menu,&vm,audio);
        check(box.box_index()==10,"another held pad bit must prevent the full-release timer reset");
        box.pad(0,0,inv,menu,&vm,audio);
        box.pad(0x80,0x80,inv,menu,&vm,audio);
        check(box.box_index()==5,"a fully released pad must restore the immediate shoulder jump");
    });
    section("ItemBox SpriteH/Pad/EXIT Reihenfolge",[&]{
        Inventory inv;MessageDialog dialog;auto bytes=event_fixture({0xff,0});EventVm vm(bytes,inv,dialog);
        StatusMenu menu;menu.active=true;vm.flags[10][0]|=0x40000;BoxMenu box;
        box.tick(0,0,inv,menu,&vm,audio);
        for(int i=0;i<8;++i)box.tick(0,0,inv,menu,&vm,audio);
        check(box.moving(),"SpriteH must keep the pad locked before the ninth tick");
        box.tick(0x800,0x800,inv,menu,&vm,audio);
        check(!box.moving(),"SpriteH must unlock before ItemBox reads the ninth tick's pad");
        box.tick(0x2,0x2,inv,menu,&vm,audio);
        check(box.row_shift()==-19.f,"confirm on the unlock frame must have entered testmode 2");
        box.tick(0x800,0x800,inv,menu,&vm,audio);
        check(box.row_shift()==-38.f,"confirm must be ignored before the scroll tail settles");
        box.tick(0x1000,0x1000,inv,menu,&vm,audio);
        check(box.box_index()==1&&box.arrows()==3,"cancel on the index-advance frame must still be ignored");
        box.tick(0x1000,0x1000,inv,menu,&vm,audio);
        check(box.arrows()==0&&!box.moving(),"cancel after the scroll must return to testmode 1");
        box.tick(0x1,0x1,inv,menu,&vm,audio); // testmode 3
        box.tick(0x800,0x800,inv,menu,&vm,audio); // wait 4 -> 3 in this frame
        for(int i=0;i<3;++i)box.tick(0,0,inv,menu,&vm,audio);
        check(!box.moving(),"EXIT must not start StatusCancel while its press counter is nonzero");
        box.tick(0,0,inv,menu,&vm,audio);
        check(box.moving(),"EXIT must start StatusCancel in the frame after wait reaches zero");
        for(int i=0;i<9;++i)box.tick(0x800,0x800,inv,menu,&vm,audio);
        check(!box.active()&&!menu.active,"closing SpriteH must release without processing the held confirm");
        menu.active=true;vm.flags[10][0]|=0x40000;box.tick(0,0,inv,menu,&vm,audio);
        for(int i=0;i<9;++i)box.tick(0,0,inv,menu,&vm,audio);
        box.tick(0x1,0x1,inv,menu,&vm,audio);box.tick(0x800,0x800,inv,menu,&vm,audio);
        box.tick(0x2,0x2,inv,menu,&vm,audio);
        for(int i=0;i<6;++i)box.tick(0,0,inv,menu,&vm,audio);
        check(!box.moving()&&menu.selected==2,"Down during a pressed EXIT must cancel the press and restore the item cursor");
    });
    section("ItemBox unabhängige Cursorzelle",[&]{
        Inventory inv;inv.slots()[2]=Inventory::pack(1,1); // original wide-item type 0x100
        MessageDialog dialog;auto bytes=event_fixture({0xff,0});EventVm vm(bytes,inv,dialog);
        StatusMenu menu;menu.active=true;vm.flags[10][0]|=0x40000;BoxMenu box;
        box.tick(0,0,inv,menu,&vm,audio);for(int i=0;i<9;++i)box.tick(0,0,inv,menu,&vm,audio);
        check(menu.selected==2&&box.cursor_cell()==2,"CursorInit must start on the left half of the first wide item");
        box.tick(8,8,inv,menu,&vm,audio);
        check(menu.selected==3&&box.cursor_cell()==4,"Right from an even wide-item cell must add two cells and one pip index");
        box.tick(0,0,inv,menu,&vm,audio);box.tick(4,4,inv,menu,&vm,audio);
        check(menu.selected==2&&box.cursor_cell()==3,"Left from sakai must select the wide item on its right half, not reconstruct cell 2");
        box.tick(0,0,inv,menu,&vm,audio);box.tick(2,2,inv,menu,&vm,audio);
        check(menu.selected==4&&box.cursor_cell()==5,"Down from the right half before sakai must add two pip indices and two cells");
    });
    // PORTIERT (Policy 2026-10-09, Auftrag C01): Die Spawn-/Fallenbereiche
    // (bhEne_EnemyAtariCheck, bhEne04_MVType01) liefen aus enemy_spawn.inc als
    // Quarantäne; jetzt bestimmt native/enemies/enemy_spawn.cpp sie. Ein
    // wartender Hund (type 1) erwacht, wenn der Spieler auf seiner Etage in
    // einem prm1=1-Bereich steht UND der Hund selbst in einem prm1=2-Bereich
    // seiner Etage mit gleichem prm3. Etagenwahl und einschließende Kanten sind
    // damit belegt: (5,5) und (10,010) innen, (35,5) außerhalb des prm1=1-
    // Bereichs, Etage 1 trifft die Etage-0-Bereiche nicht. Der Hund wird type 0,
    // ohne Platzierung, Zufallszug, Raumflag oder Bereichsverbrauch.
    {std::vector<EnemyArea> areas{{1,2,0,0,0,10,10,4,1,0,2},{1,2,0,30,0,10,10,4,2,0,2}};const auto saved_areas=areas;
     for(const int floor:{0,1})for(const auto at:{NJS_POINT3{5,0,5},NJS_POINT3{10,0,10},NJS_POINT3{35,0,5}}){
        const bool inside=floor==0&&(at.x<=10&&at.z<=10);
        EnemySpawn enemy;enemy.id=4;enemy.type=1;enemy.px=35;enemy.pz=5;EnemySpawnContext context;
        // Der Zufallszug wird gezaehlt: context.rand ist seit fd488af ein
        // std::function (geteilter Strom), kein Fortschrittszaehler. Der
        // Typ-1-Pfad ruft rand() nie, also muss der Zaehler auf null bleiben.
        int draws=0;context.rand=[&draws]{++draws;return 1;};
        check(activate_enemy_spawn(enemy,at,floor,areas,context)==inside,"type1 activation follows the player area and floor");
        check(enemy.type==(inside?0:1)&&enemy.px==35&&enemy.pz==5,"type1 activation sets type 0 without repositioning");
        check(!context.contact&&!context.areas&&!context.room_flags&&draws==0,"type1 activation touched the context or random stream");}
     check(areas[0].prm3==saved_areas[0].prm3&&areas[1].prm3==saved_areas[1].prm3,"type1 activation consumed a one-use trap area");}
    // A v3 ADX loop header is before the CRI marker and data offset.
    {std::vector<std::uint8_t> adx(50+18);adx[0]=0x80;adx[3]=46;adx[4]=3;adx[5]=18;adx[6]=4;adx[7]=1;adx[10]=0xbb;adx[11]=0x80;adx[15]=32;adx[18]=3;adx[27]=1;adx[31]=4;adx[39]=28;std::copy_n("(c)CRI",6,adx.begin()+44);auto decoded=decode_adx(adx);check(decoded.loop_start==4&&decoded.loop_end==28,"ADX v3 loop marker/ranges not parsed");adx[39]=40;bool rejected=false;try{decode_adx(adx);}catch(const std::exception&){rejected=true;}check(rejected,"ADX loop outside waveform accepted");}
    if(argc>1){const auto data=std::filesystem::u8path(argv[1]);
        // ItemBoxIconSet leaves the parts_20b pos[1] values where the scroll put
        // them; the draw must follow that mutation (rows move with it).
        {GameUi ui(data);Inventory inv;for(unsigned i=0;i<7;++i)inv.items[64+i]=Inventory::pack(5+i,1);StatusMenu menu;menu.active=true;menu.selected=1;SoftwareRenderer plain(640,480),shifted(640,480);
            ui.draw_box(plain,inv,menu,64,3,false);ui.draw_box(shifted,inv,menu,64,3,false,nullptr,0,0,-19.f,3);
            check(plain.pixels()!=shifted.pixels(),"parts_20b row shift must move the drawn box rows");}
        // Door PCM remains alive across the room sound-bank reset. sdMidiPlay
        // (ps2_sg_sd.c:510) stops the port MidiHandle[2] still names when it
        // sounds, then rotates 2 -> 8 -> 9: three immediate door triggers leave
        // only the last one sounding, on a voice the earlier ones did not use.
        {AudioMixer door_audio(data);door_audio.load_door_sounds(7);std::vector<unsigned> voices;
         for(unsigned program:{1u,0u,2u}){check(door_audio.play_door_sound(program),"original opening/closing door program unavailable");voices.push_back(door_audio.door_voice());}
         check(voices[0]!=voices[1]&&voices[1]!=voices[2]&&voices[0]!=voices[2],"door triggers did not rotate the handle's ports");
         for(unsigned slot=door_voice_first;slot<=door_voice_last;++slot)check(door_audio.active(slot)==(slot==voices[2]),"a stopped door port kept sounding or the current one is silent");
         door_audio.load_room_sounds(0,0,0);check(door_audio.active(voices[2]),"room sound reset cuts off door closing audio");}
        // Actual Steve cue has no loop; actual ambient BGM does.
        auto cue=decode_adx(read_asset_file(data/"archives/BGM1/0048.bin",32*1024*1024));check(!cue.loop_end,"Steve cue unexpectedly marked looping");AudioMixer music(data);music.play(0,"BGM1",48,1,true);for(unsigned i=0;i<3;++i)music.mix(480000);check(!music.active(0),"Steve cue loops after its natural end");auto ambient=decode_adx(read_asset_file(data/"archives/BGM1/0000.bin",32*1024*1024));check(ambient.loop_end>ambient.loop_start&&ambient.loop_end<=ambient.frames(),"original ambient loop points lost");
        for(const auto name:{"RM_0020","RM_0050","RM_0070","RM_0080","RM_0090"}){
            auto bytes=read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024);auto stage=decode_scene(bytes);auto player=stage.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer silent;RoomWorld world(bytes,stage,player,motion,frame,silent);BinaryView v(bytes);const auto h=inspect_room(bytes).room_offset,enep=v.u32(h+8);unsigned enemy=0;for(;enemy<v.u32(h+0x88);++enemy)if(v.u16(enep+enemy*36+4)==1)break;
            // Select original lower-room clip and verify that upper-body keys also run.
            const unsigned clip=std::string(name)=="RM_0020"?2:std::string(name)=="RM_0070"?1:0;
            check(world.bind(command(0x65,{0x65,1,std::uint8_t(enemy),0})),"original enemy work selection missing");check(world.bind(command(0x69,{0x69,24,1,0,std::uint8_t(clip),0,0,0})),"paired room motion unavailable");world.bind(command(0x69,{0x69,40,20,0}));const auto animated=world.model_pose(1,enemy,10);check(animated.angles!=std::array<int,3>{},"upper-body room motion remains in rest pose");
            world.bind(command(0x69,{0x69,36}));check(world.model_pose(1,enemy).position.x==0,"root control was ignored before motion update");world.tick();const auto refreshed=world.model_pose(1,enemy).position;check(refreshed.x!=0||refreshed.y!=0||refreshed.z!=0,"root override survives next keyed motion update");
            Scene composed;world.append(composed,{},stage.cameras[0]);check(!composed.triangles.empty(),"paired room motion discarded entire actor");
            // Lücke Effektprozeduren (siehe oben): ohne bhEff* reiht sich kein
            // Effekt in sys->ef_trs ein, also werden die Friedhofsflammen und
            // Funken nicht mehr zusammengesetzt. Die Raumfigur selbst wird
            // weiter gezeichnet (Zusicherung direkt darüber).
            if(std::string(name)=="RM_0020"){const auto flames=std::count_if(composed.triangles.begin(),composed.triangles.end(),[](const auto& t){return t.blend==SceneBlend::Additive;});check(flames==0,"effect gap composed additive effect geometry");}
            // Lücke bhEne_EnemyAtariCheck (siehe oben): die Wartezustände der
            // beiden Hunde kommen weiter aus den Originaldaten, aber das
            // Betreten der Bereiche schaltet keinen Hund scharf, platziert ihn
            // nicht und meldet nichts an das Raumskript.
            if(std::string(name)=="RM_0050"){SceneSpawn trap_player;std::size_t trap_motion=42;std::int64_t trap_frame=0;RoomWorld dogs(bytes,stage,trap_player,trap_motion,trap_frame,silent);check(dogs.state(1,0)->type==2&&dogs.state(1,1)->type==1,"original dogs not initialized in waiting types");
                for(const float x:{-142.f,-194.f}){trap_player.position={x,0,0};dogs.tick();}
                check(dogs.state(1,0)->type==2&&dogs.state(1,1)->type==1,"enemy spawn gap armed a dog trap");
                check(!(dogs.take_enemy_room_flags()&0x30000u),"enemy spawn gap notified the original room script");}
        }
        {GameUi ui(data);Inventory inv;inv.items[224]=Inventory::pack(5,1);inv.items[225]=Inventory::pack(55,1);StatusMenu menu;menu.active=true;menu.selected=1;SoftwareRenderer box(640,480);ui.draw_box(box,inv,menu,224,0,false);
            unsigned orange=0;for(unsigned y=190;y<216;++y)for(unsigned x=60;x<310;++x){const auto pixel=box.pixels()[y*640+x];const unsigned r=(pixel>>16)&255,g=(pixel>>8)&255,b=pixel&255;if(r>170&&g>70&&g<180&&b<70)++orange;}check(!orange,"disabled box cursor crosses item list");}
        {const auto bytes=read_asset_file(data/"rooms/RM_0090.RDT",64*1024*1024);auto stage=decode_scene(bytes);auto player=stage.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer sound(data);sound.load_room_sounds(0,9);RoomWorld world(bytes,stage,player,motion,frame,sound,data,{},"RM_0090");Inventory inventory;MessageDialog dialog;EventVm vm(bytes,inventory,dialog);
            vm.binding=[&](const auto& c){return world.bind(c);};vm.task_started=[&](unsigned level){world.start_task(level);};
            // Actual room scripts, not a copied detector rule: event 40 sets
            // the green lamp pair, event 41 swaps to the red atlas animation.
            vm.start(0,40);vm.tick();world.tick();check(vm.errors.empty()&&world.effect_state(60)->type==1&&world.effect_state(61)->type==0,"original green detector lamp script not bound");
            // Lücke Effektprozeduren (siehe oben): das Raumskript schaltet die
            // Lampentypen weiter, aber ohne bhEff122/bhEff230 reiht sich kein
            // Effekt in sys->ef_trs ein, also wird keine Lampenfläche
            // zusammengesetzt und auch kein Atlas gewählt.
            Scene green;world.append(green,{},stage.cameras[0]);const auto green_count=std::count_if(green.triangles.begin(),green.triangles.end(),[](const auto& t){return t.blend==SceneBlend::Additive;});check(green_count==0,"effect gap composed detector lamp geometry");
            // Das Raumskript setzt weiter den Lampentyp (Event-Bindung im
            // gemeinsamen World-Pfad). Atlaswahl (tex_id 414), der
            // Animationszähler ani_ct, der Phasenvorschub ct0 und das
            // Abschalten der grünen Lampe über flg 0x1000000 gehören dagegen
            // zu bhEff122/bhEff230 und finden nicht mehr statt.
            vm.start(0,41);vm.tick();world.tick();const auto lamp=world.effect_state(61);
            check(vm.errors.empty()&&lamp->type==3,"original red detector lamp script not bound");
            check(lamp->tex_id==-1&&!lamp->ani_ct&&!(world.effect_state(60)->flg&0x1000000),"effect gap chose an atlas, animated or switched off a lamp");
            const auto phase=lamp->ct0;world.tick();check(world.effect_state(61)->ct0==phase,"effect gap advanced the lamp animation phase");
            Scene red;world.append(red,{},stage.cameras[0]);check(std::none_of(red.triangles.begin(),red.triangles.end(),[](const auto& t){return t.blend==SceneBlend::Additive;}),"effect gap composed red lamp geometry");
            // Das Rücksetz-Skript stellt den grünen Lampentyp wieder her; das
            // Abschalten der roten Lampe über flg 0x1000000 gehört wieder zur
            // Effektprozedur und bleibt deshalb aus.
            vm.start(0,40);vm.tick();world.tick();check(world.effect_state(60)->type==1,"reset script does not restore the original green lamp type");
            check(!(world.effect_state(61)->flg&0x1000000),"effect gap switched off the alarm lamp");
            // The original shutter event drives both entity and local nodes,
            // plays movement list 0 then latch list 19, and restores its pose.
            sound.played.clear();vm.start(0,3);for(unsigned i=0;i<70;++i){vm.tick();world.tick();sound.mix();}
            check(vm.errors.empty()&&std::abs(world.state(2,6)->transform.position.y)<.02f,"original shutter nodes did not complete opening");
            check(std::find(sound.played.begin(),sound.played.end(),"MULTSPQ1/0013/2/seq0/prog0")!=sound.played.end()&&std::find(sound.played.begin(),sound.played.end(),"MULTSPQ1/0013/2/seq19/prog19")!=sound.played.end(),"original shutter movement/latch sound not requested");            vm.start(0,4);for(unsigned i=0;i<70;++i){vm.tick();world.tick();sound.mix();}check(vm.errors.empty()&&std::abs(world.state(2,15)->transform.position.y)<.02f,"second original shutter did not open");
            vm.start(0,7);for(unsigned i=0;i<70;++i){vm.tick();world.tick();sound.mix();}check(vm.errors.empty()&&std::abs(world.state(2,6)->transform.position.y-20)<.02f,"original shutter closing/reset node path did not finish");
            vm.start(0,8);for(unsigned i=0;i<70;++i){vm.tick();world.tick();sound.mix();}check(vm.errors.empty()&&std::abs(world.state(2,15)->transform.position.y-14)<.02f,"second original shutter did not close");

        }
    }
    if(!failures.empty()){for(const auto& f:failures)std::cerr<<"FAIL "<<f<<'\n';std::cerr<<failures.size()<<" ItemBoxChange section(s) failed; the remaining progression checks passed.\n";return 1;}
    std::cout<<"Shared texture lifetime, additive effect depth, ItemBoxChange, room motion segments and cue loops passed; effect procedures and enemy spawn activation are documented gaps.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

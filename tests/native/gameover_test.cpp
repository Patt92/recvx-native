#include "gameover.h"
#include "asset_io.h"
#include "player_stairs.h"
#include "world.h"
#include <cmath>
#include <memory>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
// Positive test of the game-over sequence (gameover.c semantics): the four
// dispatcher phases, the pass counts of bhInitGameOver/bhMainGameOver, the
// cleared original flags, the continue prompt with its alpha ramp and choice,
// Retry and the return to the title, and the death scene with its explosion
// table. The native mechanism in gameover.cpp is driven directly; no original
// procedure is linked.
static void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
// player.c:216-227 PlMtnAct[wpntp][dmlvl][3] (Todesmotion), gelesen ueber den nativen Tabellenbesitzer.
static unsigned PlMtnAct_death_column(int wpntp,int dmlvl){return recvx_player_mtn_act(wpntp,dmlvl,3);}
static void put(std::vector<std::uint8_t>& b,unsigned at,unsigned v){std::memcpy(b.data()+at,&v,4);}
static std::vector<std::uint8_t> room(){std::vector<std::uint8_t>b(1500);float version=1.73f;std::memcpy(b.data(),&version,4);put(b,16,64);put(b,28,1100);put(b,102,1400);put(b,1100,16);put(b,1104,18);put(b,1108,80);put(b,1112,110);return b;}
namespace {
struct Session {
    std::vector<std::uint8_t> bytes=room();
    Inventory inventory;MessageDialog dialog;
    EventVm events{bytes,inventory,dialog};
    AudioMixer audio;
    GameOver over;
    unsigned passes=0;
    GameOver::Result last=GameOver::Result::Running;
    explicit Session(const std::filesystem::path& data):audio(data),over(data){}
    GameOver::Result step(unsigned pad=0){last=over.tick(pad,events,audio);++passes;return last;}
    // Runs until `stop` holds, returning how many passes that took.
    template<typename Stop> unsigned run(unsigned limit,Stop stop,unsigned pad=0){
        for(unsigned i=0;i<limit;++i){
            if(stop())return i;
            step(pad);
        }
        check(stop(),"the game over did not reach the expected state");
        return limit;
    }
    bool played(const std::string& name)const{
        return std::find(audio.played.begin(),audio.played.end(),name)!=audio.played.end();
    }
};
}
int main(int argc,char** argv){try{
    _Make_SinTable();
    const auto data=argc>1?std::filesystem::path(argv[1]):std::filesystem::path{};

    // --- dispatcher, init flags and the pass counts of the opening ---------
    {Session s(data);
     for(unsigned bit:{69u,67u,70u})s.events.set_flag(1,bit,0);
     s.events.event_deadline_mode=2;s.events.event_deadline_ticks=900;
     s.over.begin(s.events,0,0,0,0,nullptr);
     check(s.over.active(),"bhSelectContinue did not start the dispatcher");
     s.step(); // bhInitGameOver
     check(!s.events.flag(1,69)&&!s.events.flag(1,67)&&!s.events.flag(1,70),
           "bhInitGameOver did not clear the three original timer flags");
     check(s.events.flags[12][0]&0x100u,"bhInitGameOver did not raise ts_flg 0x100");
     check(!(s.events.flags[10][0]&0x4u),"bhInitGameOver did not clear cb_flg 0x4");
     check(!s.events.event_deadline_mode&&!s.events.event_deadline_ticks,
           "bhInitGameOver left the event timer running");
     check(s.over.phase()==0,"the main pass did not start in mode 0");
     const auto opening=s.run(200,[&]{return s.over.phase()!=0;});
     check(opening==71,("mode 0 held for "+std::to_string(opening)+" passes instead of 71").c_str());
     check(s.over.phase()==1,"mode 0 did not hand over to the mesh");
     const auto mesh=s.run(200,[&]{return s.over.phase()!=1;});
     check(mesh==51,("the mesh ran "+std::to_string(mesh)+" passes before the prompt instead of 51").c_str());
     check(s.over.phase()==2,"the mesh did not open the continue prompt");
     // The prompt fades in by 16 alpha steps.
     check(s.over.count()==0,"the prompt started with a visible alpha");
     const auto ramp=s.run(60,[&]{return s.over.count()>=0xFF;});
     check(ramp==16,("the prompt alpha ramp took "+std::to_string(ramp)+" passes instead of 16").c_str());}

    // --- the choice: left/right move it, Yes retries -----------------------
    {Session s(data);
     s.over.begin(s.events,0,0,0,0,nullptr);
     s.run(300,[&]{return s.over.phase()==2&&s.over.count()>=0xFF;});
     const auto before=s.audio.played.size();
     s.step(0x8u); // right -> No
     check(s.audio.played.size()>before,"moving the choice played no cursor sound");
     s.step(0x4u); // left -> Yes
     s.step(0x800u); // confirm Yes
     check(s.over.phase()==3,"confirming Yes did not start the retry fade");
     const auto fade=s.run(60,[&]{return s.last==GameOver::Result::Retry;});
     check(s.last==GameOver::Result::Retry,"Yes did not reach bhPopGameData");
     check(fade>5&&fade<30,("the retry fade took "+std::to_string(fade)+" passes").c_str());
     check(!s.over.active(),"the dispatcher stayed active after the retry");
     check(s.events.flags[12][0]&0x4000u,"bhPopGameData did not set ts_flg 0x4000");}

    // --- No plays the death voice and runs out into the title --------------
    {Session s(data);
     s.over.begin(s.events,0,0,0,0,nullptr);
     s.run(300,[&]{return s.over.phase()==2&&s.over.count()>=0xFF;});
     s.step(0x8u);                 // choose No
     s.step(0x800u);               // confirm
     check(s.over.phase()==1,"declining did not return to the mesh");
     if(!data.empty())check(s.played("ADV/0008"),"declining did not play the original death voice");
     const auto out=s.run(400,[&]{return s.last==GameOver::Result::Title;});
     check(s.last==GameOver::Result::Title,"the declined game over did not return to the title");
     check(out>100,"the mesh did not run its remaining passes before the title");
     check(!s.over.active(),"the dispatcher stayed active after the title return");}

    // --- battle mode (gm_mode 3) has no prompt and exits into a retry ------
    {Session s(data);
     s.over.begin(s.events,1,3,0,0,nullptr);
     s.step();
     check(!(s.events.flags[11][0]&0x80000000u),"battle mode did not clear gm_flg 0x80000000");
     s.run(200,[&]{return s.over.phase()==1;});
     s.run(400,[&]{return s.last!=GameOver::Result::Running;});
     check(s.last==GameOver::Result::Retry,"battle mode did not retry without a prompt");
     if(!data.empty())check(s.played("ADV/0009"),"battle mode played no death voice for player 1");}

    // --- death scene: the original explosion table and its dying motion ----
    {Session s(data);
     std::unique_ptr<PlayerAsset> body;if(!data.empty())body=std::make_unique<PlayerAsset>(PlayerAsset::load(data,10));
     // gameover.c:446 bhSetMotion(plp, mtn_add, 0, NULL): Knotenteil je Pass; frm_no schiebt auch
     // waehrend der 8 Mischpaesse nach dem fuenften gov_etb-Eintrag (:470-478).
     struct Node{unsigned motion;std::int64_t frame;unsigned hokan;int rate;};std::vector<Node> nodes;unsigned entered=0;
     // gameover.c:289 liest exp0->wpntp/dmlvl beim Eintritt der Szene: begin() bekommt 0/0, der
     // Spielerbesitzer meldet dmlvl 1 -> Spalte 3 der zweiten Zeile (Motion 7), nicht Motion 4.
     s.over.death_scene_levels=[&](unsigned& w,unsigned& d){w=0;d=1;};
     s.over.death_scene_enter=[&](unsigned m){++entered;check(m==PlMtnAct_death_column(0,1),"death scene entered with a motion outside PlMtnAct[wpntp][dmlvl][3] of the scene entry");};
     s.over.death_motion_nodes=[&](unsigned m,std::int64_t f,unsigned hc,int rate){nodes.push_back({m,f,hc,rate});};
     // gameover.c:539 bhSetExplosionEffect(&gov_etb[exp_ct].px): der Weltbesitzer bekommt jede
     // Explosion mit ihrer Tabellenlage; hier wird Pass und Lage festgehalten (Aufstellung).
     struct Blast{unsigned pass;float x,y,z;};std::vector<Blast> blasts;unsigned left=0;
     s.over.death_blast=[&](NJS_POINT3 p){blasts.push_back({s.passes,p.x,p.y,p.z});return true;};
     s.over.death_scene_leave=[&]{++left;};
     s.over.begin(s.events,0,0,0,0,body.get());
     s.step();                                   // bhInitGameOver
     s.events.flags[11][0]|=0x400u;              // the self-destruct death
     s.run(200,[&]{return s.over.phase()!=0;});
     const unsigned scene_entry=s.passes;        // der Pass, der md1 10 setzte; gov_ct 0 ist der naechste
     check(s.over.phase()==10,"gm_flg 0x400 did not start the death scene");
     check(s.events.flags[6][0]==0x6Cu,"the death scene did not take sp_flg 0x6C");
     check((s.events.flags[5][0]&0x5u)==0x5u,"the death scene did not set st_flg 0x1/0x4");
     check(s.events.flags[11][0]&0x200080u,"the death scene did not set gm_flg 0x200080");
     if(!data.empty()){
         GameUi ui(data);SoftwareRenderer image(320,240);s.over.draw(image,ui);
         // gameover.c:244 explicitly installs 2/20000. Setting cam.flg 0x40
         // later is not itself a call of bhChangeViewClipRM.
         check(image.last_clip()[0]==2.f&&image.last_clip()[1]==20000.f,
               "death scene lost the explicitly installed original clip range");
     }
     // gov_etb fires at 40, 54, 68, 82, 96 and 110; the fifth one starts the
     // dying motion, its voice and the white flash. The scene hands back after
     // pass 150.
     const auto scene=s.run(300,[&]{return s.over.phase()!=10;});
     check(scene==151,("the death scene ran "+std::to_string(scene)+" passes instead of 151").c_str());
     check(s.over.blasts()==6,"the death scene did not fire all six gov_etb explosions");
     check(s.over.unsupported_blasts()==0,"a bound death_blast owner was still counted as unsupported");
     // gameover.c:35-43 gov_etb: frm 40/54/68/82/96/110 (gov_ct beim Pass), px/py/pz der Szene.
     {const float want[6][4]={{40,-25,10,-80},{54,-14,10,-60},{68,0,10,-40},{82,-6,10,-20},{96,3,10,-10},{110,-4,10,10}};
      check(blasts.size()==6,("death_blast was called "+std::to_string(blasts.size())+" times instead of once per gov_etb entry").c_str());
      for(unsigned i=0;i<6;++i){const auto& b=blasts[i];
          // s.passes zaehlt nach dem Tick hoch: im ersten Pass von md1 10 (gov_ct 0 bei der Pruefung :535,
          // gov_ct++ danach :544) steht es noch auf scene_entry, bei gov_ct n auf scene_entry + n.
          check(b.pass==scene_entry+unsigned(want[i][0]),("gov_etb entry "+std::to_string(i)+" fired at gov_ct "+std::to_string(int(b.pass)-int(scene_entry))+" instead of "+std::to_string(int(want[i][0]))).c_str());
          check(b.x==want[i][1]&&b.y==want[i][2]&&b.z==want[i][3],("gov_etb entry "+std::to_string(i)+" was handed to the owner with another position than gameover.c:35-43").c_str());}}
     check(left==1,"the scene did not hand pt_flg = 0 (gameover.c:567) to the owner exactly once on leaving md1 10");
     check(s.over.phase()==1,"the death scene did not hand back to the mesh");
     check(entered==1,"the death scene entry (mnwP = mnwPb, motion column 3) was not handed to the player owner");
     if(body){
         check(nodes.size()==151,("the node part ran "+std::to_string(nodes.size())+" times instead of once per pass").c_str());
         // Pass 110 fires the fifth blast: motion 25 from frame 0 with hokan 8/49152; the next
         // eight node parts blend (hokan 8..1) while frm_no advances by one frame each pass.
         const auto start=std::find_if(nodes.begin(),nodes.end(),[](const Node& n){return n.motion==25&&n.hokan==8;});
         check(start!=nodes.end()&&start->frame==0&&start->rate==49152,"the fifth blast did not restart motion 25 at frame 0 with hokan 8/49152");
         for(unsigned i=0;i<8;++i){const auto& n=*(start+i);
             check(n.motion==25&&n.hokan==8-i&&n.frame==std::int64_t(i)*65536,("blend pass "+std::to_string(i)+" held frm_no or hokan_count (Motion.c:21 advances every pass)").c_str());}
         check(nodes.front().motion==PlMtnAct_death_column(0,1)&&nodes.front().hokan==0&&nodes.front().rate==0,"the first node part did not sample the column-3 motion without a blend");
     }
     check(s.events.flags[6][0]==0x20u,"the death scene did not restore sp_flg 0x20");
     if(!data.empty())check(s.played("ADV/0008"),"the death scene played no death voice");}
    // --- death scene body on screen: world origin under cam.mtx of (0, 10, 35) ----------------
    // game.c:357 bhPutModel(plp) draws the body that bhCalcModel (MdlPut.c:240-264) placed at
    // plp->px/py/pz = 0 (gameover.c:303-308) under cam.mtx = cmat * T(0, -10, -35) (camera.c:28-37,
    // ln 0, angles 0; cam of gameover.c:313-327). With cmat = diag(1, -1, -1) the origin lies at
    // camera (0, 10, 35): 10 below the axis, 35 in front. njSetPerspective(11832) is 65 degrees, so
    // on 320x240 the feet (y 0) project to row 120 + 160 / tan(32.5 deg) * 10 / 35 = 191.7 and the
    // figure stands upright in the picture. A body offset by the camera position instead sits at
    // the camera and is cut by the top edge and the near plane (the former picture).
    if(!data.empty()){
     auto body=std::make_unique<PlayerAsset>(PlayerAsset::load(data,10));
     auto make=[&](const PlayerAsset* asset){
         auto s=std::make_unique<Session>(data);
         s->over.death_blast=[](NJS_POINT3){return true;};
         s->over.begin(s->events,0,0,0,0,asset);
         s->step();s->events.flags[11][0]|=0x400u;
         s->run(200,[&]{return s->over.phase()!=0;});
         for(int i=0;i<36;++i)s->step(); // gov_ct 36: fade of :262 finished (30), first gov_etb entry at 40
         check(s->over.phase()==10&&s->over.blasts()==0,"the body picture was not taken in md1 10 before the first explosion");
         return s;};
     auto with=make(body.get()),without=make(nullptr);
     GameUi ui(data);SoftwareRenderer a(320,240),b(320,240);
     with->over.draw(a,ui);without->over.draw(b,ui);
     int min_x=320,max_x=-1,min_y=240,max_y=-1;unsigned pixels=0;
     for(int y=0;y<240;++y)for(int x=0;x<320;++x){
         if(a.canvas()[std::size_t(y)*320+x]==b.canvas()[std::size_t(y)*320+x])continue;
         ++pixels;min_x=std::min(min_x,x);max_x=std::max(max_x,x);min_y=std::min(min_y,y);max_y=std::max(max_y,y);}
     check(pixels>500,"the death scene drew no body (the mesh alone is the same with and without PlayerAsset)");
     const std::string box="body pixels x "+std::to_string(min_x)+".."+std::to_string(max_x)+" y "+std::to_string(min_y)+".."+std::to_string(max_y);
     check(min_y>0&&max_y<239&&min_x>0&&max_x<319,("the death scene body is cut by the picture edge: "+box).c_str());
     check(max_y>=182&&max_y<=200,("the feet of the body are not on row 192 of the origin under cam.mtx (0, 10, 35): "+box).c_str());
     check(min_y<120,("the body does not rise above the camera axis (row 120): "+box).c_str());
     check(min_x>100&&max_x<220,("the body is not centred on the camera axis: "+box).c_str());}
    // --- death scene light: body through the room owner, mesh without light -------------------
    // game.c:357 bhPutModel(plp) runs in the light state bhAllDrawModel leaves: md1 10 sets gm_flg 0x80
    // (gameover.c:323), so bhControlLight (light.c:332) and bhSetHalfLight (game.c:352, light.c:992) end in
    // bhSetEasyDirLight (light.c:279: colour 1, intensity 0.6, ambient 0.2, direction 0/0.3/0.7) and
    // MdlPut.c:33-37 draws with that single light. Its owner is RoomWorld::append (category 1 under gm_flg
    // 0x80); the body must reach it through death_scene_append, every vertex then carries 0.2 + 0.6 * N.L
    // in grey. The mesh (njDrawTriangle3D, ps2_NaGraphics3D.c:401) takes the vertex colours gv->cl = -1
    // (gameover.c:140) as they are: final colour, no host shading. Fixture: RM_3091, the self-destruct room.
    if(!data.empty()&&std::filesystem::exists(data/"rooms/RM_3091.RDT")){
     auto asset=std::make_unique<PlayerAsset>(PlayerAsset::load(data,10));
     const auto bytes=read_asset_file(data/"rooms/RM_3091.RDT",64*1024*1024);Scene stage=decode_scene(bytes);SceneSpawn actor=stage.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;
     Session s(data);
     RoomWorld world(bytes,stage,actor,motion,frame,s.audio,data,{},"RM_3091");world.set_player_asset(asset.get());world.bind_player_collision(s.events);
     std::size_t handed=0,unlit=0,ungrey=0,mesh=0,mesh_shaded=0;float lo=9.f,hi=-9.f;
     s.over.death_blast=[](NJS_POINT3){return true;};
     s.over.death_scene_enter=[&](unsigned){world.set_presentation_flags(0x15u);world.clear_effects();}; // gameover.c:296-297/278 wie runtime.cpp
     s.over.death_scene_append=[&](Scene& scene,const SceneCamera& camera,Scene& body){
         for(const auto& t:scene.triangles){++mesh;if(!t.world_lit||t.gouraud||t.color!=0xffffffffu)++mesh_shaded;}
         Scene pass;pass.textures=body.textures;pass.triangles=body.triangles;pass.ambient_writes=body.ambient_writes;handed=pass.triangles.size();
         world.append(pass,{},camera);
         for(std::size_t i=0;i<handed&&i<pass.triangles.size();++i){const auto& t=pass.triangles[i];if(!t.world_lit||!t.gouraud){++unlit;continue;}
             for(const auto& v:t.vertices){if(std::abs(v.light.x-v.light.y)>1e-5f||std::abs(v.light.x-v.light.z)>1e-5f)++ungrey;lo=std::min(lo,v.light.x);hi=std::max(hi,v.light.x);}}};
     s.over.begin(s.events,0,0,0,0,asset.get());
     s.step();s.events.flags[11][0]|=0x400u;
     s.run(200,[&]{return s.over.phase()!=0;});
     for(int i=0;i<36;++i)s.step();
     GameUi ui(data);SoftwareRenderer image(320,240);s.over.draw(image,ui);
     const std::string got="handed "+std::to_string(handed)+", unlit "+std::to_string(unlit)+", non-grey "+std::to_string(ungrey)+", light "+std::to_string(lo)+".."+std::to_string(hi)+", mesh "+std::to_string(mesh)+" shaded "+std::to_string(mesh_shaded);
     check(handed>500,("the death scene did not hand the body to the room owner's light pass: "+got).c_str());
     check(unlit==0&&ungrey==0&&lo>=0.2f-1e-4f&&hi<=0.8f+1e-4f&&hi>0.5f,("the body is not lit by bhSetEasyDirLight(0.6) (light.c:279/992): "+got).c_str());
     check(mesh==450&&mesh_shaded==0,("the mesh is not drawn with its plain vertex colours (njDrawTriangle3D): "+got).c_str());
     std::cout<<"Death scene light: "<<got<<".\n";}
    // --- death scene without a bound owner: the six requests stay reported, not replaced ----
    {Session s(data);
     s.over.begin(s.events,0,0,0,0,nullptr);
     s.step();s.events.flags[11][0]|=0x400u;
     s.run(200,[&]{return s.over.phase()!=0;});
     s.run(300,[&]{return s.over.phase()!=10;});
     check(s.over.blasts()==6&&s.over.unsupported_blasts()==6,"without death_blast the gov_etb requests were not all reported as unsupported");}
    {Session s(data);
     s.over.death_blast=[](NJS_POINT3){return false;}; // der Besitzer konnte keinen Satz anlegen
     s.over.begin(s.events,0,0,0,0,nullptr);
     s.step();s.events.flags[11][0]|=0x400u;
     s.run(200,[&]{return s.over.phase()!=0;});
     s.run(300,[&]{return s.over.phase()!=10;});
     check(s.over.blasts()==6&&s.over.unsupported_blasts()==6,"a death_blast owner returning false was not counted as unsupported");}

    // --- the screen is actually drawn ---------------------------------------
    if(!data.empty()){
        Session s(data);GameUi ui(data);
        s.over.begin(s.events,0,0,0,0,nullptr);
        s.run(200,[&]{return s.over.phase()==1;});
        SoftwareRenderer image(320,240);
        std::fill(image.canvas().begin(),image.canvas().end(),0xff123456u);
        s.over.draw(image,ui);
        check(std::any_of(image.canvas().begin(),image.canvas().end(),[](std::uint32_t p){return p!=0xff123456u;}),
              "the game-over mesh drew nothing");
        s.run(200,[&]{return s.over.phase()==2&&s.over.count()>=0xFF;});
        std::fill(image.canvas().begin(),image.canvas().end(),0xff123456u);
        s.over.draw(image,ui);
        check(std::any_of(image.canvas().begin(),image.canvas().end(),[](std::uint32_t p){return p!=0xff123456u;}),
              "the continue prompt drew nothing");
    } else std::cout<<"No cooked data supplied: drawing and the original voices were not exercised.\n";

    std::cout<<"Game over: dispatcher phases, timings, cleared flags, prompt, retry, title return and death scene verified.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"Game-over test failed: "<<e.what()<<'\n';return 1;}}

#include "gameover.h"
#include "back_color.h"
#include "screen_fade.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

// Game over (gameover.c semantics) as a native mechanism: the four dispatcher
// phases, the GV_WORK with its 16x16 point mesh, the ring UV table, the death
// scene, the continue prompt and the exit into bhPopGameData or bhReturnTitle.
// The screen fade is not part of the game over: gameover.c only calls
// bhSetScreenFade on the one sys->fade_* work (system_screen_fade, screen_fade.h)
// and reads cb_flg 0x2; bhControlScreenFade/bhDrawScreenFade run in Task 7
// (game.c:40/111), which keeps running during the game over. No original
// procedure is linked; the small constant tables below are data.
namespace recvx {
namespace {
// gov_yn: per-column ripple weight of the mesh.
const float gov_yn[16]={
    0.005f,0.010f,0.005f,0.008f,
    0.012f,0.012f,0.010f,0.002f,
    0.003f,0.005f,0.008f,0.010f,
    0.002f,0.008f,0.005f,0.001f};
// gov_etb: explosion frame and position of the death scene, terminated by frm 0.
struct DeathBlast {int frame;float x,y,z;};
const DeathBlast gov_etb[7]={
    { 40,-25.f,10.f,-80.f},
    { 54,-14.f,10.f,-60.f},
    { 68,  0.f,10.f,-40.f},
    { 82, -6.f,10.f,-20.f},
    { 96,  3.f,10.f,-10.f},
    {110, -4.f,10.f, 10.f},
    {  0,  0.f, 0.f,  0.f}};
// cnt_mes: the continue prompt, in the original message encoding.
const std::uint16_t cnt_mes[17]={
    65281,50,69,84,82,89,31,65280,65281,57,69,83,65281,65281,46,79,0xFFFF};
// player.c PlMtnAct[wpntp][dmlvl][...]; column 3 is the death motion.
const unsigned short PlMtnAct[2][3][7]={
    {{42,39,0,4,16,52,9},{43,40,2,7,18,52,9},{44,41,3,8,19,52,9}},
    {{42,39,1,6,17,54,9},{43,40,2,7,18,54,9},{44,41,3,8,19,54,9}}};
// njSetPerspective(11832) for both the mesh and the death scene camera.
constexpr int kPerspective=11832;
// Ninja angle of a value in degrees, as the original rounds and masks it.
int degrees(float value){return int(182.04445f*value)&0xFFFF;}
}

struct GameOver::Impl {
    std::filesystem::path data,mods;
    bool running=false;
    unsigned md0=0,md1=0,ct=0;
    int mode=0;unsigned character=0,wpntp=0,dmlvl=0;
    const PlayerAsset* body=nullptr;
    // GV_WORK
    NJS_POINT3 pt[256]{},cp[256]{},op[256]{};
    int uvu[256]{},uvv[256]{};
    float pz=0,scl=0,tfn=0,tnn=0;
    int cont_x=0,count=0,cur_ct=0,exp_ct=0;
    // Death scene player work.
    unsigned player_motion=0,hokan=0;int hokan_rate=0;std::int64_t player_frame=0;
    unsigned blasts=0,unsupported_blasts=0;
    mutable std::vector<SceneTexture> textures;
    mutable bool textures_tried=false;
    // plp->txp[0] of the death scene body, fetched once per game over (gameover.c:313 mlwP->texP = txp[0]).
    mutable std::vector<SceneTexture> body_textures;
    mutable bool body_textures_tried=false;

    void ring(int index,float radius,float angle){
        uvu[index]=int(radius*njSin(degrees(angle)))+128;
        uvv[index]=int(-(radius*njCos(degrees(angle))))+128;
    }
    void build_uv(){
        int x=0,y=0,ct_ring=15;
        for(float radius=126.f-14.f;radius!=0.f;radius-=14.f,++x,++y,ct_ring-=2){
            int rn_i=0;const float ar=360.f/(4.f*float(ct_ring));
            for(int j=0;j<ct_ring;++j,++rn_i)ring((x+j)+(y*16),radius,ar*float(rn_i)-45.f);
            for(int j=0;j<ct_ring;++j,++rn_i)ring((y+ct_ring)+((x+j)*16),radius,ar*float(rn_i)-45.f);
            for(int j=ct_ring;j>0;--j,++rn_i)ring((y+j)+((x+ct_ring)*16),radius,ar*float(rn_i)-45.f);
            for(int j=ct_ring;j>0;--j,++rn_i)ring(y+((x+j)*16),radius,ar*float(rn_i)-45.f);
        }
    }
    void ripple(){
        const float scn=tfn;
        int x=0,y=0,ct_ring=15,row=0;
        for(float radius=8.f;radius>0.f;radius-=1.f,++x,++y,row+=16,ct_ring-=2){
            int rn_i=0;const float ar=360.f/(4.f*float(ct_ring));
            auto place=[&](int index){
                const int a=degrees((ar*float(rn_i)-45.f)+radius*scn);
                cp[index].x=radius*njSin(a);cp[index].y=radius*njCos(a);++rn_i;
            };
            for(int j=0;j<ct_ring;++j)place((x+j)+row);
            for(int j=0;j<ct_ring;++j)place((x+ct_ring)+((y+j)*16));
            for(int j=ct_ring;j>0;--j)place((x+j)+((y+ct_ring)*16));
            for(int j=ct_ring;j>0;--j)place(x+((y+j)*16));
        }
        for(int gy=0;gy<16;++gy)for(int gx=0;gx<16;++gx){
            const int i=gx+gy*16;
            pt[i].x=cp[i].x+op[i].x;pt[i].y=cp[i].y+op[i].y;pt[i].z=pz+op[i].z;
            op[i].y+=scl*((cp[i].y-9.f)*gov_yn[gx]);
        }
    }
    const std::vector<SceneTexture>& mesh_textures()const{
        if(!textures_tried){
            textures_tried=true;
            const auto entry=data/"archives/SYSTEM/0001.bin";
            if(std::filesystem::is_regular_file(entry)){
                const auto bytes=read_asset_file(entry,8*1024*1024);
                const auto offset=system_texture_bundle_offset(bytes);
                if(offset!=std::size_t(-1))textures=decode_texture_bundle(bytes,offset);
            }
        }
        return textures;
    }
};

GameOver::GameOver(const std::filesystem::path& data,const std::filesystem::path& mods):impl_(std::make_unique<Impl>()){
    impl_->data=data;impl_->mods=mods;
}
GameOver::~GameOver()=default;

void GameOver::begin(EventVm& events,unsigned character,int mode,unsigned weapon,unsigned level,const PlayerAsset* body){
    auto& g=*impl_;
    // bhSelectContinue: the work is fresh for every death. The asset paths are
    // taken out first; the reset would otherwise clear them before the copy.
    const auto data=g.data,mods=g.mods;
    g=Impl{};
    g.data=data;g.mods=mods;
    g.character=character;g.mode=mode;
    g.wpntp=std::min(weapon,1u);g.dmlvl=std::min(level,2u);
    g.body=body;
    g.running=true;g.md0=1;g.md1=0;g.cont_x=0;g.count=0;g.cur_ct=0;
    (void)events;
}

GameOver::Result GameOver::tick(unsigned pressed,EventVm& events,AudioMixer& audio){
    auto& g=*impl_;
    if(!g.running)return Result::Running;
    Result result=Result::Running;
    // bhSetScreenFade (screen.c:21) auf dem einen sys->fade_* mit cb_flg |= 0x2.
    auto fade=[&](std::uint32_t argb,float ct){set_screen_fade(events.flags[10][0],argb,ct);};
    switch(g.md0){
    case 1:{ // bhInitGameOver
        events.flags[12][0]|=0x100u;      // ts_flg
        events.flags[10][0]&=~0x4u;       // cb_flg
        g.ct=0;g.md0=2;g.md1=0;
        for(unsigned bit:{69u,67u,70u})events.set_flag(1,bit,1); // bhCrFlg
        events.event_deadline_mode=0;events.event_deadline_ticks=0;
        if(g.mode==3)events.flags[11][0]&=~0x80000000u;
        fade(0xFFE0E0E0u,60.f); // gameover.c:122
        g.pz=-40.f;g.scl=0.01f;g.tfn=50.f;g.tnn=-g.tfn/30.f;
        for(int y=0;y<16;++y)for(int x=0;x<16;++x){
            const int i=x+y*16;
            g.pt[i]={float(x)-7.5f,-(float(y)-7.5f),0};
            g.cp[i]=g.pt[i];g.op[i]={0,0,0};
            njUnitVector(&g.cp[i]);
        }
        g.build_uv();
        break;}
    case 2:
        switch(g.md1){
        case 0:
            ++g.ct;
            if(g.ct>70){
                g.ct=0;
                if((events.flags[11][0]&0x40u)&&(events.flags[5][0]&0x800000u)){
                    if(!(events.flags[11][0]&0x1000000u))events.flags[11][0]&=~0xC0u;
                    events.flags[5][0]&=~0x800000u;events.flags[11][0]&=~0x80000u;
                }
                if(events.flags[11][0]&0x400u){
                    // Death scene: the player body plays its dying motion while
                    // the self-destruct blasts walk past (gm_flg 0x400).
                    g.md1=10;
                    fade(0u,30.f); // gameover.c:262
                    audio.request_all_stop_sound(true,true,30); // gameover.c:264 RequestAllStopSoundEx(1, 1, 30)
                    set_back_color(0); // gameover.c:266
                    events.flags[6][0]=0x6C;events.flags[11][0]&=~0x100100u;
                    events.flags[10][0]|=0x40000000u;
                    events.flags[5][0]|=0x5u; // st_flg 0x1 | 0x4
                    events.flags[11][0]|=0x200080u;
                    if(death_scene_levels){death_scene_levels(g.wpntp,g.dmlvl);g.wpntp=std::min(g.wpntp,1u);g.dmlvl=std::min(g.dmlvl,2u);} /* gameover.c:289: exp0->wpntp/dmlvl dieses Passes */
                    g.player_motion=PlMtnAct[g.wpntp][g.dmlvl][3];
                    g.player_frame=0;g.hokan=0;g.hokan_rate=0; /* hokan_rate = 0, hokan_count = 0 */
                    if(death_scene_enter)death_scene_enter(g.player_motion);
                    break;
                }
                ++g.md1;
                fade(0u,30.f); // gameover.c:330
                audio.request_all_stop_sound(true,true,60); // gameover.c:332 RequestAllStopSoundEx(1, 1, 60)
                set_back_color(0); // gameover.c:334
                events.flags[6][0]=0x20;
                events.flags[11][0]&=~0x100100u;
            }
            break;
        case 1:
            g.ripple();
            if(g.ct==50){
                if(g.mode<3)g.md1=2;
                else audio.play_dead_voice(int(g.character));
            }
            if(g.tfn>0)g.tfn+=g.tnn;
            g.pz+=0.1f*(-14.f-g.pz);
            g.scl+=0.005f;
            ++g.ct;
            if(g.ct>120&&!(events.flags[10][0]&0x2u)){ // gameover.c:422 !(sys->cb_flg & 0x2)
                fade(0xFF000000u,30.f); // gameover.c:424
                audio.request_all_stop_sound(true,true,30); // gameover.c:426 RequestAllStopSoundEx(1, 1, 30)
            } else if(g.ct>170)g.md0=3;
            break;
        case 2:
            if(g.count<0xFF)g.count=std::min(0xFF,g.count+16);
            else {
                g.cur_ct=(g.cur_ct+2048)&0x7FFF;
                if((pressed&0x4u)&&g.cont_x>0){--g.cont_x;audio.play_system(2);}
                if((pressed&0x8u)&&g.cont_x<=0){++g.cont_x;audio.play_system(2);}
                if(pressed&0x800u){
                    if(g.cont_x==0){
                        fade(0xFF000000u,10.f); // gameover.c:485
                        audio.request_all_stop_sound(true,true,10); // gameover.c:487 RequestAllStopSoundEx(1, 1, 10)
                        g.md1=3;audio.play_system(3);
                    } else {
                        g.md1=1;
                        if(!(events.flags[11][0]&0x400u))
                            audio.play_dead_voice(events.flag(1,73)?0:int(g.character));
                    }
                }
            }
            break;
        case 3:
            // bhPopGameData once the fade has finished.
            if(!(events.flags[10][0]&0x2u)){events.flags[12][0]|=0x4000u; // gameover.c:514 !(sys->cb_flg & 0x2)
                g.running=false;result=Result::Retry;}
            break;
        case 10:{
            events.flags[5][0]|=0x4u;
            // bhSetMotion(plp, mtn_add, 0, NULL) (Motion.c:21): der Knotenteil tastet objP am
            // Stand vor dem Vorschub ab und mischt hokan_count Paesse lang (SetMtn*Hokan zieht
            // hokan_count ab); frm_no schiebt in jedem Pass, auch waehrend der Mischung.
            if(g.body&&g.body->has_motion(g.player_motion)){
                if(death_motion_nodes)death_motion_nodes(g.player_motion,g.player_frame,g.hokan,g.hokan_rate);
                if(g.hokan>0)--g.hokan;
                const auto frames=std::int64_t(g.body->motion_frames(g.player_motion))*65536;
                if(frames>0)g.player_frame=(g.player_frame+65536)%frames;
            }
            if(g.player_motion==25&&g.player_frame/65536==0)g.player_motion=26;
            const auto& blast=gov_etb[std::min<unsigned>(g.blasts,6)];
            if(blast.frame!=0&&unsigned(blast.frame)<=g.ct){
                audio.play_player_voice(534);
                // gameover.c:539 bhSetExplosionEffect(&gov_etb[exp_ct].px): weapon.c:1812 at the
                // world owner; without a binding the request is counted as unsupported, never
                // replaced by a substitute.
                if(!(death_blast&&death_blast({blast.x,blast.y,blast.z})))++g.unsupported_blasts;
                ++g.blasts;
                if(g.blasts==5){
                    g.player_motion=25;g.player_frame=0;g.hokan=8;g.hokan_rate=49152;
                    audio.play_dead_voice(int(g.character));
                    fade(0xFFFFFFFFu,30.f); // gameover.c:556 bhSetScreenFade(-1, 30)
                }
            }
            ++g.ct;
            if(g.ct>150){
                fade(0u,30.f); // gameover.c:564
                g.ct=0;g.md1=1;
                events.flags[6][0]=0x20;
                if(death_scene_leave)death_scene_leave(); // gameover.c:567 pt_flg = 0
            }
            break;}
        default:break;
        }
        break;
    case 3: // bhExitGameOver
        g.running=false;
        if(g.mode<3)result=Result::Title;
        else {events.flags[12][0]|=0x4000u;result=Result::Retry;}
        break;
    default:break;
    }
    // Kein bhControlScreenFade hier: gameover.c ruft es nie. Es laeuft im Spieltask (game.c:38-41,
    // Task 7 vor Task 14, main.c:195-201), also im naechsten Bild vor diesem Dispatcher.
    return result;
}

bool GameOver::active()const{return impl_->running;}
void GameOver::abort(){impl_->running=false;}
unsigned GameOver::phase()const{return impl_->md1;}
unsigned GameOver::count()const{return unsigned(impl_->count);}
unsigned GameOver::blasts()const{return impl_->blasts;}
unsigned GameOver::unsupported_blasts()const{return impl_->unsupported_blasts;}


void GameOver::draw(SoftwareRenderer& image,const GameUi& ui)const{
    const auto& g=*impl_;
    if(!g.running)return;
    if(g.md1!=0){
        // Db.clear (njSetBackColor(0,0,0) at the end of md1 0, gameover.c:266/334)
        // plus njSetMatrix(NULL, cmat): the mesh is built
        // in camera space, so the view is the base matrix alone. In the death
        // scene the camera sits at (0, 10, 35) and the mesh moves with it.
        const bool death=g.md1==10;
        Scene scene;scene.background=installed_back_pixel();
        const auto& atlas=g.mesh_textures();
        const bool textured=atlas.size()>4;
        if(textured)scene.textures.push_back(atlas[4]);
        auto vertex=[&](int index){
            SceneVertex v;v.position=g.pt[index];
            if(death){v.position.y+=10.f;v.position.z+=35.f;}
            v.u=float(g.uvu[index])/256.f;v.v=float(g.uvv[index])/256.f;
            return v;
        };
        for(int y=0;y<15;++y){
            std::array<SceneVertex,32> strip{};
            for(int x=0;x<16;++x){strip[x*2]=vertex(x+y*16);strip[x*2+1]=vertex(x+(y+1)*16);}
            for(int i=0;i<30;++i){
                SceneTriangle t;t.vertices={strip[i],strip[i+1],strip[i+2]};
                // njDrawTriangle3D (ps2_NaGraphics3D.c:401): Eckfarbe gv->cl (gameover.c:140, -1) >> 1 ist die ganze
                // Farbe, ohne Lichtrechnung; world_lit = fertige Farbe, keine Hostschattierung (renderer.cpp shade).
                t.color=0xffffffffu;t.textured=textured;t.texture=0;t.camera_mask=false;t.world_lit=true;t.gouraud=false;
                t.blend=SceneBlend::Alpha;
                scene.triangles.push_back(t);
            }
        }
        // game.c:357 bhPutModel(plp) zeichnet den Koerper im Lichtzustand von bhAllDrawModel: md1 10 setzt gm_flg
        // 0x80 (gameover.c:323), bhControlLight (light.c:332) und bhSetHalfLight (game.c:352, light.c:992) laufen
        // dann in bhSetEasyDirLight (light.c:279: Farbe 1, Intensitaet 0.6 / Umgebung 0.2, Richtung 0/0.3/0.7), und
        // MdlPut.c:33-37 hebt draw_tp auf das Einzellicht. Dieser Zustand hat seinen Besitzer im Raum (RoomWorld::
        // append, Kategorie 1 unter gm_flg 0x80); der Koerper geht deshalb mit death_scene_append durch denselben
        // Pass. Das Netz bleibt lichtfrei: njDrawTriangle3D (ps2_NaGraphics3D.c:401) nimmt nur die Eckfarben
        // gv->cl (gameover.c:140, -1), ohne Lichtrechnung.
        Scene body;
        if(death&&g.body&&g.body->has_motion(g.player_motion)){
            // bhCalcModel on the player work (MdlPut.c:240-264: ewP->mtx = T(px,py,pz) * R(ax,ay,az),
            // then bhCalcTree over objP): the body stands at the world origin with the original yaw
            // while the blasts pass; the nodes are plp->mlwP->objP of this pass (death_pose), not a
            // fresh sample of the advanced counter. game.c:357 bhPutModel(plp) draws it in world
            // space under cam.mtx (camera.c:28-37: cmat * T(-cam.px, -cam.py, -cam.pz) with ln 0 and
            // angles 0), i.e. the view below; the body gets no camera-space offset of its own. The
            // mesh of bhDrawGameOver is the only thing built in cmat space and moved above.
            body=g.body->sample(g.player_motion,g.player_frame,{0,0,0},35499,nullptr,death_pose?death_pose():ModelNodeOverrides{});
            if(!g.body_textures_tried){g.body_textures_tried=true;g.body_textures=death_body_textures?death_body_textures():g.body->textures();}
            body.textures=g.body_textures;
            for(auto& t:body.triangles){
                if(t.textured&&t.texture>=body.textures.size())throw std::runtime_error("death scene body texture outside its bundle");
                t.camera_mask=false;t.after_modifiers=true; // game.c:357: bhPutModel(plp) nach bhDrawEffect
            }
            for(auto& w:body.ambient_writes)w.camera_mask=false;
        }
        if(death&&death_scene_append){
            // game.c:318 bhAllDrawModel (Task 7 before Task 14) with pt_flg 0x15 under cam.flg 0x40:
            // the effects of bhSetExplosionEffect are drawn in world space under the scene camera
            // (0, 10, 35), angles 0, pers 11832 (gameover.c:313-327); the mesh above is built in
            // camera space and moved by the same translation, so one view matrix serves both.
            SceneCamera camera;camera.position={0.f,10.f,35.f};camera.angles={0,0,0};camera.distance=0.f;camera.perspective=kPerspective;camera.clip_near=2.f;camera.clip_far=20000.f;
            death_scene_append(scene,camera,body);
        } else if(!body.triangles.empty()){
            // Ohne Raumbesitzer (Werkzeug/Test) gibt es keinen Lichtzustand; der Koerper bleibt mit seinen Grundfarben.
            const auto base=scene.textures.size();
            scene.textures.insert(scene.textures.end(),body.textures.begin(),body.textures.end());
            for(auto t:body.triangles){if(t.textured)t.texture=std::uint16_t(base+t.texture);scene.triangles.push_back(t);}
        }
        const float focal=(float(image.width())*0.5f)/std::tan(0.0000958738f*(kPerspective/2));
        NJS_MATRIX view;njUnitMatrix(&view);njScale(&view,1,-1,-1);
        // Only the death scene installs a camera (0, 10, 35): the view is cam.mtx of
        // bhControlCamera (camera.c:28-37, cmat then T(-px, -py, -pz) with ln 0 and
        // angles 0) for the body and the effects in world space; the mesh is built
        // in cmat space and was moved by (0, 10, 35) above so the same view serves it.
        if(death)njTranslate(&view,0,-10.f,-35.f);
        image.draw_view(scene,view,focal*1.174f,focal*(float(image.height())/480.f)/(float(image.width())/640.f),false,{},2.f,20000.f);
    }
    // game.c:109-112 (Task 7, vor Task 14): bhDrawScreenFade bei fade_an > 0 auf dem einen sys->fade_*.
    // Der Fade-Quader (njDrawPolygon2D, attr 0x60, pri -0.81 -> -1, ps2_NaDraw2D.c:93-104) bekommt mit
    // njClipZ(-2, -20000) (gameover.c:244) sz = Ps2_zbuff_a + Ps2_zbuff_b / -1 > 65534, geklemmt auf 65534
    // (ps2_dummy.c:432-442), und kommt ueber Ps2AddOT (:469) ins OT-Fach 4095 (:1192-1201), das zuletzt
    // ausgegeben wird (Schleife aufsteigend, :1312). Das Netz (njDrawTriangle3D attr 0x80000062,
    // gameover.c:643, ebenfalls OT, ps2_NaGraphics3D.c:464-474) liegt bei z -40..-14 weit vor dem Klemmwert,
    // also darunter; die Aufforderung (bhDispMessageEx/bhDispFontEx, njDrawPolygon2DM pri -0.9 -> -1,
    // message.c:449/508) haengt Task 14 im selben Fach hinter den Fade (Ps2AddOT haengt an, :1203-1215).
    {const auto& fade=system_screen_fade();if(fade.visible())fade.draw(image.canvas(),image.width(),image.height());}
    if(g.md1==2){
        // bhDispMessageEx(264, 211, -0.9, 2, 0, (count << 24) | 0xE0E0E0, 0)
        const Message prompt(std::begin(cnt_mes),std::end(cnt_mes));
        ui.show_message(image,prompt,264,211,(std::uint32_t(g.count)<<24)|0xE0E0E0u);
        if(g.count>=0xFF){
            // bhDispFontEx(&cpos, 28, argb, -0.9): the choice cursor pulses.
            const unsigned a=unsigned(int(128.f*njSin(g.cur_ct))+120);
            const Message cursor{28,0xffff};
            ui.show_message(image,cursor,g.cont_x==0?248:328,241,(a<<24)|0xE0E0E0u);
        }
    }
}
}

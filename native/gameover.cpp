#include "gameover.h"
#include "asset_io.h"
#include "effect_sprites.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <algorithm>
#include <cmath>
#include <cstring>

// Game over (gameover.c semantics) as a native mechanism: the four dispatcher
// phases, the GV_WORK with its 16x16 point mesh, the ring UV table, the death
// scene, the continue prompt, the screen fade of screen.c and the exit into
// bhPopGameData or bhReturnTitle. No original procedure is linked; the small
// constant tables below are data.
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
    // screen.c fade state; `fading` is the original cb_flg bit 0x2.
    bool fading=false;float hkn=1,fade_ct=0;
    float an=0,rn=0,gn=0,bn=0,ao=0,ro=0,go=0,bo=0;
    float ap=0,rp=0,gp=0,bp=0;
    // Death scene player work.
    unsigned player_motion=0,hokan=0;std::int64_t player_frame=0;
    unsigned blasts=0,unsupported_blasts=0;
    mutable std::vector<SceneTexture> textures;
    mutable bool textures_tried=false;

    void set_fade(unsigned argb,float count){
        fading=true;fade_ct=0;
        hkn=count<=1.f?1.f:1.f+count;
        ao=an;ro=rn;go=gn;bo=bn;
        ap=float(argb>>24);rp=float((argb>>16)&0xff);gp=float((argb>>8)&0xff);bp=float(argb&0xff);
    }
    void control_fade(){
        if(hkn>1.f&&fade_ct==0){fade_ct+=1.f;return;}
        if(hkn==1.f){fading=false;an=ap;rn=rp;gn=gp;bn=bp;return;}
        const float span=hkn-1.f;
        an+=(ap-ao)/span;rn+=(rp-ro)/span;gn+=(gp-go)/span;bn+=(bp-bo)/span;
        fade_ct+=1.f;
        if(fade_ct>=span){fading=false;an=ap;rn=rp;gn=gp;bn=bp;}
    }
    std::uint32_t fade_color()const{
        auto channel=[](float v){return unsigned(std::clamp(v,0.f,255.f));};
        return (channel(an)<<24)|(channel(rn)<<16)|(channel(gn)<<8)|channel(bn);
    }
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
    switch(g.md0){
    case 1:{ // bhInitGameOver
        events.flags[12][0]|=0x100u;      // ts_flg
        events.flags[10][0]&=~0x4u;       // cb_flg
        g.ct=0;g.md0=2;g.md1=0;
        for(unsigned bit:{69u,67u,70u})events.set_flag(1,bit,1); // bhCrFlg
        events.event_deadline_mode=0;events.event_deadline_ticks=0;
        if(g.mode==3)events.flags[11][0]&=~0x80000000u;
        g.set_fade(0xFFE0E0E0,60.f);
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
                    g.set_fade(0,30.f);
                    for(unsigned slot=0;slot<audio_voice_count;++slot)audio.stop(slot,30);
                    events.flags[6][0]=0x6C;events.flags[11][0]&=~0x100100u;
                    events.flags[10][0]|=0x40000000u;
                    events.flags[5][0]|=0x5u; // st_flg 0x1 | 0x4
                    events.flags[11][0]|=0x200080u;
                    g.player_motion=PlMtnAct[g.wpntp][g.dmlvl][3];
                    g.player_frame=0;
                    break;
                }
                ++g.md1;
                g.set_fade(0,30.f);
                for(unsigned slot=0;slot<audio_voice_count;++slot)audio.stop(slot,60);
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
            if(g.ct>120&&!g.fading){
                g.set_fade(0xFF000000,30.f);
                for(unsigned slot=0;slot<audio_voice_count;++slot)audio.stop(slot,30);
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
                        g.set_fade(0xFF000000,10.f);
                        for(unsigned slot=0;slot<audio_voice_count;++slot)audio.stop(slot,10);
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
            if(!g.fading){events.flags[12][0]|=0x4000u;g.running=false;result=Result::Retry;}
            break;
        case 10:{
            events.flags[5][0]|=0x4u;
            // bhSetMotion(plp, mtn_add, 0): the interpolation window set with
            // the dying motion holds frm_no at zero for hokan_count passes.
            if(g.hokan>0)--g.hokan;
            else if(g.body&&g.body->has_motion(g.player_motion)){
                const auto frames=std::int64_t(g.body->motion_frames(g.player_motion))*65536;
                if(frames>0)g.player_frame=(g.player_frame+65536)%frames;
            }
            if(g.player_motion==25&&g.player_frame/65536==0)g.player_motion=26;
            const auto& blast=gov_etb[std::min<unsigned>(g.blasts,6)];
            if(blast.frame!=0&&unsigned(blast.frame)<=g.ct){
                audio.play_player_voice(534);
                // bhSetExplosionEffect belongs to the effect package; its
                // request is reported, never replaced by a substitute.
                set_explosion_effect({blast.x,blast.y,blast.z},{0,0,1});
                ++g.blasts;++g.unsupported_blasts;
                if(g.blasts==5){
                    g.player_motion=25;g.player_frame=0;g.hokan=8;
                    audio.play_dead_voice(int(g.character));
                    g.set_fade(0xFFFFFFFF,30.f);
                }
            }
            ++g.ct;
            if(g.ct>150){
                g.set_fade(0,30.f);
                g.ct=0;g.md1=1;
                events.flags[6][0]=0x20;
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
    g.control_fade();
    return result;
}

bool GameOver::active()const{return impl_->running;}
unsigned GameOver::phase()const{return impl_->md1;}
unsigned GameOver::count()const{return unsigned(impl_->count);}
unsigned GameOver::blasts()const{return impl_->blasts;}
unsigned GameOver::unsupported_blasts()const{return impl_->unsupported_blasts;}

namespace {
void blend_over(SoftwareRenderer& image,std::uint32_t argb){
    const unsigned a=(argb>>24)&255;if(!a)return;
    const unsigned r=(argb>>16)&255,g=(argb>>8)&255,b=argb&255;
    for(auto& pixel:image.canvas()){
        const unsigned dr=(pixel>>16)&255,dg=(pixel>>8)&255,db=pixel&255;
        pixel=0xff000000u|((r*a+dr*(255-a))/255)<<16|((g*a+dg*(255-a))/255)<<8|((b*a+db*(255-a))/255);
    }
}
}

void GameOver::draw(SoftwareRenderer& image,const GameUi& ui)const{
    const auto& g=*impl_;
    if(!g.running)return;
    if(g.md1!=0){
        // njSetBackColor(0,0,0) plus njSetMatrix(NULL, cmat): the mesh is built
        // in camera space, so the view is the base matrix alone. In the death
        // scene the camera sits at (0, 10, 35) and the mesh moves with it.
        const bool death=g.md1==10;
        Scene scene;scene.background=0xff000000u;
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
                t.color=0xffffffffu;t.textured=textured;t.texture=0;t.camera_mask=false;
                t.blend=SceneBlend::Alpha;
                scene.triangles.push_back(t);
            }
        }
        if(death&&g.body&&g.body->has_motion(g.player_motion)){
            // bhCalcModel on the player work: the body stands at the origin with
            // the original yaw while the blasts pass.
            auto model=g.body->sample(g.player_motion,g.player_frame,{0,0,0},35499);
            const auto base=scene.textures.size();
            scene.textures.insert(scene.textures.end(),model.textures.begin(),model.textures.end());
            for(auto& t:model.triangles){
                if(t.textured)t.texture=std::uint16_t(base+t.texture);
                t.camera_mask=false;
                for(auto& v:t.vertices){v.position.y+=10.f;v.position.z+=35.f;}
                scene.triangles.push_back(t);
            }
        }
        const float focal=(float(image.width())*0.5f)/std::tan(0.0000958738f*(kPerspective/2));
        NJS_MATRIX view;njUnitMatrix(&view);njScale(&view,1,-1,-1);
        // Only the death scene installs a camera (0, 10, 35); the mesh is built
        // in camera space and is moved with it so the base matrix still applies.
        if(death)njTranslate(&view,0,-10.f,-35.f);
        image.draw_view(scene,view,focal*1.174f,focal*(float(image.height())/480.f)/(float(image.width())/640.f),false,{},2.f,20000.f);
    }
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
    blend_over(image,g.fade_color());
}
}

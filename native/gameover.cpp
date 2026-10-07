#include "gameover.h"
#include "asset_io.h"
#include "effect_sprites.h"
#include "chunk_blend.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace recvx {
namespace {
struct NJS_POINT2 {float x,y;};
union NJS_COLOR {unsigned color;struct {short u,v;} tex;};
struct NJS_POINT3COL {NJS_POINT3* p;NJS_COLOR* col;NJS_COLOR* tex;int num;};
struct EXP_WORK {unsigned wpntp,dmlvl;};
struct Object {unsigned flg=0;};
struct Model {void* texP=nullptr;Object owP[32];};
struct Player {unsigned stflg=0;float px=0,py=0,pz=0;int ay=0,mtn_no=0,hokan_rate=0,hokan_count=0,mtn_add=65536,frm_no=0;void* mnwP=nullptr;void* mnwPb=nullptr;void* txp[1]{};EXP_WORK* exp0=nullptr;Model* mlwP=nullptr;};
struct System {unsigned ts_flg=0,cb_flg=0,gm_flg=0,st_flg=0,sp_flg=0,pt_flg=0,ev_flg[32]{},pad_ps=0;unsigned gov_md0=0,gov_md1=0,gov_ct=0,evt_tmd=0,evt_tim=0,ply_id=0;int gm_mode=0;void* gov_exp=nullptr;float fade_pbk=0,fade_ct=0,fade_hkn=0,fade_an=0,fade_rn=0,fade_gn=0,fade_bn=0,fade_ao=0,fade_ro=0,fade_go=0,fade_bo=0,fade_ap=0,fade_rp=0,fade_gp=0,fade_bp=0;int ef_tlist=0;const unsigned short* mes_tp=nullptr;struct{unsigned mdflg=0;}obwp[2];};
struct Camera {unsigned flg;int pers,ln,ax,ay,az,axp,ayp,azp;float px,py,pz,pxp,pyp,pzp;};
struct Frame {bool black=false,mesh=false,text=false,cursor=false,clear_effects=false;int result=0,fade_ticks=0,stop_ticks=-1,dead_voice=-1,action_sound=-1;unsigned fade_color=0,text_color=0,cursor_color=0;float text_x=0,text_y=0,cursor_x=0,cursor_y=0;std::vector<unsigned> sounds;std::vector<NJS_POINT3> explosions;};
System* sys;Player* plp;Camera cam;Frame* output;void* cmat=nullptr;


void* bhGetFreeMemory(unsigned,unsigned);
void bhCrFlg(unsigned* flags,unsigned bit){flags[bit/32]&=~(0x80000000u>>(bit%32));}
int bhCkFlg(unsigned* flags,unsigned bit){return (flags[bit/32]&(0x80000000u>>(bit%32)))!=0;}

void RequestAllStopSoundEx(int,int,int ticks){output->stop_ticks=ticks;}
void CallSystemSe(int,unsigned sound){output->sounds.push_back(sound);}
void CallPlayerDeadVoice(int player){output->dead_voice=player;}
void CallPlayerActionSe(int sound,int){output->action_sound=sound;}
void bhSetExplosionEffect(NJS_POINT3* p){output->explosions.push_back(*p);}
void njSetBackColor(unsigned,unsigned,unsigned){output->black=true;}
void bhClearEffect(){output->clear_effects=true;}
void bhReturnTitle(){output->result=2;}
void bhPopGameData(){output->result=1;}
void njFogDisable(){}
void njClipZ(float,float){}
void njSetPerspective(int){}
void njSetMatrix(void*,void*){}
void njTextureFilterMode(int){}
void njColorBlendingMode(int,int){}
void njSetTexture(void*){}
void njSetTextureNum(int){}
const PlayerAsset* motion_body=nullptr;
void bhSetMotion(Player* p,int add,int,void*){p->frm_no+=add;if(motion_body&&motion_body->has_motion(p->mtn_no)){const int frames=int(motion_body->motion_frames(p->mtn_no))*65536;if(frames>0)p->frm_no%=frames;}}
void bhCalcModel(Player*){}
void bhDispMessageEx(float x,float y,float,int,int,unsigned color,int){output->text=true;output->text_x=x;output->text_y=y;output->text_color=color;}
void bhDispFontEx(NJS_POINT2* p,int,unsigned color,float){output->cursor=true;output->cursor_x=p->x;output->cursor_y=p->y;output->cursor_color=color;}
void njDrawTriangle3D(NJS_POINT3COL*,int,unsigned){output->mesh=true;}
#include "gameover.inc"
GV_WORK* allocated;
void* bhGetFreeMemory(unsigned,unsigned){return allocated;}
}
struct GameOver::Impl {System system;GV_WORK work{};Player player;EXP_WORK exp{};Model model;Frame frame;bool running=false,black=false;unsigned fade_color=0;float fade=0,target=0,step=0;SceneTexture texture;std::vector<SceneTexture> common;std::vector<EffectSprite> sprites;const PlayerAsset* body=nullptr;Camera camera{};};
GameOver::GameOver(const std::filesystem::path& data,const std::filesystem::path& mods):impl_(std::make_unique<Impl>()){
    if(data.empty())return;
    const auto bytes=read_asset_file(data/"archives/SYSTEM/0001.bin",8*1024*1024);auto textures=decode_texture_bundle(bytes,common_effect_textures_offset(bytes));
    if(textures.size()<=4)throw std::runtime_error("original game-over texture 4 missing");Scene scene;scene.textures=std::move(textures);apply_texture_files(scene,data,mods,"effects/common");impl_->common=std::move(scene.textures);impl_->texture=impl_->common[4];
    for(auto& texture:impl_->common)for(auto& pixel:texture.pixels)pixel=(pixel&0xffffff)|(std::min(255u,(pixel>>24)*2)<<24);
    impl_->texture=impl_->common[4];
}
GameOver::~GameOver()=default;
void GameOver::begin(EventVm& events,unsigned character,int mode,unsigned weapon,unsigned level,const PlayerAsset* body){
    auto& i=*impl_;i.system={};i.work={};i.frame={};i.running=true;i.black=false;i.fade=0;i.target=0;i.step=0;
    i.system.ply_id=character;i.system.gm_mode=mode;i.system.gov_md0=0;i.system.gov_md1=0;
    i.player={};i.body=body;i.sprites.clear();i.camera={};i.exp={std::min(1u,weapon),std::min(2u,level)};i.player.exp0=&i.exp;i.player.mlwP=&i.model;
    i.system.cb_flg=events.flags[10][0];i.system.gm_flg=events.flags[11][0];i.system.ts_flg=events.flags[12][0];i.system.st_flg=events.flags[5][0];i.system.sp_flg=events.flags[6][0];std::copy(events.flags[1].begin(),events.flags[1].end(),i.system.ev_flg);

}
GameOver::Result GameOver::tick(unsigned pressed,EventVm& events,AudioMixer& audio){
    auto& i=*impl_;if(!i.running)return Result::Running;
    sys=&i.system;plp=&i.player;allocated=&i.work;cam=i.camera;motion_body=i.body;i.frame={};output=&i.frame;
    sys->pad_ps=pressed;bhControlGameOver();i.camera=cam;
    bhControlScreenFade();
    i.fade=std::clamp(sys->fade_an/255.f,0.f,1.f);i.fade_color=(unsigned(sys->fade_rn)<<16)|(unsigned(sys->fade_gn)<<8)|unsigned(sys->fade_bn);
    i.black|=i.frame.black;
    if(i.frame.clear_effects)i.sprites.clear();
    if(i.frame.action_sound>=0)audio.play_room_sound((unsigned(i.frame.action_sound)>>8)&15,unsigned(i.frame.action_sound)&255,6);
    set_effect_context({sys->st_flg,sys->pt_flg,0,0,0,sys->cb_flg,sys->sp_flg});set_effect_walls(nullptr,0);
    for(const auto& p:i.frame.explosions)set_explosion_effect(p,{0,0,1});
    auto emitted=take_effect_output();for(const auto& e:emitted.spawns){EffectSprite sprite;initialize_effect_sprite(sprite,e);i.sprites.push_back(std::move(sprite));}
    for(auto& sprite:i.sprites)if(sprite.flg)step_effect_sprite(sprite);
    emitted=take_effect_output();for(const auto& e:emitted.spawns){EffectSprite sprite;initialize_effect_sprite(sprite,e);i.sprites.push_back(std::move(sprite));}
    i.sprites.erase(std::remove_if(i.sprites.begin(),i.sprites.end(),[](const auto& s){return !s.flg;}),i.sprites.end());
    if(i.frame.stop_ticks>=0)for(unsigned slot=0;slot<audio_voice_count;++slot)audio.stop(slot,unsigned(i.frame.stop_ticks));
    for(auto sound:i.frame.sounds)audio.play_system(sound);
    if(i.frame.dead_voice>=0)audio.play(0,"ADV",unsigned(i.frame.dead_voice+8),AudioMixer::adx_gain(-45),false,AudioMixer::adx_fade_ticks(2));
    events.flags[10][0]=sys->cb_flg;events.flags[11][0]=sys->gm_flg;events.flags[12][0]=sys->ts_flg;events.flags[5][0]=sys->st_flg;events.flags[6][0]=sys->sp_flg;std::copy(sys->ev_flg,sys->ev_flg+32,events.flags[1].begin());events.event_timer=sys->evt_tim;
    if(i.frame.result)i.running=false;
    return i.frame.result==1?Result::Retry:i.frame.result==2?Result::Title:Result::Running;
}
void GameOver::draw(SoftwareRenderer& image,const GameUi& ui)const{
    const auto& i=*impl_;if(i.black)std::fill(image.canvas().begin(),image.canvas().end(),0xff000000);
    if(i.frame.mesh){Scene scene;scene.background=0xff000000;scene.textures=i.common;
        SceneCamera camera;camera.position={0,0,0};camera.angles={0,0,0};camera.distance=0;camera.perspective=11832;camera.clip_near=2;camera.clip_far=20000;
        if(i.system.gov_md1==10){camera.position={i.camera.px,i.camera.py,i.camera.pz};camera.angles={i.camera.ax,i.camera.ay,i.camera.az};
            if(i.body&&i.body->has_motion(unsigned(i.player.mtn_no))){const auto base=unsigned(scene.textures.size());scene.textures.insert(scene.textures.end(),i.body->textures().begin(),i.body->textures().end());append_actor(scene,i.body->sample(unsigned(i.player.mtn_no),i.player.frm_no,{i.player.px,i.player.py,i.player.pz},i.player.ay),base);}
            std::map<unsigned,unsigned> bases;const auto categories=common_effect_categories();for(unsigned n=0;n<categories.size();++n)bases[n]=categories[n].first;
            for(const auto& sprite:i.sprites){if(!sprite.quads.empty())append_custom_effect(scene,sprite,camera,bases);else if(sprite.tex_id>=0&&unsigned(sprite.tex_id)<categories.size())append_effect_sprite(scene,sprite,{sprite.px,sprite.py,sprite.pz},{sprite.sx,sprite.sy,sprite.sz},camera,categories[unsigned(sprite.tex_id)].first+sprite.ani_ct);}}

        for(unsigned y=0;y<15;++y){std::array<SceneVertex,32> vertices;for(unsigned x=0;x<16;++x)for(unsigned row=0;row<2;++row){const unsigned n=x+(y+row)*16;const auto& p=i.work.pt[n];vertices[2*x+row]={p,float(i.work.uv[n].tex.u)/256.f,float(i.work.uv[n].tex.v)/256.f,{}};}
            for(unsigned n=0;n<30;++n){SceneTriangle t;t.vertices=n&1?std::array<SceneVertex,3>{vertices[n+1],vertices[n],vertices[n+2]}:std::array<SceneVertex,3>{vertices[n],vertices[n+1],vertices[n+2]};t.texture=4;t.textured=true;t.color=0xffffffff;t.camera_mask=false;t.world_lit=false;t.depth_write=false;t.blend=SceneBlend::Ninja;t.blend_register=ninja_blend_register(8,6);scene.triangles.push_back(t);}}
        image.draw(scene,camera);}
    if(i.frame.text)ui.show_gameover_message(image,Message(cnt_mes,cnt_mes+17),int(i.frame.text_x),int(i.frame.text_y),i.frame.text_color);
    if(i.frame.cursor)ui.show_gameover_message(image,Message{28,0xffff},int(i.frame.cursor_x),int(i.frame.cursor_y),i.frame.cursor_color);
    if(i.fade>0)for(auto& pixel:image.canvas()){unsigned out=0xff000000;for(unsigned shift:{0u,8u,16u})out|=unsigned(((pixel>>shift)&255)*(1-i.fade)+((i.fade_color>>shift)&255)*i.fade)<<shift;pixel=out;}
}
bool GameOver::active()const{return impl_->running;}
unsigned GameOver::phase()const{return impl_->system.gov_md1;}
unsigned GameOver::count()const{return impl_->system.gov_ct;}
}

#include "effect_sprites.h"
#include "chunk_blend.h"
#include "effect_collision.h"
#include <stdexcept>
namespace recvx {
namespace {
using O_WRK=EffectSprite;
struct EFF5UV {int u,v;};
struct EFF5UVTBL {EFF5UV* pTblTop;int lTblMax,lTexID,lAniCt,pad,w,h;};
struct EF_WORK {unsigned flg;unsigned short id,type;short flr_no;unsigned short mdlver;float px,py,pz,sx,sy,sz;short ay,ax;};
struct OriginalSpriteQueue {unsigned ef_trsn=0,ef_pncn=0;O_WRK* ef_trs[512]{};O_WRK* ef_pnc[512]{};unsigned st_flg=0,pt_flg=1,cb_flg=0,sp_flg=8,ef_fncn=0;O_WRK* ef_fnc[512]{};int ef_tlist=0;int ef_tn[512]{};EF_WORK ef{};} queue;
auto* sys=&queue;
struct OriginalPlayer {unsigned flg=0,stflg=0,mdflg=0;} player_work;
auto* plp=&player_work;
struct UV_WORK {float u,v,xs,ys;};
struct LGT_WK {unsigned flg,type;int aspd,lkflg,lkno,lkono,lsrc;float px,py,pz,lx,ly,lz,vx,vy,vz,spc,dif,amb,r,g,b,nr,fr;int iang,oang,ax,ay,az;};
// The platform libc random sequence is not matched yet. It only seeds atlas phase.
int effect_random(){static unsigned state=1;state=state*1103515245+12345;return int((state>>1)&0x7fffffff);}
struct POINT {float px,py,pz,ox,oy,oz;};
// bhSetEffect/bhSetEffectTb are requests the host fulfils from its effect slots; only effects with a bound procedure start.
EffectOutput pending;
bool effect_bound(unsigned id);
int bhSetEffectTb(EF_WORK* ef,NJS_POINT3* offset,unsigned char*,int lkono){
    if(!effect_bound(ef->id)){pending.unsupported.insert(ef->id);return -1;}
    EffectSpawn spawn;spawn.flg=ef->flg;spawn.id=ef->id;spawn.type=ef->type;spawn.flr_no=ef->flr_no;spawn.mdlver=ef->mdlver;spawn.position={ef->px,ef->py,ef->pz};spawn.scale={ef->sx,ef->sy,ef->sz};spawn.ax=ef->ax;spawn.ay=ef->ay;spawn.lkono=lkono;
    if(offset){spawn.has_offset=true;spawn.offset=*offset;}pending.spawns.push_back(spawn);return int(pending.spawns.size())-1;
}
int bhSetEffect(int effno,POINT* point,unsigned char*,int lkono){
    if(!effect_bound(unsigned(effno))){pending.unsupported.insert(unsigned(effno));return -1;}
    EffectSpawn spawn;spawn.id=unsigned(effno);spawn.position={point->px,point->py,point->pz};spawn.has_offset=true;spawn.offset={point->ox,point->oy,point->oz};spawn.lkono=lkono;pending.spawns.push_back(spawn);return int(pending.spawns.size())-1;
}
void CallPlayerActionSe(int se,int){pending.sounds.push_back(unsigned(se));}
void bhCheckBombAtari(NJS_POINT3* position,float radius,int dmax,int dmin){pending.bombs.push_back({*position,radius,dmax,dmin});}
// rom->lgtp[0] is the effect light slot bhSetExplosion fills; cam is the view direction passed to set_explosion.
struct EffectLight {unsigned flg,type;int aspd,lkflg,lkno,lkono,mode,lsrc;float r,g,b,nr,fr,px,py,pz,lx,ly,lz;int ct0;void* light;} effect_light{};
using LGT_WORK=EffectLight;
struct {EffectLight* lgtp=&effect_light;} room_state;
auto* rom=&room_state;
NJS_MATRIX draw_camera;
struct {float vx=0,vy=0,vz=1;NJS_MATRIX* mtx=&draw_camera;} camera_state;
auto& cam=camera_state;
using NJS_TEXTURE_VTX=EffectSprite::Vertex;
struct EXP_DRAW022_WORK {unsigned short flg;short ct;float px,py,pz;};
struct EXP_EFF025_WORK {unsigned short flg;short ct;float scl,px,py,pz,xn,yn,zn;};
using ATR_WORK=CollisionWall;
const std::vector<CollisionWall>* effect_walls=nullptr;float effect_room_height=0;CollisionWall wall_hit;
O_WRK* drawing=nullptr;unsigned draw_texture=0,draw_source=8,draw_destination=6;
void* bhSetExtraEffectWork(){drawing->extra=std::make_shared<std::array<unsigned,312>>();(*drawing->extra)[0]=2;return drawing->extra->data();}
ATR_WORK* bhCheckWallType(NJS_POINT3* position,unsigned flags,float radius,float height){if(!effect_walls)return nullptr;const auto hit=effect_wall(*effect_walls,effect_room_height,*position,flags,radius,height);if(!hit)return nullptr;wall_hit=*hit;return &wall_hit;}
void bhSetWallRefAngle(ATR_WORK* wall,NJS_POINT3* position,float radius,float height,int* angle){reflect_effect_wall(*wall,effect_room_height,*position,radius,height,*angle);}
void njSetTexture(void*){}
void njSetTextureNum(int number){draw_texture=unsigned(number);}
void njControl3D(int){}
void njTextureFilterMode(int){}
void njColorBlendingMode(int which,int value){(which?draw_destination:draw_source)=unsigned(value);}
void njTranslateEx(NJS_VECTOR* p){njTranslateV(nullptr,p);}
void njScaleEx(NJS_VECTOR* p){njScaleV(nullptr,p);}
void njDrawTexture3DEx(NJS_TEXTURE_VTX* vertices,int count,int){
    if(!drawing||count!=4)return;NJS_MATRIX matrix;njGetMatrix(&matrix);EffectSprite::Quad q;q.center={matrix[12],matrix[13],matrix[14]};q.category=draw_texture;q.source=draw_source;q.destination=draw_destination;
    for(unsigned n=0;n<4;++n){q.vertices[n]=vertices[n];q.vertices[n].x*=matrix[0];q.vertices[n].y*=matrix[5];q.vertices[n].z*=matrix[10];}drawing->quads.push_back(q);
}
void bhDraw024(O_WRK*);void bhDraw025(O_WRK*);
#include "effect_sprites.inc"
const LGT_WK& lighter_light_table(){return lighter_light;}
}
void set_effect_context(const EffectContext& c){queue.st_flg=c.st_flg;queue.pt_flg=c.pt_flg;queue.cb_flg=c.cb_flg;queue.sp_flg=c.sp_flg;player_work={c.player_flg,c.player_stflg,c.player_mdflg};}
LighterSprite lighter_sprite(){
    // bhSetEffectTb: copy flg/id/type and the scale; the procedure initializes the rest.
    LighterSprite out;out.sprite.flg=lighter_effect.flg;out.sprite.id=lighter_effect.id;out.sprite.type=lighter_effect.type;out.sprite.ax=lighter_effect.ax;
    out.scale={lighter_effect.sx,lighter_effect.sy,lighter_effect.sz};return out;
}
OriginalLight lighter_light(){const auto& l=lighter_light_table();return {l.flg,l.type,l.aspd,l.lkflg,l.lkno,l.lkono,l.lsrc,{l.px,l.py,l.pz},{l.lx,l.ly,l.lz},{l.vx,l.vy,l.vz},l.spc,l.dif,l.amb,{l.r,l.g,l.b},l.nr,l.fr};}
std::size_t common_effect_textures_offset(const std::vector<std::uint8_t>& data){
    auto u32=[&](std::size_t at){if(at+4>data.size())throw std::runtime_error("effect data truncated");return std::uint32_t(data[at])|data[at+1]<<8|data[at+2]<<16|std::uint32_t(data[at+3])<<24;};
    std::size_t dp=4;std::uint32_t sz=u32(0);
    for(const int flg:effect_info_flags){if(flg==-1)break;if(flg&1){if(sz>data.size()-dp)throw std::runtime_error("effect model outside data");dp+=sz;sz=u32(dp);dp+=4;}}
    return (dp+31)&~std::size_t(31);
}
std::vector<std::pair<unsigned,unsigned>> common_effect_categories(){std::vector<std::pair<unsigned,unsigned>> out;unsigned first=0;for(std::size_t i=0;effect_info_flags[i]!=-1;++i){out.push_back({first,unsigned(effect_info_anims[i])});first+=unsigned(effect_info_anims[i]);}return out;}
namespace {bool effect_bound(unsigned id){return id==24||id==25||id==5||id==9||id==16||id==23||id==122||id==200||id==201||id==218||id==230||id==231||id==232;}}
bool supported_effect_sprite(unsigned id){return effect_bound(id);}
bool step_effect_sprite(EffectSprite& sprite,bool* queued){queue.ef_trsn=0;queue.ef_pncn=0;queue.ef_fncn=0;drawing=&sprite;sprite.quads.clear();if(sprite.id==24)bhEff024(&sprite);else if(sprite.id==25)bhEff025(&sprite);else if(sprite.id==5)bhEff005(&sprite);else if(sprite.id==16)bhEff016(&sprite);else if(sprite.id==23)bhEff023(&sprite);else if(sprite.id==9)bhEff009(&sprite);else if(sprite.id==122)bhEff122(&sprite);else if(sprite.id==200)bhEff200(&sprite);else if(sprite.id==201)bhEff201(&sprite);else if(sprite.id==218)bhEff218(&sprite);else if(sprite.id==230)bhEff230(&sprite);else if(sprite.id==231)bhEff231(&sprite);else if(sprite.id==232)bhEff232(&sprite);else return false;if(queue.ef_fncn&&sprite.flg){njUnitMatrix(&draw_camera);for(unsigned i=0;i<512;++i)queue.ef_tn[i]=int(i);if(sprite.id==24)bhDraw024(&sprite);else if(sprite.id==25)bhDraw025(&sprite);}
    drawing=nullptr;if(queued)*queued=queue.ef_trsn>0||queue.ef_pncn>0||queue.ef_fncn>0;return true;}
// bhSetEffectTb: copy flg/id/type/mdlver/flr_no, position, scale (also kept in sxb..), angles and the joint offset.
void initialize_effect_sprite(EffectSprite& s,const EffectSpawn& e){
    s=EffectSprite{};s.flg=e.flg;s.id=e.id;s.type=e.type;s.tex_id=-1;s.mdlver=e.mdlver;s.flr_no=e.flr_no;s.px=e.position.x;s.py=e.position.y;s.pz=e.position.z;
    s.sx=s.sxb=e.scale.x;s.sy=s.syb=e.scale.y;s.sz=s.szb=e.scale.z;s.ax=e.ax;s.ay=e.ay;s.az=0;s.lkono=e.lkono;
    if(e.has_offset){s.lox=e.offset.x;s.loy=e.offset.y;s.loz=e.offset.z;}
}
void set_explosion(NJS_POINT3 position,NJS_POINT3 view){
    camera_state={view.x,view.y,view.z};effect_light=EffectLight{};bhSetExplosion(&position);
    // rom->lgtp[0] after bhSetExplosion: exported through the output (the host installs it in its light slot 0).
    OriginalLight light{effect_light.flg,effect_light.type,effect_light.aspd,effect_light.lkflg,effect_light.lkno,effect_light.lkono,effect_light.lsrc,{effect_light.px,effect_light.py,effect_light.pz},{0,0,0},{effect_light.lx,effect_light.ly,effect_light.lz},0,0,0,{effect_light.r,effect_light.g,effect_light.b},effect_light.nr,effect_light.fr};
    pending.light=light;
}
void set_effect_walls(const std::vector<CollisionWall>* walls,float height){effect_walls=walls;effect_room_height=height;}
void set_explosion_effect(NJS_POINT3 position,NJS_POINT3 view){camera_state.vx=view.x;camera_state.vy=view.y;camera_state.vz=view.z;bhSetExplosionEffect(&position);}
void append_custom_effect(Scene& scene,const EffectSprite& sprite,const SceneCamera& camera,const std::map<unsigned,unsigned>& bases){for(const auto& q:sprite.quads){const auto base=bases.find(q.category);if(base==bases.end())throw std::runtime_error("original custom-effect texture missing");EffectSprite quad;quad.id=302;quad.flg=0x100000;quad.tv=q.vertices;quad.bl_src=int(q.source);quad.bl_dst=int(q.destination);append_effect_sprite(scene,quad,q.center,{1,1,1},camera,base->second);}}
EffectOutput take_effect_output(){EffectOutput out=std::move(pending);pending=EffectOutput{};return out;}
void append_effect_sprite(Scene& scene,const EffectSprite& s,NJS_POINT3 position,NJS_POINT3 scale,const SceneCamera& camera,unsigned texture){
    if(s.flg&0x1000000)return;
    // bhDrawTrsEffect3D: translation, UnitRotPortion for flag 0x100000,
    // Z rotation, scale. Invert the room view rotation to place the billboard.
    NJS_MATRIX basis;njUnitMatrix(&basis);
    if(s.flg&0x100000){njRotateY(&basis,-camera.angles[1]);njRotateX(&basis,-camera.angles[0]);njRotateZ(&basis,-camera.angles[2]);njScale(&basis,1,-1,-1);njRotateZ(&basis,s.az);}
    else njRotateXYZ(&basis,s.ax,s.ay,s.az);
    std::array<SceneVertex,4> vertices;
    for(unsigned i=0;i<4;++i){NJS_POINT3 point{s.tv[i].x*scale.x,s.tv[i].y*scale.y,s.tv[i].z*scale.z};njCalcVector(&basis,&point,&point);point.x+=position.x;point.y+=position.y;point.z+=position.z;vertices[i]={point,s.tv[i].u,s.tv[i].v,{}};}
    for(auto indices:{std::array<unsigned,3>{0,1,2},std::array<unsigned,3>{1,3,2}}){SceneTriangle triangle;for(unsigned i=0;i<3;++i)triangle.vertices[i]=vertices[indices[i]];triangle.color=s.tv[0].col;triangle.texture=texture;triangle.textured=true;triangle.camera_mask=false;triangle.world_lit=true;triangle.depth_write=false;
        // Effects bound with their original njColorBlendingMode pair (hit blood, explosion). The older atlas sprites keep the
        // additive approximation they were verified with until their pairs are checked against the original look.
        if(s.id==5||s.id==23||s.id==302){triangle.blend=SceneBlend::Ninja;triangle.blend_register=ninja_blend_register(unsigned(s.bl_src),unsigned(s.bl_dst));}else triangle.blend=SceneBlend::Additive;scene.triangles.push_back(triangle);}
}
}

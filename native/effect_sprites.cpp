#include "effect_sprites.h"
#include "chunk_blend.h"
#include <stdexcept>
namespace recvx {
// Host-Typen der Effektverknüpfung; EffectSprite hält nur Zeiger darauf.
struct EffectLinkNode {NJS_MATRIX mtx{};};
struct EffectModelWork {EffectLinkNode* owP=nullptr;};
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Alle Effektprozeduren aus effsub1.c /
// effsub5.c / effect.c (bhEff000, 005, 009, 015, 016, 023, 024, 025, 029, 122,
// 200, 201, 218, 230, 231, 232, bhDraw024/025, bhEff_Sub356, bhSetExplosion,
// bhSetExplosionEffect) und die zugehörigen Originaldaten (die ef_info-Tabelle
// mit Flags und Animationslängen, WpnEffTab[1][0], lgttab[1], die EFF5UV-
// Atlastabellen) kamen aus effect_sprites.inc, erzeugt von
// tools/generate_effect_sprites.py. Generator und Include sind Quarantäne
// (archived/FORBIDDEN_DONTUSE/) und dürfen weder gelesen noch ausgeführt
// werden. Mit den Prozeduren entfällt auch die Host-Brücke, die nur für sie
// existierte (das sys-Teilwerk mit ef_trs/ef_pnc/ef_pol/ef_fnc, der
// Spieler-Teilsatz, die ATR-Umschrift, die Wand-/Boden-/Wasserdienste,
// bhSetEffectTb/bhSetEffect als Slotanforderung, CallYakkyouSe).
//
// Zu leisten: die Effektprozeduren, ihre UV-Atlanten und die ef_info-Tabelle
// als eigenen nativen Mechanismus bestimmen — nicht über einen Generator auf
// die Originalquellen.
//
// Solange das offen ist:
// - step_effect_sprite führt nichts aus und meldet mit false, dass für diese
//   Effekt-ID keine Prozedur gebunden ist; supported_effect_sprite ist für
//   jede ID false.
// - set_explosion / set_explosion_effect fordern nichts an; take_effect_output
//   bleibt leer (keine Spawns, keine Töne, keine Bombenanfrage, kein Licht).
// - lighter_sprite / lighter_light liefern leere Originaldatensätze, weil
//   WpnEffTab/lgttab fehlen. flg bleibt 0, der Host startet also keinen Effekt
//   und das Raumlicht 1 trägt nichts bei. Keine geratenen Skalen oder Farben.
// - common_effect_categories ist leer, und common_effect_textures_offset
//   liefert std::size_t(-1) als dokumentiertes "nicht bestimmbar": ohne die
//   ef_info-Flags lässt sich die Kette der Modellblöcke in SYSTEM/0001 nicht
//   überspringen. Aufrufer müssen diesen Wert als "keine gemeinsamen
//   Effekttexturen" behandeln und das Laden überspringen; ein geratener Offset
//   würde die PVP-Liste falsch dekodieren.
// - set_effect_context / set_effect_walls / set_effect_ground /
//   set_effect_link_matrix haben keinen Abnehmer mehr und verwerfen ihre
//   Eingaben.
//
// Erhalten bleibt ausschließlich Host-Code, der nicht aus dem Generator kam:
// initialize_effect_sprite (die Feldübernahme einer EffectSpawn-Anfrage),
// original_effect_slot (die Slotwahl über flg & 3) und der Zeichenpfad
// append_effect_sprite / append_custom_effect.
//
// Sichtbare Folge: Es werden keine Effekte mehr dargestellt oder gestartet —
// keine Feuerzeugflamme, keine Explosion mit Trümmern und Rauch, keine
// Mündungs-/Treffereffekte, keine Flammen auf Rodrigos Tisch, keine
// Metalldetektor-Lampen. Raumskripte, die Effekte anfordern, laufen weiter,
// bewirken aber nichts.
namespace {
EffectOutput pending;
}
void set_effect_context(const EffectContext& context){(void)context;}
LighterSprite lighter_sprite(){return {};}
OriginalLight lighter_light(){return {};}
std::size_t common_effect_textures_offset(const std::vector<std::uint8_t>& data){
    (void)data;return std::size_t(-1);
}
std::vector<std::pair<unsigned,unsigned>> common_effect_categories(){return {};}
bool supported_effect_sprite(unsigned id){(void)id;return false;}
bool step_effect_sprite(EffectSprite& sprite,bool* queued){
    (void)sprite;if(queued)*queued=false;return false;
}
void set_effect_walls(const std::vector<CollisionWall>* walls,float height){(void)walls;(void)height;}
void set_effect_ground(GroundCollision* ground,const std::vector<CollisionWall>* room,const std::vector<CollisionWall>* mobile){(void)ground;(void)room;(void)mobile;}
void set_effect_link_matrix(unsigned node,const float* matrix){
    // Die Knotenmatrix hat ohne Effektprozeduren keinen Abnehmer.
    (void)node;(void)matrix;
}
void set_explosion(NJS_POINT3 position,NJS_POINT3 view){(void)position;(void)view;}
void set_explosion_effect(NJS_POINT3 position,NJS_POINT3 view){(void)position;(void)view;}
EffectOutput take_effect_output(){EffectOutput out=std::move(pending);pending=EffectOutput{};return out;}
// bhSetEffectTb: copy flg/id/type/mdlver/flr_no, position, scale (also kept in sxb..), angles and the joint offset.
void initialize_effect_sprite(EffectSprite& s,const EffectSpawn& e){
    s=EffectSprite{};s.flg=e.flg;s.id=e.id;s.type=e.type;s.tex_id=-1;s.mdlver=e.mdlver;s.flr_no=e.flr_no;s.px=e.position.x;s.py=e.position.y;s.pz=e.position.z;
    s.sx=s.sxb=e.scale.x;s.sy=s.syb=e.scale.y;s.sz=s.szb=e.scale.z;s.ax=e.ax;s.ay=e.ay;s.az=0;s.lkono=e.lkono;
    s.lkwkp=e.linked_work;
    if(e.has_offset){s.lox=e.offset.x;s.loy=e.offset.y;s.loz=e.offset.z;}
}
// effect.c:bhSetEffectTb scans the shared eff[512] pool and accepts the first
// slot whose flg has neither allocation bit set. Missing tail entries are zeroed.
int original_effect_slot(const std::vector<unsigned>& flags){for(unsigned i=0;i<512;++i)if(i>=flags.size()||!(flags[i]&3u))return int(i);return -1;}
void append_custom_effect(Scene& scene,const EffectSprite& sprite,const SceneCamera& camera,const std::map<unsigned,unsigned>& bases){for(const auto& q:sprite.quads){const auto base=bases.find(q.category);if(base==bases.end())throw std::runtime_error("original custom-effect texture missing");EffectSprite quad;quad.id=302;quad.flg=0x100000;quad.tv=q.vertices;quad.bl_src=int(q.source);quad.bl_dst=int(q.destination);append_effect_sprite(scene,quad,q.center,{1,1,1},camera,base->second);}}
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

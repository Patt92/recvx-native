/* Komponentenpruefung der nativen Partikelarbeiten (effsub3.c: bhSetEffParticle,
   bhSetEffParticleMk2, bhEff302, bhEff_PtclSpriteDrawB, bhEff_PtclLineDraw,
   bhSetBloodPoolLnk, bhEff300). Alle Erwartungen sind aus den Originaltabellen
   PtclTbl/PtclDat (effsub3.c:810-908), Eff302Prm (effsub3.c:29-48), bhEff300
   (effsub3.c:234-401) und dem AllocOwork-Verhalten abgeleitet, nicht aus dem
   nativen Code. */
#include "effect_particles.h"
#include "enemies/enemy_work.h"
#include "core_random.h"
#include "chunk_blend.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "scene.h"
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
using namespace recvx;

static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}

/* Original-BP_WORK (types.h:2210, 0x40 Byte) - die Vorlage, die spawn_pool liest. */
struct BpWork{NJS_POINT3 off_pos;int srd_dir;float srd_pos;float bld_spd;float srt_spd[5];int srt_dir[5];};
static_assert(sizeof(BpWork)==0x40,"BP_WORK size differs");
static_assert(offsetof(BpWork,srd_dir)==0x0C,"BP_WORK srd_dir offset differs");
static_assert(offsetof(BpWork,srd_pos)==0x10,"BP_WORK srd_pos offset differs");
static_assert(offsetof(BpWork,bld_spd)==0x14,"BP_WORK bld_spd offset differs");
static_assert(offsetof(BpWork,srt_spd)==0x18,"BP_WORK srt_spd offset differs");
static_assert(offsetof(BpWork,srt_dir)==0x2C,"BP_WORK srt_dir offset differs");

static NJS_POINT3 at_rest(const ParticleLink&,NJS_POINT3 offset){return offset;}
static bool no_wall(NJS_POINT3){return false;}

static void seed(){recvx_core_random_seed(0x12345678u);}

/* PtclTbl[0] = { 2, PtclDat00 } mit PtclDat00 = {{0,255,1},{0,224,1}} und
   Eff302Prm[0] = {1.0, -0.08, 20.0, 0.5, 0.5, 0.025, 16, {0,2,4,6,8,10},
   {0,-0.03266667,0}}.

   bhEff302 ist zweistufig (effsub3.c:1109): case 0 richtet die sechs Stufen ein
   und erhoeht mode0 (effsub3.c:1245), zeichnet aber nichts. Erst der naechste
   Dispatch laeuft in case 1. Stufe k entsteht also im Dispatch 2 + stg_timer[k]
   und faellt weg, sobald ihr Timer stg_erase = 16 erreicht, d. h. im Dispatch
   18 + stg_timer[k]. Der Timer wird in jedem case-1-Durchlauf erhoeht
   (effsub3.c:1322, ausserhalb der if-Kette, also auch fuer negative Werte). */
static void emitter_stages()
{
    seed();
    EffectParticles p;
    check(p.active()==0,"a fresh particle pool is not empty");
    check(p.unsupported().empty(),"the particle emitters reported an unsupported service");
    ParticleLink link;
    p.spawn(0,link,{1,2,3},{0,1,0},0xFF804020u);
    check(p.active()==2,"PtclTbl[0] set_num 2 did not create two particle works");

    SceneCamera camera;
    Scene scene;
    p.append(scene,camera,4);
    check(scene.triangles.empty(),"a particle work drew before its first dispatch");

    /* Dispatch 1 ist case 0: einrichten, nichts zeichnen. */
    p.tick(at_rest,no_wall);
    check(p.active()==2,"mode0 case 0 dropped a particle work");
    p.append(scene,camera,4);
    check(scene.triangles.empty(),"mode0 case 0 already drew a stage");

    /* Dispatch 2 ist der erste case-1-Durchlauf und legt Stufe 0 an. */
    p.tick(at_rest,no_wall);
    check(p.active()==2,"mode0 case 1 dropped a particle work");
    scene.triangles.clear();
    p.append(scene,camera,4);
    /* Je Arbeit und Stufe vier njPtclDrawSprite(&bufP[4k], 4, ..) (effsub3.c:1420-1423).
       njPtclDrawSprite legt je Punkt zwei Eckpunkte an, reicht aber num = 4 an
       Ps2AddPrim3D weiter (ELF 0x002D8528/0x002D86A4), das genau num Eckpunkte
       ausgibt: zwei GS-Sprites je Aufruf, acht je Stufe, zu je zwei Dreiecken. */
    check(scene.triangles.size()==32u,"the first Eff302Prm[0] stage did not draw two works of eight GS sprites");
    /* stg_tim[0] steht nach dem Anlegen auf 1 (effsub3.c:1322): der
       Texturrahmen ist tex_id + (stg_tim & 3), hier die Basis 4. */
    check(scene.triangles[0].texture==5u,"the particle texture frame differs from stg_tim & 3");
    /* col_lv 255: (color & 0xFF00FF) * 256 >> 8 ist die Identitaet. col_lv 224
       (zweiter Satz, zweite Arbeit) liefert (0x80 * 225) >> 8 = 0x70,
       (0x40 * 225) >> 8 = 0x38, (0x20 * 225) >> 8 = 0x1C; Alpha bleibt fest. */
    check(scene.triangles[0].color==0xFF804020u,"col_lv 255 changed the particle colour");
    check(scene.triangles[16].color==0xFF70381Cu,"col_lv 224 did not scale the particle colour");
    check(scene.triangles[0].blend==SceneBlend::Ninja&&scene.triangles[0].blend_register==0x44,
          "njColorBlendingMode(0,8)/(1,6) is not the particle blend register");
    check(scene.triangles[0].depth_write==false,"particle sprites must not write depth");
    check(scene.triangles[0].textured&&!scene.triangles[0].camera_mask,"particle sprites must be textured billboards");
    /* vtx_scale = 0.025; njPtclDrawSprite (ps2_NinjaPtcl.c:126-134) legt die
       Ecken bei scr.x/y -+ iz * w * dist, ohne AspectW, waehrend scr.x selbst
       mit AspectW = 1.174 projiziert ist (njViewScreenMatrix ps2_NaView.c:221,
       BHD_ASPECT_X main.c:53). Das GS-Sprite ist bildschirmachsenparallel mit
       konstanter Tiefe; in Sichtkoordinaten misst es 2 * 0.025 / 1.174
       waagerecht und 2 * 0.025 senkrecht, A (ST 0/0) liegt oben links.
       scl_add_1st greift erst im naechsten Tick. Gedrehte Kamera, damit eine
       Weltachsen-Abbildung auffaellt. */
    {
        SceneCamera turned;
        turned.angles={0x0800,0x2400,0x0300};
        turned.position={1,2,-6};
        Scene view_scene;
        p.append(view_scene,turned,4);
        check(view_scene.triangles.size()==32u,"the turned camera changed the GS sprite count");
        NJS_MATRIX view;camera_view_matrix(turned,view);
        for(std::size_t t=0;t<view_scene.triangles.size();t+=2){
            float minx=1e9f,maxx=-1e9f,miny=1e9f,maxy=-1e9f,minz=1e9f,maxz=-1e9f;
            float u0x=0,u0y=0,u1x=0,u1y=0;
            for(std::size_t k=t;k<t+2;++k)for(const auto& v:view_scene.triangles[k].vertices){
                NJS_POINT3 src=v.position,q;njCalcPoint(&view,&src,&q);
                minx=std::fmin(minx,q.x);maxx=std::fmax(maxx,q.x);
                miny=std::fmin(miny,q.y);maxy=std::fmax(maxy,q.y);
                minz=std::fmin(minz,q.z);maxz=std::fmax(maxz,q.z);
                if(v.u==0&&v.v==0){u0x=q.x;u0y=q.y;}
                if(v.u==1&&v.v==1){u1x=q.x;u1y=q.y;}
            }
            check(std::fabs((maxx-minx)-0.05f/1.174f)<1e-4f,"the GS sprite width ignores iz*w*dist without AspectW");
            check(std::fabs((maxy-miny)-0.05f)<1e-4f,"the GS sprite height differs from 2 * vtx_scale");
            check(maxz-minz<1e-4f,"a GS sprite must keep one depth");
            check(u0x<u1x&&u0y<u1y,"the GS sprite texture does not run from the top-left corner A");
        }
    }

    /* Stufe k entsteht im Dispatch 2 + stg_timer[k] und faellt im Dispatch
       18 + stg_timer[k] weg. Mit stg_timer {0,2,4,6,8,10} stehen also nach
       Dispatch 2/4/6/8/10/12 je eine Stufe mehr, ab Dispatch 18/20/22/24/26
       je eine weniger; Dispatch 28 nimmt die letzte. */
    struct Step{int dispatch;unsigned stages;};
    const Step steps[]={{4,2},{6,3},{8,4},{10,5},{12,6},{17,6},{18,5},{26,1},{27,1}};
    int dispatch=2;
    for(const Step& s:steps){
        while(dispatch<s.dispatch){p.tick(at_rest,no_wall);++dispatch;}
        scene.triangles.clear();
        p.append(scene,camera,4);
        check(scene.triangles.size()==std::size_t(2*8*2*s.stages),"the particle stage count differs from Eff302Prm[0]");
    }

    /* Bei stg_num == 0 setzt bhEff302 oP->flg = 0 und gibt damit alle drei
       Poolplaetze der Arbeit frei: Stufe 5 geht im Dispatch 28. */
    p.tick(at_rest,no_wall);
    check(p.active()==0,"bhEff302 did not release the three pool slots per particle work");
    scene.triangles.clear();
    p.append(scene,camera,4);
    check(scene.triangles.empty(),"a released particle work kept drawing");
    p.spawn(0,link,{1,2,3},{0,1,0},0xFF804020u);
    check(p.active()==2,"a released particle pool slot was not reusable");
}

/* PtclDat04 = {{5,255,2},{11,128,1}} (effsub3.c:826-829): der erste Satz wird
   nach FuncTbl[2] = bhEff_PtclLineDraw gezeichnet, der zweite als Sprite. Beide
   Eff302Prm-Zeilen haben stg_timer = {-1,-1,-1,-1,-1,0}; -1 setzt stg_tim auf 1
   und rueckt stg_stt weiter (effsub3.c:1163-1171), die einzige Stufe liegt
   daher auf Index 5 und entsteht im ersten case-1-Durchlauf, also im
   Dispatch 2. */
static void emitter_line_kind()
{
    seed();
    EffectParticles p;
    ParticleLink link;
    p.spawn(4,link,{0,0,0},{0,1,0},0xFF804020u);
    check(p.active()==2,"PtclDat04 did not create one line and one sprite work");
    SceneCamera camera;
    Scene scene;
    p.tick(at_rest,no_wall);   /* case 0 */
    p.tick(at_rest,no_wall);   /* erster case-1-Durchlauf */
    p.append(scene,camera,4);
    /* Nur der Sprite-Satz zeichnet Dreiecke (acht GS-Sprites, s. o.); der
       Liniensatz geht in scene.lines. */
    check(scene.triangles.size()==16u,"the sprite set of PtclDat04 did not draw eight GS sprites");
    /* bhEff_PtclLineDraw legt je Stufe sechzehn Strecken an und setzt
       lne_col[0] = col & 0xFFFFFF, lne_col[1] = col. */
    check(scene.lines.size()==16u,"bhEff_PtclLineDraw did not emit sixteen segments");
    check(scene.lines[0].colors[0]==0x00804020u,"the line start colour must carry RGB only");
    check(scene.lines[0].colors[1]==0xFF804020u,"the line end colour must carry the alpha byte too");
    for(const auto& line:scene.lines){
        const float dx=line.points[1].x-line.points[0].x;
        const float dy=line.points[1].y-line.points[0].y;
        const float dz=line.points[1].z-line.points[0].z;
        check(std::isfinite(dx)&&std::isfinite(dy)&&std::isfinite(dz),"a particle line is not finite");
        /* (dir[15-i] * spd + grv) * spd * scale * 100; Eff302Prm[5] hat
           vtx_speed 0.5, spd_rand 0.8 und vtx_scale 0.01, also spd in
           [0.3, 0.7] und scl in [0.3, 0.7]. Das haelt jede Strecke unter
           einem Meter. */
        check(dx*dx+dy*dy+dz*dz>0.0f,"a particle line has zero length");
        check(dx*dx+dy*dy+dz*dz<1.0f,"a particle line left the length range of Eff302Prm[5]");
    }
}

/* Der Originalpool hat 512 Plaetze; ein Satz aus PtclDat belegt drei, PtclTbl[0]
   besteht aus zwei Saetzen. Der Vergabecursor startet je Aufruf wieder bei 0. */
static void pool_capacity()
{
    seed();
    EffectParticles p;
    ParticleLink link;
    unsigned created=0;
    for(unsigned i=0;i<200;++i){
        const std::size_t before=p.active();
        p.spawn(0,link,{0,0,0},{0,1,0},0xFFFFFFFFu);
        if(p.active()==before)break;
        check(p.active()==before+2u,"a particle set must occupy three slots and yield two works");
        ++created;
    }
    check(created==85u,"512 pool slots must carry 85 complete two-set emitters");
    check(p.active()==170u,"the particle works of the successful emitters differ");
}

/* flg 0x80 loest die Wurzelarbeit ueber die Knotenmatrix des Besitzers auf
   (effect.c:812). Die Weltposition entsteht erst im Tick, nicht beim Anlegen. */
static void linked_work()
{
    seed();
    EffectParticles p;
    ParticleLink link;
    link.linked=true;link.kind=1;link.index=7;link.node=5;
    p.spawn(0,link,{0,0,1},{0,1,0},0xFFFFFFFFu);
    const NJS_POINT3 joint{10,20,30};
    unsigned resolved=0;
    const auto resolve=[&](const ParticleLink& l,NJS_POINT3 offset){
        check(l.linked&&l.kind==1&&l.index==7&&l.node==5,"a linked particle lost its link");
        ++resolved;
        return NJS_POINT3{joint.x+offset.x,joint.y+offset.y,joint.z+offset.z};
    };
    /* Dispatch 1 ist case 0, Dispatch 2 legt Stufe 0 an. PtclTbl[0] hat zwei
       Saetze, also wird je Dispatch zweimal aufgeloest. */
    for(int i=0;i<2;++i)p.tick(resolve,no_wall);
    check(resolved==4u,"the two linked particle works were not resolved once per tick each");
    SceneCamera camera;
    Scene scene;
    p.append(scene,camera,4);
    check(!scene.triangles.empty(),"the linked particle work drew nothing");
    /* Der Punkt entsteht am aufgeloesten Gelenk plus off_pos, gestreut um
       pos_rand 0.5 (also hoechstens 0.25 je Achse), und wandert noch im selben
       Durchlauf um dir * spd + grv (effsub3.c:1348-1368).

       Schranken aus Eff302Prm[0]: ang_rand 20 ergibt je Achse
       182.04445 * (20 * (r - 0.5)), also hoechstens zehn Grad, zusammen also
       hoechstens dreissig Grad Abweichung von der Startrichtung (0,1,0):
       dir.y >= cos 30 = 0.866, |dir.x| und |dir.z| <= sin 30 = 0.5. Mit
       spd = 1.0 - 0.5 * (1.0 * (r - 0.5)) liegt spd in [0.75, 1.25]. Die halbe
       Spritekante traegt 0.025, die Schwerkraft 0.0327 je Durchlauf.

       Ohne Aufloesung der Verknuepfung laege der Punkt bei (0,0,1) statt am
       Gelenk, also mehr als zehn Einheiten daneben. */
    for(int v=0;v<3;++v){
        const NJS_POINT3& q=scene.triangles[0].vertices[std::size_t(v)].position;
        check(std::fabs(q.x-joint.x)<0.9f,"a linked particle ignored the resolved joint x");
        check(std::fabs(q.z-(joint.z+1.0f))<0.9f,"a linked particle ignored its joint offset");
        check(std::fabs(q.y-joint.y)<1.53f,"a linked particle ignored the resolved joint y");
        /* 0.866 * 0.75 - 0.25 - 0.025 - 0.033 = 0.34: der Schritt entlang der
           Richtung ist auch im unguenstigsten Fall sichtbar. */
        check(std::fabs(q.y-joint.y)>0.34f,"the linked particle did not advance along its direction");
    }
}

/* bhSetBloodPoolLnk (effsub3.c:145-231) + bhEff300: py += 0.01, dann off_pos,
   dann 0.001 * sys->bl_ct. bhDrawTrsEffect3D zeichnet njDrawTexture3DEx(tvp,
   16, 1) (effect.c:1414), njDrawTextureSub3D gibt das als Ps2AddPrim3D(0x3E..)
   aus (ps2_NaDraw.c:351). Das PRIM-Feld beginnt bei Bit 47 (PRE ist Bit 46, ELF
   Ps2AddPrim3D 0x002CBE1C: Tag 0x3000400000008000 | prim | num), 0x3E << 48 ist
   also PRIM 0x7C = TRISTRIP|IIP|TME|FGE|ABE: ein Streifen ueber die 16 Eckpunkte
   in tv_buf-Reihenfolge, Texturrahmen 17 + pal_bnk, Farbe 0xF0808080. */
static void blood_pool()
{
    seed();
    const BpWork table{{0,0.1f,0},0,0,0.06f,{0.5f,0.7f,0.4f,1.0f,1.0f},{1,3,8,11,15}};
    EffectParticles p;
    p.spawn_pool(&table,{2,3,4},0,1);
    check(p.active()==1,"bhSetBloodPoolLnk did not create a pool work");
    p.tick(at_rest,no_wall);
    Scene scene;
    p.append_pools(scene,17);
    check(scene.triangles.empty(),"bhEff300 case 0 must not enter ef_trs");
    p.tick(at_rest,no_wall);
    p.append_pools(scene,17);
    check(scene.triangles.size()==14u,"bhEff300 did not emit one 16-vertex triangle strip");
    check(scene.triangles[0].color==0xF0808080u,"the blood pool colour differs");
    check(scene.triangles[0].texture==18u,"the blood pool texture frame 17 + pal_bnk differs");
    check(scene.triangles[0].blend==SceneBlend::Ninja&&scene.triangles[0].blend_register==0x44,
          "the blood pool blend register differs");
    check(std::fabs(scene.triangles[0].vertices[0].position.y-(3+0.01f+0.1f))<1e-5f,
          "the blood pool resting height differs from py += 0.01 and off_pos");
    check(scene.triangles[0].vertices[0].position.x!=2.0f,"bhEff300 case 1 did not spread before queuing");

    /* bhEff300 case 0 (effsub3.c:264-284): ang beginnt bei eff_dir + 16384 und
       springt um 4096 * i, bei geradem i negativ; u/v = 0.5 + 0.5 * cos/sin(ang).
       Dreieck t des Streifens besteht aus den Eckpunkten t, t+1, t+2. */
    float uv[16][2];
    {
        int ang=0+16384;
        for(int i=0;i<16;++i){
            int tmp=4096*i;if(!(i&1))tmp=-tmp;ang+=tmp;
            uv[i][0]=0.5f+0.5f*njCos(ang);uv[i][1]=0.5f+0.5f*njSin(ang);
        }
    }
    const auto has_uv=[&](const SceneTriangle& tr,int i){
        for(const auto& v:tr.vertices)if(std::fabs(v.u-uv[i][0])<1e-5f&&std::fabs(v.v-uv[i][1])<1e-5f)return true;
        return false;};
    double area=0;
    for(unsigned t=0;t<14;++t){
        const SceneTriangle& tr=scene.triangles[t];
        check(tr.texture==18u,"a blood pool triangle lost its texture frame");
        check(has_uv(tr,int(t))&&has_uv(tr,int(t)+1)&&has_uv(tr,int(t)+2),
              "a blood pool triangle is not (t, t+1, t+2) of the tv_buf strip");
        check(tr.ninja_primitive!=0u&&tr.ninja_primitive==scene.triangles[0].ninja_primitive&&tr.ninja_screen_reject,
              "the blood pool strip lost the njDrawTextureSub3D call clip rule");
        const auto& a=tr.vertices[0];const auto& b=tr.vertices[1];const auto& c=tr.vertices[2];
        area+=0.5*std::fabs(double(b.u-a.u)*double(c.v-a.v)-double(c.u-a.u)*double(b.v-a.v));
        check(std::fabs(a.position.y-b.position.y)<1e-6f&&std::fabs(a.position.y-c.position.y)<1e-6f,
              "the blood pool strip left its floor plane");
    }
    /* Der Zickzack-Streifen fuellt das Sechzehneck im Texturkreis (Radius 0.5)
       genau einmal: Flaeche 8 * 0.25 * sin(22.5 Grad). Acht Paar-Rechtecke oder
       ein falsch geordneter Streifen ergeben eine andere Summe. */
    check(std::fabs(area-2.0*std::sin(3.14159265358979/8.0))<1e-4,"the blood pool strip does not tile the sixteen-gon once");

    /* sys->bl_ct staffelt die Hoehe und laeuft ueber 0x1FF zurueck. */
    EffectParticles q;
    q.spawn_pool(&table,{2,3,4},0,1);
    q.spawn_pool(&table,{2,3,4},0,1);
    q.tick(at_rest,no_wall);
    q.tick(at_rest,no_wall);
    Scene staggered;
    q.append_pools(staggered,17);
    check(staggered.triangles.size()==28u,"two blood pools did not emit two strips");
    check(std::fabs((staggered.triangles[14].vertices[0].position.y-staggered.triangles[0].vertices[0].position.y)-0.001f)<1e-6f,
          "sys->bl_ct did not stagger the blood pool height by 0.001");
    check(staggered.triangles[14].ninja_primitive!=staggered.triangles[0].ninja_primitive,
          "two blood pool draw calls share one clip identity");

    /* Die Lache bleibt stehen, bis der Besitzer sie freigibt. */
    for(int i=0;i<300;++i)p.tick(at_rest,no_wall);
    check(p.active()==1,"the blood pool stopped on its own");
    scene.triangles.clear();
    p.append_pools(scene,17);
    check(scene.triangles.size()==14u,"the resting blood pool stopped drawing");
}

/* effsub3.c:187, 389-397: lkwkp entscheidet bei der Einreihung ueber stflg
   und pt_flg 0x2. NULL ignoriert das Gegnertor. Der Zeichenpass liest die zuvor
   aufgebaute ef_trs-Liste, auch wenn sich der Gegnerstatus danach aendert. */
static void blood_pool_visibility()
{
    const BpWork table{{0,0.1f,0},0,0,0.06f,{0,0,0,0,0},{0,0,0,0,0}};
    RECVX_ENEMY_WORK enemy{};
    EffectParticles linked,unlinked;
    linked.spawn_pool(&table,{0,0,0},0,0,&enemy);
    unlinked.spawn_pool(&table,{0,0,0},0,0);
    const auto draws=[](const EffectParticles& particles){Scene s;particles.append_pools(s,0);return s.triangles.size();};
    check(draws(linked)==0&&draws(unlinked)==0,"a newly allocated blood pool was queued before control");
    linked.tick(at_rest,no_wall,0);unlinked.tick(at_rest,no_wall,0);
    check(draws(linked)==0&&draws(unlinked)==0,"bhEff300 init entered ef_trs");
    linked.tick(at_rest,no_wall,0);unlinked.tick(at_rest,no_wall,0);
    check(draws(linked)==0&&draws(unlinked)==14,"NULL link must ignore pt_flg 0x2, linked pool must obey it");
    linked.tick(at_rest,no_wall,2);
    check(draws(linked)==14,"visible enemy pool was not queued");
    enemy.stflg=0x1000000u;
    check(draws(linked)==14,"draw pass changed the already queued pool list");
    linked.tick(at_rest,no_wall,2);
    check(draws(linked)==0,"sleeping enemy pool entered ef_trs");
    enemy.stflg=0;
    check(draws(linked)==0,"draw pass requeued a suppressed blood pool");
    linked.tick(at_rest,no_wall,2);
    check(draws(linked)==14,"pool did not return with its enemy");
    for(unsigned i=0;i<260;++i)linked.tick(at_rest,no_wall,2);
    enemy.stflg=0x1000000u;linked.tick(at_rest,no_wall,2);
    check(draws(linked)==0,"resting bhEff300 case 2 ignored the enemy stflg");
    enemy.stflg=0;linked.tick(at_rest,no_wall,2);
    check(draws(linked)==14,"resting bhEff300 case 2 did not resume drawing");
}

/* bhEne_SetBloodEffect4 reicht die beiden Blutfarben unveraendert durch
   (bhSetEffParticleMk2): mtn_attr = src_col, mtn_no = dst_col. PtclTbl[1] nutzt
   Eff302Prm[1] mit stg_timer {0,1,8,9,16,17}: Stufe 0 entsteht im Dispatch 2,
   Stufe 1 im Dispatch 3. Nach drei Dispatches stehen also zwei Stufen je
   Arbeit, nach zwei erst eine. */
static void emitter_mk2()
{
    seed();
    EffectParticles p;
    ParticleLink link;
    p.spawn_mk2(1,link,{0,1,0},{0,0,1},0xFF204080u,0x10FF8040u);
    p.tick(at_rest,no_wall);   /* case 0 */
    p.tick(at_rest,no_wall);   /* Dispatch 2: Stufe 0 */
    SceneCamera camera;
    Scene scene;
    p.append(scene,camera,4);
    check(scene.triangles.size()==32u,"PtclTbl[1] did not draw two works of one stage");
    /* Die frisch angelegte Stufe traegt src_col unveraendert (effsub3.c:1294). */
    check(scene.triangles[0].color==0xFF204080u,"bhSetEffParticleMk2 changed the source colour");
    check(scene.triangles[16].color==0xFF204080u,"the second Mk2 set lost its source colour");

    p.tick(at_rest,no_wall);   /* Dispatch 3: Stufe 1 */
    scene.triangles.clear();
    p.append(scene,camera,4);
    check(scene.triangles.size()==64u,"PtclTbl[1] did not draw two works of two stages");
    /* Stufe 1 ist neu und traegt src_col; Stufe 0 hat bereits einen
       col_add/col_sub-Schritt hinter sich (effsub3.c:1301-1302). */
    check(scene.triangles[16].color==0xFF204080u,"the freshly created Mk2 stage lost its source colour");
    check(scene.triangles[0].color!=0xFF204080u,"the older Mk2 stage did not take a colour step");
}

int main()
{
    try{
        /* njRotateXYZ in bhEff302 case 0 braucht die Sinustabelle; ohne sie
           liefern njSin/njCos null, die Rotationsmatrix wird null und jede
           Partikelrichtung waere (0,0,0). Runtime (runtime.cpp:159) und die
           Nachbarsuiten richten sie genauso ein. */
        _Make_SinTable();
        emitter_stages();
        std::cout<<"emitter stages ok\n";
        emitter_line_kind();
        std::cout<<"emitter line kind ok\n";
        pool_capacity();
        std::cout<<"pool capacity ok\n";
        linked_work();
        std::cout<<"linked work ok\n";
        blood_pool();
        blood_pool_visibility();
        std::cout<<"blood pool ok\n";
        emitter_mk2();
        std::cout<<"emitter mk2 ok\n";
    }catch(const std::exception& e){
        std::cerr<<"FAILED: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}

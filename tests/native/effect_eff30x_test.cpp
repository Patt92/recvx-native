// Komponententest fuer bhEff305 (effsub3.c:1779) und bhEff306 (effsub3.c:1944) ohne Raum und
// ohne Datendateien. Der Satz entsteht wie im Original ueber bhSetEffectTb
// (EffectSpawn + initialize_effect_sprite); Kamera, Wind und Knotenmatrix sind Testaufbau.
// Erwartungswerte sind aus den Originaltabellen (Eff305Prm, Tex2uv, Eff306Prm, Tex8uv,
// ColTbl) und den Originalrechenregeln hergeleitet, Lebensdauern aus time/time0/time1/wait,
// nicht aus dem Port abgelesen. Zufall: derselbe EE-Generator (core_random), Saat 1.
#include "effect_eff30x.h"
#include "effect_sprites.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "core_random.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
static bool close(float a,float b,float eps=1e-4f){return std::fabs(a-b)<eps;}
static bool has_gap(const std::set<std::string>& gaps,const char* needle){
    for(const auto& g:gaps)if(g.find(needle)!=std::string::npos)return true;return false;
}
// rand() der Originalroutinen als Anteil: -rand() / -2.1474836E9f (effsub3.c:2014/:2036).
static float original_random(){return -recvx_core_random_next()/-2.1474836e9f;}
// bhSetEffectTb(&sys->ef, off, lkwkp, lkono): Vorlage -> Satz.
static EffectSprite work(unsigned id,unsigned type,NJS_POINT3 pos,NJS_POINT3 scale,int ax,int ay,unsigned mdlver,const NJS_POINT3* off=nullptr){
    EffectSpawn spawn;spawn.flg=1;spawn.id=id;spawn.type=type;spawn.position=pos;spawn.scale=scale;spawn.ax=ax;spawn.ay=ay;spawn.mdlver=mdlver;
    if(off){spawn.has_offset=true;spawn.offset=*off;}
    EffectSprite s;initialize_effect_sprite(s,spawn);return s;
}
// px += unit(cam - p) * dst (effsub3.c:1845-1853 und :2091-2099).
static NJS_POINT3 toward_camera(NJS_POINT3 p,NJS_POINT3 cam,float dst){
    NJS_POINT3 v{cam.x-p.x,cam.y-p.y,cam.z-p.z};njUnitVector(&v);
    return {p.x+v.x*dst,p.y+v.y*dst,p.z+v.z*dst};
}
static bool uv_is(const EffectSprite& s,float u,float v,float xs,float ys){
    return s.tv[0].u==u&&s.tv[0].v==v&&s.tv[1].u==u+xs&&s.tv[1].v==v&&s.tv[2].u==u&&s.tv[2].v==v+ys&&s.tv[3].u==u+xs&&s.tv[3].v==v+ys;
}
static bool col_is(const EffectSprite& s,std::uint32_t c){for(const auto& v:s.tv)if(v.col!=c)return false;return true;}

// ---- bhEff305 ---------------------------------------------------------------------------
// effsub3.c:1781 Eff305Prm[4]: {color, speed, accel, time, offset, src_scl, dst_scl}.
struct Prm305 {std::uint32_t color;float speed,accel;int time;float offset,src,dst;};
static const Prm305 prm305[4]={
    {0x70F0C080u,0.1f,-0.01f,16,1.5f,1.5f,3.0f},{0x70F0C080u,0.1f,-0.01f,20,1.5f,2.0f,3.0f},
    {0x70F0C080u,0.1f,-0.01f,24,1.5f,2.5f,3.5f},{0x70F0C080u,0.1f,-0.01f,28,1.5f,3.0f,3.5f}};
// effsub3.c:1788 Tex2uv[8]: u-Spalten 0.125..0.875, Zeile 7 = Zeile 6.
static const float tex2uv_u[8]={0.125f,0.25f,0.375f,0.5f,0.625f,0.75f,0.875f,0.875f};
static const float tex2uv_wh=0.12109375f;

static void eff305_type(unsigned type,NJS_POINT3 scale,int ax,int ay,const EffectContext& ctx){
    const Prm305& p=prm305[type];
    const NJS_POINT3 P{1.0f,2.0f,3.0f};
    auto s=work(305,type,P,scale,ax,ay,0);
    bool queued=false;
    take_effect_output();
    step_eff305(s,ctx,queued);
    check(take_effect_output().gaps.empty(),"305: keine Luecke bei type 0..3");
    // Start (:1809-1827).
    check(queued,"305 Pass 1: Eintrag in sys->ef_trs");
    check(s.flg==0x4300001u&&s.mode0==1u&&s.tex_id==2&&s.ani_ct==0u&&s.bl_src==8&&s.bl_dst==10,"305 Pass 1: flg/tex_id/ani_ct/Blend");
    check(s.shp_ct==p.accel,"305: shp_ct = accel");
    {float e=p.speed;e+=p.accel;check(s.spd==e,"305 Pass 1: spd = speed + accel");}
    const int ct2=int((p.color/std::uint32_t(p.time))&0xFF000000u);
    check(s.ct2==ct2,"305: ct2 = (color / time) & 0xFF000000");
    check(s.ct1==int(p.color)-ct2,"305 Pass 1: ct1 = color - ct2");
    check(s.ct0==p.time-1&&s.ct3==1,"305 Pass 1: ct0 = time - 1, ct3 = 1");
    // Skalierung (:1829-1837): sx = src * sxb + (dst * sxb - src * sxb) / time.
    {float sx=p.src;sx*=scale.x;float lox=((p.dst*scale.x)-sx)/float(p.time);sx+=lox;
     float sy=p.src;sy*=scale.y;float loy=((p.dst*scale.y)-sy)/float(p.time);sy+=loy;
     check(s.sx==sx&&s.sy==sy&&s.lox==lox&&s.loy==loy,"305 Pass 1: sx/sy und lox/loy aus src_scl/dst_scl/time");}
    // Farbe (:1905-1923): Pass 1 zeichnet color unvermindert; UV Zeile 0.
    check(col_is(s,p.color),"305 Pass 1: Eckfarbe = color");
    check(uv_is(s,tex2uv_u[0],0.0f,tex2uv_wh,tex2uv_wh),"305 Pass 1: Tex2uv[0]");
    // Bewegung (:1841-1860, :1879-1903): Lage = P + offset zur Kamera, + ao * speed, + Wind gpx.
    const float winds=effect_weather().winds;const int windr=effect_weather().windr;
    const float g=0.1f*winds;
    check(s.gpy==g&&s.gpx==g+g,"305 Pass 1: gpx = gpy = 0.1 winds, gpx += gpy");
    check(s.yn==0.0f-(-0.0054444447f)&&s.xn==0.0f&&s.zn==0.0f,"305 Pass 1: yn -= -0.0054444447, xn/zn 0");
    const NJS_POINT3 p0=toward_camera(P,ctx.camera_position,p.offset);
    const float aox=-njSin(ay)*njCos(ax),aoy=njSin(ax),aoz=-njCos(ay)*njCos(ax);
    check(s.aox==aox&&s.aoy==aoy&&s.aoz==aoz,"305: aox/aoy/aoz aus ay/ax");
    const float wx=-njSin(windr),wz=-njCos(windr);
    check(close(s.px,p0.x+aox*p.speed+g*wx)&&close(s.py,p0.y+aoy*p.speed)&&close(s.pz,p0.z+aoz*p.speed+g*wz),"305 Pass 1: Lage");
    // Paesse 2..time: UV-Zeile min(k-1, 7), Alpha sinkt um ct2 je Pass, spd bleibt > 0.
    for(int k=2;k<=p.time;++k){
        queued=false;step_eff305(s,ctx,queued);
        check(queued&&s.flg==0x4300001u,"305: jeder Pass bis time ist eingetragen");
        const int row=k-1<7?k-1:7;
        check(uv_is(s,tex2uv_u[row],0.0f,tex2uv_wh,tex2uv_wh),"305: Tex2uv-Zeile je Pass");
        check(col_is(s,std::uint32_t(int(p.color)-(k-1)*ct2)),"305: Farbe = color - (k-1) ct2");
        check(s.spd>0.0f,"305: spd bleibt > 0 (:1891)");
        check(s.ct0==p.time-k,"305: ct0 zaehlt je Pass");
        if(k==5&&ax==16384){
            // Geschlossene Form nach 5 Paessen (ax 16384: ao = (0, 1, 0)):
            // sum spd = 5 speed + accel (0+1+2+3+4), sum yn = 0.0054444447 (0+1+2+3+4), sum gpx = g (1+2+3+4+5).
            const float dy=5.0f*p.speed+p.accel*10.0f+0.0054444447f*10.0f;
            check(close(s.py,p0.y+dy,2e-3f)&&close(s.px,p0.x+g*15.0f*wx,2e-3f)&&close(s.pz,p0.z+g*15.0f*wz,2e-3f),"305 Pass 5: Flug/Auftrieb/Wind");
        }
    }
    // Pass time + 1 (:1873): ct0-- <= 0 -> flg 0, kein Eintrag (der Aufrufer in
    // effect_sprites.cpp uebergibt `queued` je Pass frisch mit false).
    queued=false;step_eff305(s,ctx,queued);
    check(!queued&&s.flg==0u,"305: Satzende im Pass time + 1");
}
static void eff305(){
    EffectContext ctx;ctx.camera_position={0.0f,10.0f,20.0f};
    effect_weather().winds=2.0f;effect_weather().windr=8192;
    for(unsigned t=0;t<4;++t){
        eff305_type(t,{1,1,1},16384,10922+int(t)*4369,ctx);   // en19.c:3704 Schritt (ax 16384) / :3746 Aufprall
        eff305_type(t,{2.0f,0.5f,1.0f},0,-20000+int(t)*7000,ctx); // ax 0 (Knie/Hand, Spielerreaktion) mit Erzeugerskalierung
    }
    // Ohne Wind: gpx 0, Lage nur aus ao (Gegenprobe der Windsumme).
    effect_weather().winds=0.0f;
    {auto s=work(305,0,{1,2,3},{1,1,1},0,0,0);bool q=false;step_eff305(s,ctx,q);
     const NJS_POINT3 p0=toward_camera({1,2,3},ctx.camera_position,1.5f);
     check(s.gpx==0.0f&&close(s.px,p0.x)&&close(s.pz,p0.z-0.1f)&&close(s.py,p0.y),"305 ohne Wind: Lage = p0 + (0, 0, -speed)");}
    // type 4 (:1818 liest hinter Eff305Prm): Luecke, Satzende, kein Ersatz.
    {take_effect_output();auto s=work(305,4,{1,2,3},{1,1,1},0,0,0);bool q=false;step_eff305(s,ctx,q);
     check(!q&&s.flg==0u&&has_gap(take_effect_output().gaps,"bhEff305"),"305 type 4: Luecke gemeldet, flg 0");}
}

// ---- bhEff306 ---------------------------------------------------------------------------
// effsub3.c:1982 Eff306Prm[8]: {tex_id, wait, time0, time1, offset, src_scl, dst_scl}.
struct Prm306 {int tex_id,wait,time0,time1;float offset,src,dst;};
static const Prm306 prm306[8]={
    {8,0,4,32,3.0f,0.2f,1.5f},{8,0,4,48,3.0f,0.2f,1.5f},{8,2,8,64,3.0f,0.3f,2.0f},{8,2,8,80,3.0f,0.3f,2.0f},
    {8,1,16,96,3.0f,0.5f,2.5f},{8,1,16,112,3.0f,0.5f,2.5f},{8,0,4,48,3.0f,0.2f,1.0f},{8,0,4,32,3.0f,0.2f,1.0f}};
// effsub3.c:1950 Tex8uv[10]: {u, v}, Breite/Hoehe 0.18359375.
static const float tex8uv[10][2]={{0.0f,0.0f},{0.1875f,0.0f},{0.375f,0.0f},{0.5625f,0.0f},{0.75f,0.0f},
    {0.0f,0.1875f},{0.1875f,0.1875f},{0.375f,0.1875f},{0.5625f,0.1875f},{0.75f,0.1875f}};
static const float tex8uv_wh=0.18359375f;
// effsub3.c:1946 ColTbl[4].
static const std::uint32_t col_tbl[4]={0xFFF0F0F0u,0xFF808080u,0xFFD0D0D0u,0xFFC0C0C0u};

static void eff306_type(unsigned type,const EffectContext& ctx){
    const Prm306& p=prm306[type];
    // Erzeuger (en19.c:3436/:3446): Lage = Gelenkursprung, Versatz off -> lox, Link = Werk/Gelenk.
    NJS_MATRIX link;njUnitMatrix(&link);link[12]=10.0f;link[13]=20.0f;link[14]=30.0f;
    const NJS_POINT3 P{10.0f,20.0f,30.0f},off{1.0f,2.0f,3.0f},linked{11.0f,22.0f,33.0f};
    // Erwarteter zweiter Startwert: mdlver 0 -> genau ein rand() fuer ayp (:2036).
    recvx_core_random_seed(1);
    const int ayp=int(5461.0f*original_random())+10922;
    check(ayp>=10922&&ayp<=16383,"Testaufbau: ayp im Originalbereich 10922..16383");
    recvx_core_random_seed(1);
    auto s=work(306,type,P,{1,1,1},0,0,0,&off);s.link_matrix=link;
    bool queued=false;take_effect_output();
    step_eff306(s,ctx,queued);
    check(take_effect_output().gaps.empty(),"306: keine Luecke bei type 0..7 mit Knoten");
    // Start (:2005-2040).
    check(s.flg==0x4300001u&&s.ani_ct==0u&&s.bl_src==8&&s.bl_dst==10&&s.mode0==1u,"306 Pass 1: flg/ani_ct/Blend/mode0");
    check(s.tex_id==8&&s.type==type,"306: tex_id aus Eff306Prm, type bleibt (mdlver 0)");
    check(s.mtn_no==p.time0&&s.ct2==p.time1&&s.spd==p.offset&&s.sz==p.src,"306: mtn_no/ct2/spd/sz");
    check(s.ayp==ayp&&s.axp==ayp,"306 Pass 1: ayp aus rand, axp = ayp nach dem ersten Nachlauf");
    const float syb=(p.dst-p.src)/float(p.time0);
    check(s.syb==syb,"306: syb = (dst_scl - src_scl) / time0");
    NJS_POINT3 expect;
    if(p.wait==0){
        check(queued&&s.ct1==0&&s.ct0==p.time0-1,"306 Pass 1 ohne Wartezeit: Eintrag, ct0 zaehlt");
        {float sxb=p.src;sxb+=syb;check(s.sxb==sxb,"306 Pass 1: sxb += syb");}
        expect=toward_camera(linked,ctx.camera_position,p.offset);
    }else{
        check(!queued&&s.ct1==p.wait-1&&s.ct0==p.time0,"306 Pass 1 mit Wartezeit: kein Eintrag, ct1 zaehlt");
        check(s.sxb==p.src,"306 Pass 1 mit Wartezeit: sxb unveraendert");
        expect=toward_camera(P,ctx.camera_position,p.offset);
    }
    check(close(s.px,expect.x)&&close(s.py,expect.y)&&close(s.pz,expect.z),"306 Pass 1: Lage (Knoten * lox bzw. Startlage) + offset zur Kamera");
    {const float rnd=0.1f*njSin(ayp);check(s.sx==s.sxb*(1.0f+rnd)&&s.sy==s.sxb*(1.0f-rnd),"306: sx/sy = sxb (1 +- 0.1 sin axp)");}
    // UV: Zeile ayp & 7 (:2038), Farbe ColTbl[(ayp >> 13) & 3] = ColTbl[1] fuer 10922..16383.
    const int row0=ayp&7;
    check(uv_is(s,tex8uv[row0][0],tex8uv[row0][1],tex8uv_wh,tex8uv_wh),"306 Pass 1: Tex8uv[ayp & 7]");
    check(col_is(s,col_tbl[1])&&((ayp>>13)&3)==1,"306 Pass 1: ColTbl[(axp >> 13) & 3] = 0xFF808080");
    check(s.ct3==row0+1,"306 Pass 1: ct3 rueckt vor");
    // Weiterlauf: Wartezeit (ohne Eintrag, Lage driftet weiter zur Kamera), dann
    // time0 + 2 Paesse Fall 1, time1 + 2 Paesse Fall 2, time0 + 2 Paesse Fall 3, Endpass Fall 4.
    int counts[5]={0,0,0,0,0};int unqueued_wait=p.wait==0?0:1;int axp=ayp;int ct3=row0+1;
    float sxb_at_mode2=0;bool checked_mode2=false,saw_wrap=false;int passes=1;
    NJS_POINT3 prev{s.px,s.py,s.pz};
    if(p.wait==0)counts[1]=1;
    for(;;){
        const unsigned mode_before=s.mode0;
        queued=false;step_eff306(s,ctx,queued);++passes;
        check(passes<400,"306 endet nicht");
        if(s.flg==0u){check(!queued&&mode_before==4u,"306: Endpass ist Fall 4 ohne Eintrag");break;}
        if(mode_before<=3u&&s.ct1>0){check(!queued,"306 Wartezeit: kein Eintrag");++unqueued_wait;}
        else if(mode_before<=3u&&!queued){ // letzter Wartepass (ct1 wurde 0)
            ++unqueued_wait;
            const NJS_POINT3 e=toward_camera(prev,ctx.camera_position,p.offset);
            check(close(s.px,e.x)&&close(s.py,e.y)&&close(s.pz,e.z),"306 Wartezeit: Lage driftet je Pass um offset zur Kamera");
        }else{
            check(queued,"306 Fall 1/2/3: Eintrag");
            ++counts[mode_before];
            // :2082 der Nachlauf (Drift zur Kamera) entfaellt in dem Pass, der mode0 auf 4 schaltet.
            const NJS_POINT3 e=s.mode0==4u?linked:toward_camera(linked,ctx.camera_position,p.offset);
            check(close(s.px,e.x)&&close(s.py,e.y)&&close(s.pz,e.z),"306 Fall 1/2/3: Lage je Pass neu aus dem Knoten");
        }
        if(mode_before==1u&&s.mode0==2u&&!checked_mode2){
            checked_mode2=true;sxb_at_mode2=s.sxb;
            // src + (time0 + 2) syb: das Original laeuft zwei Schritte ueber dst_scl hinaus.
            float e=p.src;for(int i=0;i<p.time0+2;++i)e+=syb;
            check(s.sxb==e,"306: sxb am Ende von Fall 1 = src + (time0 + 2) syb");
        }
        if(mode_before==2u)check(s.sxb==sxb_at_mode2,"306 Fall 2: sxb konstant");
        if(mode_before==2u&&s.mode0==3u)check(s.syb==-syb&&s.ct0==p.time0,"306: Uebergang 2 -> 3 negiert syb, ct0 = mtn_no");
        // Nachlauf (:2082-2133): Winkel, Farbe, UV-Zeile ct3 (Stand nach dem vorigen Pass), sx/sy.
        // :2082 `if (oP->mode0 != 4)`: der Pass, der mode0 auf 4 schaltet, laesst den Nachlauf aus.
        if(s.mode0==4u){check(s.axp==axp&&s.ct3==ct3,"306: kein Nachlauf im Pass des Uebergangs nach 4");prev={s.px,s.py,s.pz};continue;}
        axp+=ayp;
        check(s.axp==axp,"306: axp += ayp je Pass");
        check(col_is(s,col_tbl[(axp>>13)&3]),"306: Farbe je Pass aus ColTbl");
        {const float rnd=0.1f*njSin(Angle(axp));check(s.sx==s.sxb*(1.0f+rnd)&&s.sy==s.sxb*(1.0f-rnd),"306: sx/sy je Pass");}
        const int row=ct3;
        check(uv_is(s,tex8uv[row][0],tex8uv[row][1],tex8uv_wh,tex8uv_wh),"306: Tex8uv[ct3] je Pass");
        ct3=ct3+1>9?ct3+1-9:ct3+1;
        check(s.ct3==ct3,"306: ++ct3 > 9 -> ct3 -= 9");
        if(row==9){saw_wrap=true;check(s.ct3==1,"306: Umlauf von Zeile 9 auf Zeile 1");}
        if(saw_wrap)check(s.ct3!=0,"306: nach dem ersten Umlauf kommt Zeile 0 nicht wieder (:2129-2132)");
        check(s.ct3>=0&&s.ct3<=9,"306: ct3 innerhalb Tex8uv[10]");
        prev={s.px,s.py,s.pz};
    }
    check(unqueued_wait==p.wait,"306: Wartepaesse = wait");
    check(counts[1]==p.time0+2&&counts[2]==p.time1+2&&counts[3]==p.time0+2,"306: Passzahlen time0 + 2 / time1 + 2 / time0 + 2");
    check(passes==p.wait+2*p.time0+p.time1+6+1,"306: Gesamtpaesse = wait + 2 time0 + time1 + 7");
    check(close(s.sxb,p.src,1e-4f),"306: sxb nach Fall 3 wieder bei src_scl");
    check(saw_wrap,"306: UV-Umlauf im Test erreicht");
}
static void eff306(){
    EffectContext ctx;ctx.camera_position={0.0f,0.0f,100.0f};
    for(unsigned t=0;t<8;++t)eff306_type(t,ctx);
    // mdlver 1 (Erzeuger en19.c:3432 / en29.c:1578): type = (int)(255 rand) & 7 vor dem ayp-rand.
    for(std::uint32_t seed=1;seed<=8;++seed){
        recvx_core_random_seed(seed);
        const unsigned type=unsigned(int(255.0f*original_random())&7);
        const int ayp=int(5461.0f*original_random())+10922;
        recvx_core_random_seed(seed);
        NJS_MATRIX link;njUnitMatrix(&link);
        auto s=work(306,0,{0,0,0},{1,1,1},0,0,1);s.link_matrix=link;
        bool q=false;step_eff306(s,ctx,q);
        check(s.type==type&&s.ayp==ayp,"306 mdlver 1: type aus dem ersten, ayp aus dem zweiten rand()");
        check(s.ct3==((ayp&7)+1)&&s.mtn_no==prm306[type].time0,"306 mdlver 1: Tabelle nach dem gewuerfelten type");
    }
    // Ohne Knotenmatrix: Fall 1 ohne Wartezeit -> Luecke, Satzende.
    {take_effect_output();auto s=work(306,0,{0,0,0},{1,1,1},0,0,0);bool q=false;step_eff306(s,ctx,q);
     check(!q&&s.flg==0u&&has_gap(take_effect_output().gaps,"bhEff306"),"306 ohne Knoten: Luecke, flg 0");}
    // Ohne Knotenmatrix, aber in der Wartezeit (type 2, wait 2): noch kein Knotenzugriff.
    {take_effect_output();auto s=work(306,2,{0,0,0},{1,1,1},0,0,0);bool q=false;step_eff306(s,ctx,q);
     check(!q&&s.flg==0x4300001u&&s.ct1==1&&take_effect_output().gaps.empty(),"306 Wartezeit ohne Knoten: kein Zugriff, keine Luecke");
     q=false;step_eff306(s,ctx,q);check(!q&&s.flg==0x4300001u&&s.ct1==0,"306 zweiter Wartepass");
     q=false;step_eff306(s,ctx,q);check(!q&&s.flg==0u&&has_gap(take_effect_output().gaps,"bhEff306"),"306 erster Linkpass ohne Knoten: Luecke");}
    // type 8 bei mdlver 0 (:2017 liest hinter Eff306Prm): Luecke, Satzende.
    {take_effect_output();auto s=work(306,8,{0,0,0},{1,1,1},0,0,0);bool q=false;step_eff306(s,ctx,q);
     check(!q&&s.flg==0u&&has_gap(take_effect_output().gaps,"bhEff306"),"306 type 8: Luecke, flg 0");}
}

int main(){try{_Make_SinTable();
    recvx_core_random_seed(1);
    take_effect_output();
    eff305();
    eff306();
    std::cout<<"effect eff30x PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"effect eff30x failed: "<<e.what()<<'\n';return 1;}}

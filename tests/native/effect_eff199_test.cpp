// Komponententest fuer effsub0.c:3478 bhEff199 ohne Raum und ohne Datendateien.
// Die Erwartungen stammen aus den Originalzeilen und werden hier unabhaengig vom Port
// nachgerechnet: Punkttabelle :3480-:3493, sechs Aufrufe mit fuenf Drehungen
// njRotateXYZ(NULL, -256, 256, 0) (:3500-:3515; ps2_NaMatrix.c:649-659 Z, Y, X nachmultipliziert,
// Achsenformeln aus den VU-Bloecken :517-:645), njPtclDrawPolygon (ps2_NinjaPtcl.c:22-82):
// zwei Eckpunkte scr -+ h je Punkt, Ps2AddPrim3D(.., n) mit n = 11 -> n/2 = 5 Sprites aus den
// Punkten 0..4 (ELF 0x002D8334/0x002D845C), Eckfarbe R/G/B + 1 und (A + 1) / 2 in den unteren
// 8 Bit des PACKED-RGBAQ (:37-:45, ELF 0x002D834C-0x002D837C). Der Satz bleibt unveraendert
// (kein O_WRK-Parameter), es gibt keine Anforderung und keine Zeichenliste des Satzes.
#include "effect_sprites.h"
#include "effect_sprites_internal.h"
#include "chunk_blend.h"
#include "ninja_math.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
static bool close(float a,float b,float eps=1e-4f){return std::fabs(a-b)<eps;}
static bool has_gap(const std::set<std::string>& gaps,const char* needle){for(const auto& g:gaps)if(g.find(needle)!=std::string::npos)return true;return false;}

// effsub0.c:3480-:3493: die Tabelle der Prozedur.
static const float kPoints[11][3]={
    {-45.0f,5.0f,94.0f},{-75.0f,6.0f,96.0f},{-105.0f,7.0f,98.0f},{-135.0f,8.0f,100.0f},{-165.0f,9.0f,102.0f},
    {-195.0f,10.0f,104.0f},{-225.0f,11.0f,106.0f},{-255.0f,12.0f,108.0f},{-285.0f,13.0f,110.0f},{-315.0f,14.0f,112.0f},{0,0,0}};
struct P {float x,y,z;};
// ps2_NaMatrix.c:517-:559 njRotateX: Zeile1' = c*Zeile1 + s*Zeile2, Zeile2' = -s*Zeile1 + c*Zeile2,
// mit out = Zeile0*x + Zeile1*y + Zeile2*z also y' = c*y - s*z, z' = s*y + c*z am lokalen Punkt.
static P rot_x(P p,Angle a){a&=0xFFFF;const float s=njSin(a),c=njCos(a);return {p.x,c*p.y-s*p.z,s*p.y+c*p.z};}
// :561-:603 njRotateY: Zeile0' = c*Zeile0 - s*Zeile2, Zeile2' = s*Zeile0 + c*Zeile2 -> x' = c*x + s*z, z' = -s*x + c*z.
static P rot_y(P p,Angle a){a&=0xFFFF;const float s=njSin(a),c=njCos(a);return {c*p.x+s*p.z,p.y,-s*p.x+c*p.z};}
// :605-:647 njRotateZ: Zeile0' = c*Zeile0 + s*Zeile1, Zeile1' = -s*Zeile0 + c*Zeile1 -> x' = c*x - s*y, y' = s*x + c*y.
static P rot_z(P p,Angle a){a&=0xFFFF;const float s=njSin(a),c=njCos(a);return {c*p.x-s*p.y,s*p.x+c*p.y,p.z};}
// njRotateXYZ(NULL, ax, ay, az) = njRotateZ, njRotateY, njRotateX nachmultipliziert (:649-:659):
// der lokale Punkt durchlaeuft erst Rx, dann Ry, dann Rz. Nach k Drehungen: M = cam.mtx * (Rz Ry Rx)^k,
// also Weltpunkt T^k(p) mit T(p) = Rz(Ry(Rx(p))).
static P turn(P p){return rot_z(rot_y(rot_x(p,-256),256),0);}
static P expected_center(int group,int point){P p{kPoints[point][0],kPoints[point][1],kPoints[point][2]};for(int k=0;k<group;++k)p=turn(p);return p;}
// ps2_NinjaPtcl.c:37-:40 fuer col = 0xFFAFAFFF (:3497), je Kanal die unteren 8 Bit des 32-Bit-Worts
// (PACKED RGBAQ); Hostalpha 0xFF fuer As 128 (volle Quelle).
static std::uint32_t expected_color(){
    const std::uint32_t col=0xFFAFAFFFu;
    const unsigned r=(((col>>16)&0xFFu)+1u)&0xFFu,g=(((col>>8)&0xFFu)+1u)&0xFFu,b=((col&0xFFu)+1u)&0xFFu;
    const unsigned as=(((col>>24)&0xFFu)+1u)/2u;
    check(r==0xB0u&&g==0xB0u&&b==0x00u&&as==0x80u,"ps2_NinjaPtcl.c:37-40: 0xAF+1 = 0xB0, 0xFF+1 laeuft auf 0x00 ueber, A (0xFF+1)/2 = 0x80");
    return 0xFF000000u|(r<<16)|(g<<8)|b;
}
static EffectSprite room_work(){
    // room.c:781 bhSetEffectTb(ep, NULL, NULL, 0) eines Raumtabellensatzes: flg 1, id 199, Lage beliebig
    // (die Prozedur liest sie nicht).
    EffectSpawn e;e.flg=1;e.id=199;e.type=0;e.position={12.0f,-3.0f,7.0f};e.scale={1,1,1};
    EffectSprite s;initialize_effect_sprite(s,e);return s;
}

int main(){try{_Make_SinTable();
    const std::uint32_t color=expected_color();
    check(color==0xFFB0B000u,"bhEff199 Eckfarbe: GS-Bytes B0/B0/00 mit vollem Alpha");
    // --- Dispatcher: 199 ist registriert und braucht keinen Kontext. ---
    check(supported_effect_sprite(199),"effect.c:34 bhJumpEffect0[49] = bhEff199 ist nativ registriert");
    {
        EffectSprite s=room_work();bool q=false;
        check(step_effect_sprite(s,&q)&&q,"bhEff199 ohne Kontext (die Prozedur liest keinen) laeuft und zeichnet");
    }
    // --- Ein Pass: 6 Gruppen x 5 Sprites, Lage (Rz Ry Rx)^k * ptcl[i], Groesse +-1 Bildpunkt, Farbe. ---
    {
        take_effect_output();
        EffectSprite s=room_work();EffectContext c;c.pt_flg=0; // kein bhDrawEffect-Pass: die Prozedur zeichnet im Steuerpass trotzdem
        const unsigned flg=s.flg,mode0=s.mode0;const float px=s.px;
        bool q=false;check(step_effect_sprite(s,c,&q),"bhEff199: Dispatch ueber step_effect_sprite");
        check(q&&s.custom_queued,"bhEff199: Zeichenauftrag dieses Passes (eigene Vierecke), auch ohne pt_flg 0x10");
        check(s.quads.size()==30,"ps2_NinjaPtcl.c:35/:76 + ps2_dummy.c:536: n = 11 -> 5 Sprites je Aufruf, sechs Aufrufe (:3500-:3515) = 30");
        for(int group=0;group<6;++group)for(int i=0;i<5;++i){
            const auto& qd=s.quads[std::size_t(group*5+i)];const P e=expected_center(group,i);
            check(close(qd.center.x,e.x,1e-2f)&&close(qd.center.y,e.y,1e-2f)&&close(qd.center.z,e.z,1e-2f),"bhEff199 :3501-:3513: Gruppe k liegt unter (Rz(0) Ry(256) Rx(-256))^k");
            check(!qd.textured&&qd.screen_size&&!qd.world_space&&qd.pn==4&&qd.fogged&&!qd.gouraud,"njPtclDrawPolygon: ohne TME, h in Bildpunkten (:51-:59), FGE, Viereck");
            check(qd.inherit_blend,"bhEff199 setzt kein Blendpaar: das ALPHA-Register bleibt, wie vorgefunden");
            for(unsigned k=0;k<4;++k){const auto& v=qd.vertices[k];
                check(close(std::fabs(v.x),1.0f)&&close(std::fabs(v.y),1.0f)&&v.z==0&&v.col==color,"njPtclDrawPolygon: Ecken scr -+ h mit h = 1.0 (:3500), Eckfarbe aus :37-:45");}
            check(close(qd.vertices[0].x,-1.0f)&&close(qd.vertices[0].y,-1.0f)&&close(qd.vertices[3].x,1.0f)&&close(qd.vertices[3].y,1.0f),"njPtclDrawPolygon :51/:56: erster Eckpunkt scr - h, zweiter scr + h");
        }
        // Gruppe 0 ist die rohe Tabelle; die erste Drehung veraendert sie messbar (256 = 1.40625 Grad).
        check(close(s.quads[0].center.x,-45.0f)&&close(s.quads[0].center.y,5.0f)&&close(s.quads[0].center.z,94.0f),"bhEff199 :3500: erste Gruppe ohne Drehung");
        check(!close(s.quads[5].center.x,-45.0f,1e-3f)||!close(s.quads[5].center.z,94.0f,1e-3f),"bhEff199 :3501: zweite Gruppe ist gedreht");
        // Punkt 5 (-195) liefert nur einen Eckpunkt, Punkte 6..10 keinen: keine Flaeche davon.
        for(const auto& qd:s.quads)check(qd.center.x>-170.0f,"Ps2AddPrim3D(.., 11): aus Punkt 5 (-195) und spaeter entsteht kein Sprite");
        // Der Satz selbst bleibt unberuehrt: kein Parameter, keine Anforderung, kein Ende.
        check(s.flg==flg&&s.mode0==mode0&&s.px==px&&!s.opaque_queued&&!s.punch_queued&&!s.trs2d_queued,"bhEff199 hat keinen O_WRK-Parameter: flg/mode0/Lage unveraendert, keine Zeichenliste des Satzes");
        check(s.fnc_exit_src<0&&s.fnc_exit_dst<0,"bhEff199 ruft kein njColorBlendingMode: hinterlaesst den Blendzustand unveraendert");
        auto out=take_effect_output();
        check(out.spawns.empty()&&out.sounds.empty(),"bhEff199: keine sys->ef-Anforderung, keine Stimme");
        check(has_gap(out.gaps,"Ps2_ptcl_spr_flag"),"bhEff199: Ps2_ptcl_spr_flag (ps2_NinjaPtcl.c:74-81) ist als Luecke benannt");
        check(has_gap(out.gaps,"Steuerpass"),"bhEff199: Zeichnung im Steuerpass statt bhDrawEffect ist als Luecke benannt");
        // effect.c:742: der naechste Pass beginnt mit leeren Listen - wieder 30, nicht 60.
        q=false;step_effect_sprite(s,c,&q);
        check(q&&s.quads.size()==30&&s.flg==flg,"bhEff199: jeder Pass zeichnet dieselben 30 Sprites neu, der Satz lebt weiter");
        // Auch unter dem Zeichenpass-Tor und im Kontext mit Kamera aendert sich die Weltlage nicht:
        // :3495 njSetMatrix(NULL, cam.mtx) traegt der Host, die Gruppen bleiben Weltpunkte.
        EffectContext d;d.pt_flg=0x10;d.camera_ready=true;d.camera_angles={1000,-2000,300};
        step_effect_sprite(s,d,&q);
        for(int group=0;group<6;++group)for(int i=0;i<5;++i){const P e=expected_center(group,i);const auto& qd=s.quads[std::size_t(group*5+i)];
            check(close(qd.center.x,e.x,1e-2f)&&close(qd.center.z,e.z,1e-2f),"bhEff199: Weltlage unabhaengig von der Kamera (cam.mtx ist Zeichenmatrix, nicht Teil der Punkte)");}
        take_effect_output();
    }
    // --- Zeichenpfad des Raumbesitzers: zwei Dreiecke je Sprite, untexturiert, Blendregister wie vorgefunden. ---
    {
        EffectSprite s=room_work();EffectContext c;bool q=false;step_effect_sprite(s,c,&q);take_effect_output();
        Scene scene;SceneCamera camera;camera.distance=10.0f;
        std::map<unsigned,std::pair<unsigned,unsigned>> bases;
        std::pair<unsigned,unsigned> blend{8,10};   // der Zustand, den der ef_fnc-Vorgaenger hinterliess
        append_custom_effect(scene,s,camera,bases,&blend);
        check(scene.triangles.size()==60,"append_custom_effect: 30 GS-Sprites als 60 Dreiecke");
        for(const auto& t:scene.triangles){
            check(!t.textured&&t.blend==SceneBlend::Ninja&&t.blend_register==ninja_blend_register(8,10)&&t.color==color,"bhEff199: untexturiert, erbt das Blendregister des Vorgaengers (8/10), Eckfarbe B0/B0/00");}
        check(blend.first==8&&blend.second==10,"bhEff199 laesst das Blendpaar fuer den Nachfolger unveraendert");
        Scene scene2;std::pair<unsigned,unsigned> entry{8,6};append_custom_effect(scene2,s,camera,bases,&entry);
        check(scene2.triangles.front().blend_register==ninja_blend_register(8,6)&&entry==std::make_pair(8u,6u),"bhEff199 unter dem Eingangszustand 8/6 (effect.c:1268/1416) zeichnet mit 8/6");
    }
    std::cout<<"effect eff199 PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"effect eff199 FAIL: "<<e.what()<<'\n';return 1;}}

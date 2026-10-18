#include "view_clip.h"
#include "renderer.h"
#include <iostream>
using namespace recvx;
// PORTIERUNG (Policy 2026-10-09, native Neubestimmung): Die Clip-Bereiche
// kommen jetzt aus den in native/view_clip.cpp aus event.c übernommenen
// Tabellen. Geprüft werden echte Nachschlagen gegen die Originalwerte — mit
// Vorzeichenkehrung (Original negatives View-Space-z → positiver Abstand),
// dem Rcase-Vergleich nur der Event-Tabelle und dem Standard bei Fehltreffer.
static int failures=0;
static void check(const char* what,ViewClip got,float near,float far){
    if(got.near!=near||got.far!=far){
        std::cerr<<what<<": got ("<<got.near<<","<<got.far<<") expected ("<<near<<","<<far<<")\n";++failures;}}
int main(){
    // bhChangeViewClip: Schlüssel (room, rcase, evc_no).
    check("event st0 room2 rcase0 evc1",event_view_clip(0,2,0,1),24.0f,5000.0f);
    check("event st0 room2 rcase0 evc4",event_view_clip(0,2,0,4),25.0f,20000.0f);
    check("event st3 room13 rcase0 evc28",event_view_clip(3,13,0,28),20.799999f,20000.0f);
    check("event st9 room9 rcase0 evc16",event_view_clip(9,9,0,16),35.0f,8000.0f);
    // Rcase muss treffen: dieselbe Zelle mit falschem rcase ist ein Fehltreffer.
    check("event st0 room2 rcase1 evc1 (rcase mismatch)",event_view_clip(0,2,1,1),2.0f,20000.0f);
    // Fehltreffer und Stage außerhalb 0..9 liefern den Ninja-Standard.
    check("event miss",event_view_clip(0,2,0,99),2.0f,20000.0f);
    check("event stage out of range",event_view_clip(10,2,0,1),2.0f,20000.0f);
    check("event display bit cleared, miss",event_view_clip(0,2,0,99,0),1.0f,99.0f);
    check("unrelated flags do not enable game range",event_view_clip(0,2,0,99,0x100),1.0f,99.0f);
    check("table hit overrides non-game default",event_view_clip(0,2,0,1,0),24.0f,5000.0f);
    check("Rcase miss uses non-game default",event_view_clip(0,2,1,1,0),1.0f,99.0f);
    check("out-of-range stage uses display default",event_view_clip(10,2,0,1,0),1.0f,99.0f);
    // bhChangeViewClipRM: Schlüssel (room, evc_no); rcase wird ignoriert.
    check("room st0 room0 cut1",room_view_clip(0,0,1),10.0f,1000.0f);
    check("room st0 room2 cut4 (rcase ignored)",room_view_clip(0,2,4),4.0f,20000.0f);
    check("room st9 room12 cut1",room_view_clip(9,12,1),3.4f,20000.0f);
    check("room miss",room_view_clip(0,0,99),2.0f,20000.0f);
    check("room stage out of range",room_view_clip(10,0,1),2.0f,20000.0f);
    check("monitor bypasses a matching room row",room_view_clip(0,0,1,true),1.0f,20000.0f);
    check("monitor with no matching row",room_view_clip(0,0,99,true),1.0f,20000.0f);
    check("monitor does not consult stage table",room_view_clip(10,0,1,true),1.0f,20000.0f);
    check("native monitor range",monitor_view_clip(),1.0f,20000.0f);
    check("camera return preserves room range",restored_view_clip({4,1000},0x200,0),4,1000);
    check("camera return uses eye-view range",restored_view_clip({4,1000},0x200,0x40),1,20000);
    check("non-game return overrides eye view",restored_view_clip({4,1000},0,0x40),1,99);
    // Positive raster checks: range selection must change visible geometry,
    // not merely return the expected pair of numbers.
    SoftwareRenderer image(64,64);image.set_supersampling(1);image.set_render_threads(1);
    NJS_MATRIX matrix{};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;
    auto visible=[&](float z,ViewClip clip){
        Scene scene;SceneTriangle t;t.color=0xffffffff;
        t.vertices[0].position={-.5f,-.5f,z};t.vertices[1].position={0,.5f,z};t.vertices[2].position={.5f,-.5f,z};
        scene.triangles.push_back(t);image.draw_view(scene,matrix,32*z,32*z,false,{},clip.near,clip.far);
        if(image.last_clip()[0]!=clip.near||image.last_clip()[1]!=clip.far)++failures;
        return image.covered_pixels()!=0;
    };
    if(visible(1.5f,room_view_clip(0,0,99))||!visible(1.5f,room_view_clip(0,0,1,true))){
        std::cerr<<"monitor near plane does not change the rendered geometry\n";++failures;}
    if(visible(150.f,event_view_clip(0,2,0,99,0))||!visible(150.f,event_view_clip(0,2,0,99,0x200))){
        std::cerr<<"display flag does not change far-plane clipping\n";++failures;}
    if(visible(200.f,event_view_clip(9,28,0,10))||!visible(150.f,event_view_clip(9,28,0,10))){
        std::cerr<<"original table far plane does not clip geometry\n";++failures;}
    // _Make_ClipVolume (ps2_NaView.c:298): nur wenn x UND y negativ sind, bleibt ClipVolume
    // 320/240 und ClipDispW/H werden -x/2, -y/2; sonst x/y mit ClipDisp 2047.
    auto volume=[&](const char* what,ClipVolume got,float x,float y,float w,float h){
        if(got.x!=x||got.y!=y||got.display_w!=w||got.display_h!=h){
            std::cerr<<what<<": got ("<<got.x<<","<<got.y<<","<<got.display_w<<","<<got.display_h<<") expected ("
                     <<x<<","<<y<<","<<w<<","<<h<<")\n";++failures;}};
    volume("ninja default",ClipVolume{},320,240,2047,2047);
    volume("make 1024/960",make_clip_volume(1024,960),1024,960,2047,2047);
    volume("make -320/-240",make_clip_volume(-320,-240),320,240,160,120);
    volume("make mixed sign takes the else branch",make_clip_volume(-320,240),-320,240,2047,2047);
    // bhChangeClipVolume (event.c:13560): Schluessel (room, rcase, evc_no).
    volume("event st3 room15 rcase0 evc3",event_clip_volume(3,15,0,3),320,240,160,120);
    volume("event st5 room0 rcase1 evc0",event_clip_volume(5,0,1,0),1024,960,2047,2047);
    volume("event st5 room0 rcase0 evc0 (rcase mismatch)",event_clip_volume(5,0,0,0),320,240,2047,2047);
    volume("event st6 room10 rcase0 evc30",event_clip_volume(6,10,0,30),1024,960,2047,2047);
    volume("event st9 room29 rcase0 evc0",event_clip_volume(9,29,0,0),320,240,160,120);
    volume("event st9 room35 rcase0 evc45",event_clip_volume(9,35,0,45),1024,960,2047,2047);
    volume("event stage without rows",event_clip_volume(0,7,0,6),320,240,2047,2047);
    volume("event miss",event_clip_volume(9,28,0,8),320,240,2047,2047);
    // bhChangeClipVolumeRM (event.c:13647): Schluessel (room, cam.ncut); rcase wird ignoriert.
    volume("room st0 room7 cut6",room_clip_volume(0,7,6),1024,960,2047,2047);
    volume("room st0 room9 cut2",room_clip_volume(0,9,2),1024,960,2047,2047);
    volume("room st0 room15 cut0",room_clip_volume(0,15,0),1740,1740,2047,2047);
    volume("room st7 room3 cut3 (rcase ignored)",room_clip_volume(7,3,3),1024,960,2047,2047);
    volume("room st9 room5 cut3",room_clip_volume(9,5,3),1740,1740,2047,2047);
    volume("room st0 room7 cut5 miss",room_clip_volume(0,7,5),320,240,2047,2047);
    volume("room st0 room2 cut4 (view clip only)",room_clip_volume(0,2,4),320,240,2047,2047);
    volume("room stage without rows",room_clip_volume(2,3,2),320,240,2047,2047);
    // ClipMatrix2 (_Make_ClipMatrix, ps2_NaView.c:233): m[0][0] = dist/x, m[1][1] = dist/y.
    // Bei dist 500 und z 100 liegt x 100 bei 500 Ninja-Einheiten: ausserhalb von 320,
    // innerhalb von 1024. y 60 liegt bei 300: ausserhalb von 240, innerhalb von 960.
    {const NJS_POINT3 right{100,0,100},down{0,60,100},center{0,0,100},near_point{0,0,0.5f};
     auto code=[&](NJS_POINT3 p,ClipVolume v){return strip_clip_code(p,500,v.x,v.y,2,20000);};
     if(code(right,ClipVolume{})!=1u||code(right,room_clip_volume(0,7,6))!=0u){std::cerr<<"ClipVolume x does not bound +x\n";++failures;}
     if(code(down,ClipVolume{})!=4u||code(down,room_clip_volume(0,7,6))!=0u){std::cerr<<"ClipVolume y does not bound +y\n";++failures;}
     if(code(NJS_POINT3{-100,-60,100},ClipVolume{})!=(2u|8u)){std::cerr<<"negative half planes wrong\n";++failures;}
     if(code(right,event_clip_volume(3,15,0,3))!=1u){std::cerr<<"negative table entry must keep ClipVolume 320/240\n";++failures;}
     if(code(center,room_clip_volume(0,15,0))!=0u||code(near_point,room_clip_volume(0,15,0))!=32u){std::cerr<<"z planes must not depend on ClipVolume\n";++failures;}
     // 1740 ist die groesste Tabellenzeile: 1740*AspectW 1.174 = 2042.8 liegt noch innerhalb
     // der Anzeigegrenze 2047 von _Check_DisplayAreaPoint (ps2_Vu1Strip.c:766).
     if(!(1740.0f*1.174000025f<2047.0f)){std::cerr<<"1740 volume reaches the display guard band\n";++failures;}}
    if(failures){std::cerr<<failures<<" view clip checks failed\n";return 1;}
    std::cout<<"View clip and clip volume tables match the original event.c ranges.\n";return 0;}

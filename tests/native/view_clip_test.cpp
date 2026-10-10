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
    if(failures){std::cerr<<failures<<" view clip checks failed\n";return 1;}
    std::cout<<"View clip tables match the original event.c ranges.\n";return 0;}

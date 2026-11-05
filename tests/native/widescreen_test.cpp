// Host-Mod Breitbild (widescreen.h): anamorphe Projektion wie der GameCube-Patch (BHD_ASPECT_X 1.174 *
// 0.75). Geprueft werden njProjectScreen (NaViewScreenMatrix), das Hauptbild von SoftwareRenderer::draw
// und dass ausgeschaltet die Originalwerte unveraendert bleiben. Die CAM_WRK-Versteckmasken (cut.c:620/641)
// gelten fuer 4:3: im Breitbild bleiben nur Objekte sichtbar, die ganz im erweiterten Streifen liegen.
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "renderer.h"
#include "widescreen.h"
#include <cmath>
#include <iostream>
#include <vector>
using namespace recvx;
static int failures=0;
static void check(bool ok,const char* what){if(!ok){std::cerr<<"widescreen: "<<what<<'\n';++failures;}}
// Spaltenmitte der vom Dreieck bedeckten Bildpunkte, -1 wenn nichts gezeichnet wurde.
static float drawn_column(float x){
    Scene scene;scene.background=0xff000000;
    SceneTriangle t;t.color=0xffffffff;
    t.vertices[0].position={x-1.0f,-1.0f,-100.0f};t.vertices[1].position={x+1.0f,-1.0f,-100.0f};t.vertices[2].position={x,1.0f,-100.0f};
    scene.triangles.push_back(t);
    SceneCamera camera;camera.clip_near=1.0f;camera.clip_far=1000.0f;
    SoftwareRenderer image(640,480);image.set_supersampling(1);image.draw(scene,camera);
    double sum=0;unsigned count=0;
    for(int y=0;y<480;++y)for(int c=0;c<640;++c)if(image.pixels()[std::size_t(y)*640+c]!=scene.background){sum+=c+0.5;++count;}
    return count?float(sum/count):-1.0f;
}
// Bildpunkte eines per Kameramaske versteckten Objekts 5, dessen Dreiecke bei den x-Werten liegen.
static unsigned hidden_object_pixels(std::initializer_list<float> xs,std::vector<unsigned>* revealed=nullptr){
    Scene scene;scene.background=0xff000000;
    for(float x:xs){
        SceneTriangle t;t.color=0xffffffff;t.camera_mask=true;t.object=5;
        t.vertices[0].position={x-1.0f,-1.0f,-100.0f};t.vertices[1].position={x+1.0f,-1.0f,-100.0f};t.vertices[2].position={x,1.0f,-100.0f};
        scene.triangles.push_back(t);
    }
    SceneCamera camera;camera.clip_near=1.0f;camera.clip_far=1000.0f;camera.hidden_objects[0]=0x80000000u>>5;
    if(revealed)*revealed=widescreen_revealed_objects(scene,camera);
    SoftwareRenderer image(640,480);image.set_supersampling(1);image.draw(scene,camera);
    unsigned count=0;for(auto c:image.pixels())count+=c!=scene.background;
    return count;
}
int main(){
    _Make_SinTable();
    check(recvx_widescreen_scale==1.0f&&recvx_aspect_x()==1.174f&&NaViewScreenMatrix[0]==fNaViwAspectW,"default must be the original 4:3 aspect");
    NJS_MATRIX unit;njUnitMatrix(&unit);NJS_POINT3 p{10.0f,5.0f,100.0f};NJS_POINT2 off,on;
    njProjectScreen(&unit,&p,&off);
    recvx_set_widescreen(1);
    check(recvx_aspect_x()==1.174f*0.75f&&NaViewScreenMatrix[0]==fNaViwAspectW*0.75f,"16:9 must scale NaViewScreenMatrix like the GameCube patch");
    check(fNaViwAspectW==1.174000025f,"njSetAspect value itself stays the original");
    njProjectScreen(&unit,&p,&on);
    check(std::fabs((on.x-fNaViwOffsetX)-(off.x-fNaViwOffsetX)*0.75f)<1e-3f&&on.y==off.y,"njProjectScreen: x offset 3/4, y unchanged");
    recvx_set_widescreen(0);
    check(NaViewScreenMatrix[0]==fNaViwAspectW,"switching off restores the original matrix");
    // Hauptbild: x = 20 liegt in 4:3 bei Spalte ~501, in 16:9 bei 320 + 3/4 des Abstands.
    const float c43=drawn_column(20.0f),edge43=drawn_column(40.0f);
    recvx_set_widescreen(1);
    const float c169=drawn_column(20.0f),edge169=drawn_column(40.0f);
    recvx_set_widescreen(0);
    check(c43>480.0f&&c43<520.0f,"4:3 reference column");
    check(std::fabs((c169-320.0f)-(c43-320.0f)*0.75f)<1.5f,"16:9 column must be 3/4 of the 4:3 offset");
    check(edge43<0.0f&&edge169>560.0f&&edge169<620.0f,"geometry right of the 4:3 edge must become visible in 16:9");
    check(drawn_column(20.0f)==c43,"switching off restores the original image");
    // 4:3-Rand bei z 100: x = 100*tan(pi/8)/1.174 = 35.3; der 16:9-Streifen reicht bis 47.
    std::vector<unsigned> revealed;
    check(hidden_object_pixels({40.0f},&revealed)==0&&revealed.empty(),"4:3: hide mask stays untouched");
    recvx_set_widescreen(1);
    check(hidden_object_pixels({40.0f},&revealed)>0&&revealed==std::vector<unsigned>{5},"16:9: hidden object entirely in the extra strip becomes visible");
    check(hidden_object_pixels({-40.0f})>0,"16:9: left strip as well");
    check(hidden_object_pixels({35.3f},&revealed)==0&&revealed.empty(),"16:9: object crossing the 4:3 edge stays hidden");
    check(hidden_object_pixels({20.0f})==0,"16:9: object inside the 4:3 image stays hidden");
    check(hidden_object_pixels({40.0f,-40.0f},&revealed)==0&&revealed.empty(),"16:9: object spanning both strips stays hidden");
    recvx_set_widescreen(0);
    if(failures)return 1;
    std::cout<<"widescreen mod: projection, main image, strip hide masks and reset passed\n";
    return 0;
}

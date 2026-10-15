#include "ui.h"
#include "asset_io.h"
#include "item_asset.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
// D03 Darstellung der Itemansicht: die originale Kameraentfernung beider
// Zoomphasen (CameraSet 20 - pos_0.z + pos_1.z, 3.2 je Durchlauf), der
// Absenkversatz der Öffnungsaktion (mw.pos.y um 0.05 bis -0.76) und die
// Modellskalierung (ModelScaleSet über npGetWHDSize). Geprüft wird die
// gerechnete Wirkung und, mit Datenpfad, das gezeichnete Bild.
static void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
namespace {
// Ein Item mit Öffnungsaktion und Dietrich im Bestand.
Inventory case_fixture(unsigned item){
    Inventory inv;std::fill(inv.slots()+1,inv.slots()+12,0);
    inv.slots()[2]=Inventory::pack(item,itemdata[item].max);inv.slots()[3]=Inventory::pack(50,1);
    return inv;
}
struct Drawn {unsigned pixels=0;float cx=0,cy=0;int top=480,bottom=-1;};
Drawn measure(const SoftwareRenderer& image){
    // Der Ansichtsrahmen liegt bei (60,132) und ist 352x196 gross; der
    // Hintergrund der Ansicht ist 0xff101010.
    Drawn out;double sx=0,sy=0;
    for(int y=132;y<132+196;++y)for(int x=60;x<60+352;++x){
        const auto pixel=image.pixels()[std::size_t(y)*640+x];
        if(pixel==0xff101010u)continue;
        ++out.pixels;sx+=x;sy+=y;out.top=std::min(out.top,y);out.bottom=std::max(out.bottom,y);
    }
    if(out.pixels){out.cx=float(sx/out.pixels);out.cy=float(sy/out.pixels);}
    return out;
}
}
int main(int argc,char** argv){try{
    _Make_SinTable();
    // CameraInit/CameraSet in Ruhe: 20 - 12 + 0.
    check(std::abs(inspection_rest_camera_distance()-8.f)<1e-4f,"rest camera distance differs from CameraInit/CameraSet");
    // Beide Zoomphasen bewegen die Kamera um 3.2 je Durchlauf über 32
    // Durchläufe; am Umschaltpunkt steht sie 8 + 32*3.2 entfernt.
    {auto inv=case_fixture(128);ItemInspection state;state.check(inv,2,{});
     check(state.question(),"case 128 does not ask its original question");
     state.answer(inv,true);check(state.phase()==4,"yes does not start the opening action");
     check(std::abs(state.camera_distance()-8.f)<1e-4f,"camera moved before the zoom phase");
     // Die Aktion senkt den Gegenstand, die Kamera bleibt stehen.
     const auto start=state.model_offset();unsigned action_ticks=0;
     while(state.phase()==4&&action_ticks<64){state.tick(inv);++action_ticks;
        check(std::abs(state.camera_distance()-8.f)<1e-4f,"camera moved during the opening action");}
     check(state.phase()==5,"opening action does not reach the zoom phase");
     const auto lowered=state.model_offset();
     check(lowered.x==start.x&&lowered.z==start.z,"the action moved the item sideways");
     check(std::abs(lowered.y-(-.76f))<1e-4f,"the action does not lower the item to -0.76, got "+std::to_string(lowered.y));
     check(start.y>lowered.y,"the item was not lowered at all");
     // Auszoomen: 32 Durchläufe, Entfernung wächst linear um 3.2.
     float previous=state.camera_distance();unsigned out_ticks=0;
     while(state.phase()==5&&out_ticks<64){state.tick(inv);++out_ticks;
        const auto now=state.camera_distance();
        if(state.phase()==5)check(std::abs(now-previous-3.2f)<1e-3f,"zoom-out step differs from 3.2");
        previous=now;}
     check(out_ticks==32,"zoom-out does not take 32 passes, got "+std::to_string(out_ticks));
     check(std::abs(previous-(8.f+32.f*3.2f))<1e-2f,"zoom-out does not reach the original switch distance, got "+std::to_string(previous));
     check(state.phase()==1,"zoom-out does not continue into the zoom-in phase");
     // Das Ergebnisitem startet auf seinem eigenen hoseipos.
     check(std::abs(state.model_offset().y-inspection_model_offset(35).y)<1e-4f,"the result item does not start on its own hoseipos");
     // Einzoomen: dieselben 32 Durchläufe zurück bis zur Ruhelage.
     unsigned in_ticks=0;while(state.running()&&in_ticks<64){state.tick(inv);++in_ticks;}
     check(in_ticks==32,"zoom-in does not take 32 passes, got "+std::to_string(in_ticks));
     check(std::abs(state.camera_distance()-8.f)<1e-2f,"zoom-in does not return to the rest distance");
     check(Inventory::id(inv.slots()[2])==35,"the transformation did not commit");}
    // ModelScaleSet: 1.52 geteilt durch die Vektorlänge der Achsenmaxima; acht
    // Lese-IDs benutzen 1.84.
    {const NJS_VECTOR whd{3,4,0};const float length=5;
     unsigned small_item=0,large_item=0;
     for(unsigned item=1;item<156&&(!small_item||!large_item);++item){
        const auto model=inspection_model_id(item);
        const bool large=model==1||model==2||model==3||model==4||model==6||model==10||model==11||model==103;
        if(large&&!large_item)large_item=item;else if(!large&&!small_item)small_item=item;}
     check(small_item,"no item uses the 1.52 scale base");
     check(std::abs(inspection_model_scale(small_item,whd)-1.52f/length)<1e-5f,"1.52 scale base differs");
     check(large_item,"no item uses the 1.84 scale base");
     check(std::abs(inspection_model_scale(large_item,whd)-1.84f/length)<1e-5f,"1.84 scale base differs");
     bool rejected=false;try{(void)inspection_model_scale(small_item,NJS_VECTOR{0,0,0});}catch(const std::exception&){rejected=true;}
     check(rejected,"a model without extent must not produce a scale");}
    if(argc>1){
        const auto data=std::filesystem::u8path(argv[1]);GameUi ui(data);MessageDialog dialog;
        const auto out=argc>2?std::filesystem::u8path(argv[2]):std::filesystem::path{};
        if(!out.empty())std::filesystem::create_directories(out);
        auto render=[&](Inventory& inv,StatusMenu& menu,const std::string& name){
            SoftwareRenderer image(640,480);ui.draw(image,inv,menu,dialog,0);
            if(!out.empty())image.save_bmp(out/(name+".bmp"));
            return measure(image);};
        // Ruhelage: das Modell steht sichtbar im Rahmen und füllt ihn nicht aus.
        auto inv=case_fixture(128);StatusMenu menu;menu.message_source=[&](unsigned index){return ui.system_message(index);};
        menu.active=menu.examining=true;menu.selected=2;
        const auto initial=inspection_default_angles(128);
        menu.examine_pitch=initial[0];menu.examine_yaw=initial[1];menu.examine_roll=initial[2];
        const auto rest=render(inv,menu,"examine-rest");
        check(rest.pixels>500,"the item view draws almost nothing at the original distance: "+std::to_string(rest.pixels));
        check(rest.pixels<352*196*9/10,"the item fills the whole view at the original distance");
        // Öffnungsaktion: der Absenkversatz schiebt das Modell nach unten.
        menu.examine_pitch=menu.examine_yaw=menu.examine_roll=0;
        menu.input(MenuInput::Confirm,inv);
        // bhControlMessage-Durchlaeufe: erst die aufgebaute Seite bzw. offene Auswahl nimmt OK an (message.c:228-337).
        const auto reveal=[&]{for(unsigned k=0;k<400&&menu.message.active()&&!menu.message.closable()&&!menu.message.choosing()&&!menu.message.turnable();++k)menu.message.update();};
        unsigned guard=0;reveal();while(menu.message.active()&&!menu.message.choices()&&++guard<30){menu.input(MenuInput::Confirm,inv);reveal();}
        check(menu.message.choices(),"the case question did not offer a choice");
        reveal();menu.input(MenuInput::Confirm,inv);
        const auto opening_start=render(inv,menu,"examine-action-0");
        for(unsigned i=0;i<16;++i)menu.tick(inv);
        const auto opening_mid=render(inv,menu,"examine-action-16");
        check(menu.inspection.running(),"the opening action ended before the comparison");
        // Die Öffnungsaktion dreht den Deckel auf und verschiebt den
        // Gegenstand gleichzeitig entlang -y; beides vergrößert die Silhouette.
        check(opening_mid.pixels>opening_start.pixels,"the opening action does not change the drawn silhouette");
        check(opening_mid.top<opening_start.top&&opening_mid.bottom>opening_start.bottom,"the opening action does not move the drawn edges: "+std::to_string(opening_start.top)+".."+std::to_string(opening_start.bottom)+" -> "+std::to_string(opening_mid.top)+".."+std::to_string(opening_mid.bottom));
        // Auszoomen: in der grössten Entfernung bleibt deutlich weniger Fläche.
        while(menu.inspection.phase()!=5&&menu.inspection.running())menu.tick(inv);
        for(unsigned i=0;i<31;++i)menu.tick(inv);
        const auto far=render(inv,menu,"examine-zoom-far");
        check(menu.inspection.running(),"the zoom phase ended before the comparison");
        check(far.pixels*4<rest.pixels,"the item does not recede over the original 32 zoom passes: "+std::to_string(far.pixels)+" vs "+std::to_string(rest.pixels));
        // Richtung des Versatzes isoliert: Items 33 und 136 benutzen dasselbe
        // Modell, dieselbe Sichtbarkeitsmaske und dasselbe Skalierungsflag,
        // aber hoseipos y -0.43 gegen 0. Die Itemansicht benutzt die
        // Identitätsbasis (kein (1,-1,-1) wie die Raumkamera), und der
        // Renderer bildet größeres y auf größeres Bildschirm-y ab; ein
        // negativerer Versatz muss also höher erscheinen. Dieselbe Richtung
        // nimmt die Öffnungsaktion mit ihrem Weg nach -0.76.
        {check(inspection_model_id(33)==inspection_model_id(136),"items 33/136 no longer share their model");
         const auto low=inspection_model_offset(33),high=inspection_model_offset(136);
         check(low.y<high.y&&low.x==high.x&&low.z==high.z,"the 33/136 offsets no longer differ in y only");
         auto draw_pickup=[&](unsigned item,const std::string& name){
            Inventory hold;StatusMenu show;show.message_source=[&](unsigned index){return ui.system_message(index);};
            show.active=true;show.pickup=item;show.examine_pitch=show.examine_yaw=show.examine_roll=0;
            return render(hold,show,name);};
         const auto lowered_draw=draw_pickup(33,"offset-low"),raised_draw=draw_pickup(136,"offset-high");
         check(lowered_draw.pixels>500&&raised_draw.pixels>500,"the offset comparison frames draw nothing");
         check(lowered_draw.cy+1.f<raised_draw.cy,"a more negative hoseipos must draw higher in the identity-basis item view: "+std::to_string(raised_draw.cy)+" -> "+std::to_string(lowered_draw.cy));}
        // Nach dem Durchlauf steht das Ergebnis wieder in Ruhelage.
        for(unsigned i=0;i<120&&menu.inspection.running();++i)menu.tick(inv);
        check(!menu.inspection.running()&&inv.contains(35),"the transformation did not finish through the menu");
        const auto result=render(inv,menu,"examine-result");
        check(result.pixels>500,"the result item is not drawn at the rest distance");
        std::cout<<"Item view: original camera distance (8 .. "<<(8.f+32.f*3.2f)<<"), action offset to -0.76, ModelScaleSet bases and the drawn rest/lowered/far/result frames verified.\n";
        return 0;
    }
    std::cout<<"Item view: original camera distance, action offset and ModelScaleSet bases verified; pass the data path to check the drawn frames.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"Item view test failed: "<<e.what()<<'\n';return 1;}}

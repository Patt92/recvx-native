#include "inspection.h"
#include "binary_view.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace recvx {
// Untersuchung und Dateibildschirm, eigenständig nativ (D03).
// Semantikreferenz: itemview.c (ItemModelCheck, ItemModelMessageWait,
// ItemModelActionSet, ItemModelChangeZoomIn/Out, MdlDirChk, MdlAction00/01/02,
// MdlEvalflagsSet, MdlHideCheck, FlagErase, LighterOpen, DrawSubItem),
// fileview.c (FileNumberSwitch, PlayPageCheck, FileScrollSet/PageScroll für die
// Seitenzählung, ReadFstx für den Hintergrund) und sub1.c (Namensauswahl im
// Statusbildschirm). Kein Funktionskörper übernommen, kein Originalkern.
// Die reinen Datentabellen sind übernommen und als solche gekennzeichnet.
namespace {
// fileview.c: Seitenzahl je Datei (Low-Nibble ist die letzte Seite, gezählt ab
// 0), Startoffset der Seitentexte und die Hintergrundnummer im ITEM-Archiv.
constexpr unsigned kFilePages[24]={
    13, 4, 4, 5,  5, 7, 3, 6,  8, 4, 4,13,
     1, 1, 6, 7,  9, 5, 4, 4,  6,11, 7, 6};
constexpr unsigned kFileHeader[24]={
      0, 14, 19, 24,  30, 36, 44, 48,  55, 64, 69, 74,
     88, 90, 92, 99, 107,117,123,128, 133,140,152,160};
constexpr unsigned kFileWallpaper[24]={
    159,157,151,148, 150,153,158,151, 147,146,146,146,
    155,154,152,147, 147,156,149,149, 148,149,152,152};
// bhDispMessage-Basis der Seitentexte: bufnum = (Seite + 280) + fsheader[Datei].
constexpr unsigned kPageMessageBase=280;
// FileNumberSwitch als reine Zuordnung Raum/Item -> Datei. roomno = rom_no +
// stg_no*100. Raum 551 liefert die Datei unabhängig vom Item (und setzt dort
// ssd_flg 0x1000, das MakeTag als Besitz von Datei 11 zeigt; dieser Raumeffekt
// liegt beim Aufrufer, siehe FileFolder::open(bonus)).
struct FileEntry {unsigned room,item,file;};
constexpr FileEntry kFileItems[]={
    {  7, 48, 8},{ 11, 47, 9},{108, 47,10},{207,135,12},{207,102,18},{204,134,13},
    {106,133,14},{601, 48,15},{901, 48,15},{604, 48,16},{604,132,17},{607,132,17},
    {907,132,17},{702,102,19},{922, 49,20},{904,102,21},{904, 48,16},{919,133,22},
    {918,133,23},{  4,132, 1},{  9, 49, 3},{313,132, 4},{718,132, 4},{104, 58, 5},
    {929,125, 6},{712,132, 7}};
constexpr unsigned kRoom551File=11;
// DrawSubItem subcsr 3: nach der Umwandlung startet die Ansicht für diese drei
// Items mit festen Winkeln statt mit der dsptbl-Pose.
constexpr bool fixed_exit_pose(unsigned item){return item==105||item==30||item==35;}
// Eulerwinkel der transponierten X/Y/Z-Drehung, wie inspection_default_angles
// sie aus dsptbl zieht.
std::array<Angle,3> euler_from_rotations(Angle ax,Angle ay,Angle az){
    NJS_MATRIX m;njUnitMatrix(&m);njRotateX(&m,ax);njRotateY(&m,ay);njRotateZ(&m,az);njTransposeMatrix(&m);
    if(std::abs(m[2])>.999999f)return {0,m[2]<0?16384:49152,int(10430.381f*std::atan2(-m[4],m[5]))&65535};
    return {int(10430.381f*std::atan2(m[6],m[10]))&65535,
            int(10430.381f*std::asin(std::clamp(-m[2],-1.f,1.f)))&65535,
            int(10430.381f*std::atan2(m[1],m[0]))&65535};
}
// MdlDirChk: dirang[0] ist die halbe Breite des zulässigen Fensters, dirang[1]
// seine Mitte. Die 16-Bit-Verkürzung des Originals bleibt erhalten.
bool direction_in_window(const short* window,int model_angle){
    if(window[0]==0)return true;
    const std::int16_t shifted=std::int16_t((model_angle+window[0])&0xffff);
    const int difference=int(shifted)-int(window[1]);
    return (difference&0xffff)<=int(window[0])*2;
}
// ItemModelCheck: diese Items beantworten die Richtungsprüfung nie.
bool never_inspectable(unsigned item){return item==19||item==33||item==34||item==136||item==142;}
// itemflg-Spalten: 0 Sichtbarkeitsmaske, 1 Aktionsroutine (0 keine, 1..3),
// 2 hoseipos, 3 Skalierung, 4 c_a-Startzeile | Anzahl<<4, 5 mestbl-Offset,
// 6 Ergebnisindex für idsettbl/getmestbl/itemidtbl.
unsigned hide_row(unsigned item){return dsptbl[item].hide;}
// ItemModelActionSet: (actionprg-1)[itemflg[...][1]] -> MdlAction00/01/02. Die
// Deckelachse ist bei MdlAction02 Z, sonst X; die Knotentiefe unterscheidet
// MdlAction01 nach Modell.
unsigned action_axis(unsigned action){return action==3?2u:0u;}
unsigned action_depth(unsigned action,unsigned item){
    if(action!=2)return 3;
    if(item==128)return 5;
    if(item==63)return 4;
    return 3;
}
constexpr int kLidStep=1456,kCounterStep=728,kLidEnd=32768;
// MdlAction00/01/02: mw.pos.y sinkt um 0.05 je Durchlauf bis -0.76.
constexpr float kActionDrop=.05f,kActionFloor=-.76f;
// CameraInit/CameraSet und beide Zoomphasen.
constexpr float kCameraRest=12.f,kCameraDis=20.f,kZoomStep=3.2f;
unsigned lid_steps(){return unsigned((kLidEnd+kLidStep-1)/kLidStep);} // 23
// Knotenbaum eines Itempakets (MDL ab Offset 4, 52 Byte je Objekt).
struct NodeTree {
    BinaryView view;std::size_t base=0,objects=0;unsigned count=0;
    std::vector<unsigned> child,sibling;
    explicit NodeTree(const std::vector<std::uint8_t>& packet):view(packet){
        base=view.relative(4,view.u16(8),0);count=view.u16(10);
        if(!count||count>4096)throw std::runtime_error("invalid item model object count");
        objects=view.relative(base,view.u32(16),std::size_t(count)*52);
        child.assign(count,~0u);sibling.assign(count,~0u);
        for(unsigned i=0;i<count;++i){
            const auto node=objects+std::size_t(i)*52;
            child[i]=link(view.u32(node+44));sibling[i]=link(view.u32(node+48));
        }
    }
    unsigned link(std::uint32_t offset)const{
        if(offset==0xffffffffu)return ~0u;
        const auto at=base+offset;
        if(at<objects||(at-objects)%52||(at-objects)/52>=count)return ~0u;
        return unsigned((at-objects)/52);
    }
    std::size_t at(unsigned index)const{return objects+std::size_t(index)*52;}
    unsigned descend(unsigned depth)const{unsigned node=0;for(unsigned i=0;i<depth;++i){if(node==~0u)return ~0u;node=child[node];}return node;}
};
// MdlHideCheck: Bit je Knoten, Kind und Geschwister erhalten dieselbe
// verdoppelte Maske; das Bit wandert mit der Baumtiefe. chk ist im Original ein
// unsigned char und läuft nach acht Schritten über.
void hide_check(std::vector<std::uint32_t>& flags,const NodeTree& tree,unsigned node,unsigned el,unsigned chk,unsigned depth=0){
    if(node==~0u||depth>256)return;
    if(el&chk){
        flags[node]|=8;
        el&=~chk;chk=(chk*2)&0xff;
        if(el){hide_check(flags,tree,tree.child[node],el,chk,depth+1);hide_check(flags,tree,tree.sibling[node],el,chk,depth+1);}
    }else if(el){
        chk=(chk*2)&0xff;
        hide_check(flags,tree,tree.child[node],el,chk,depth+1);
        hide_check(flags,tree,tree.sibling[node],el,chk,depth+1);
    }
}
}
// --- Untersuchungsautomat ---
struct ItemInspection::State {
    // S_WORK testmode: 2 Ruhe, 6 Frage, 4 Aktion, 5 Auszoomen, 1 Einzoomen,
    // 7 Dateibildschirm (Ergebnis 85).
    unsigned phase=2,source=0,item=0,mesid=0,result=0,index=0,slot=0;
    unsigned action=0,action_state=0,action_frame=0,count=0,spin_ticks=0;
    // STCAM_WRK pos_0.z / pos_1.z und SITEM.mw.pos.
    float camera_z=12.f,target_z=0.f;NJS_POINT3 offset{};
    bool message=false;
    std::optional<std::array<Angle,3>> display_reset;
};
ItemInspection::ItemInspection():state_(std::make_shared<State>()){}
void ItemInspection::check(Inventory& inventory,unsigned slot,std::array<Angle,3> angles){
    // ItemModelCheck mit gedrückter Bestätigung: Richtungsfenster, Dietrich,
    // Meldung und der Übergang in Frage oder Aktion.
    if(slot<1||slot>=12||!Inventory::id(inventory.slots()[slot]))throw std::runtime_error("invalid examination slot");
    const auto item=Inventory::id(inventory.slots()[slot]);
    if(item>=156)throw std::runtime_error("invalid examination item");
    state_=std::make_shared<State>();
    auto& s=*state_;s.slot=slot;s.item=s.source=item;s.offset=inspection_model_offset(item);
    const auto row=hide_row(item);const unsigned windows=itemflg[row][4];
    const unsigned first=windows&0xf,rows=windows>>4;
    unsigned matched=0,hit=0;
    if(first){
        for(unsigned x=0;x<rows&&hit!=3;++x){
            const short* window=c_a[first+x];hit=0;
            for(unsigned axis=0;axis<3;++axis)hit+=direction_in_window(window+axis*2,angles[axis])?1u:0u;
            matched=x;
        }
    }
    if(never_inspectable(item))hit=0;
    if(hit!=3){
        // Kein Fenster getroffen: der Gegenstand meldet seinen eigenen Text.
        // Nur der leere Feuerlöscher (82 mit Low-Byte 0) meldet 140.
        if(item!=82||(inventory.slots()[slot]&0xff))s.index=item;
        else s.index=140;
        s.message=true;return;
    }
    const unsigned entry=mestbl[matched+itemflg[row][5]];
    unsigned message=entry&0xfff,kind=entry&0xf000;
    if(message==181){
        // Verschlossener Behälter: ohne Dietrich 188 und kein Vorgang.
        if(inventory.search(50)){s.index=message;s.message=true;}
        else{s.index=188;s.message=true;kind=0;}
    }else{
        if(item!=106){s.index=message;s.message=true;}
        if(message==72)inventory.slots()[slot]|=0x80000000u; // bekannter Name
    }
    if(kind==0x2000){s.mesid=item;s.phase=4;s.action=itemflg[row][1];s.action_state=0;}
    else if(kind)s.phase=6;
}
void ItemInspection::answer(Inventory& inventory,bool yes){
    // ItemModelMessageWait: Ja startet die Aktion, falls das Item eine hat,
    // sonst direkt das Auszoomen. Nein kehrt in die Ansicht zurück.
    (void)inventory;auto& s=*state_;
    if(s.phase!=6)return;
    if(!yes){s.phase=2;return;}
    s.mesid=s.item;s.action=itemflg[hide_row(s.item)][1];
    if(s.action){s.phase=4;s.action_state=0;}
    else{s.phase=5;s.count=32;s.spin_ticks=0;}
}
void ItemInspection::cancel(Inventory& inventory,unsigned slot){
    // ItemModelCheck mit Abbrechen: der Magnumkasten normalisiert seine
    // Munition auf Item 13, bevor die Ansicht schließt.
    if(slot>=1&&slot<12&&Inventory::id(inventory.slots()[slot])==30){
        if(const auto found=inventory.search(30))inventory.slots()[found]=Inventory::pack(13,itemdata[13].max);
    }
    state_=std::make_shared<State>();
}
void ItemInspection::tick(Inventory& inventory){
    auto& s=*state_;
    switch(s.phase){
    case 4:{
        // MdlAction00 verbringt den ersten Durchlauf ohne Bewegung (maincsr
        // 0 -> 2), MdlAction01/02 bewegen bereits im ersten. Fertig ist die
        // Aktion, sobald der Deckelwinkel 32768 erreicht; ein weiterer
        // Durchlauf meldet das Ende.
        const bool first_idle=s.action==1;
        if(s.action_state==0){s.action_state=first_idle?2u:1u;if(first_idle)return;}
        if(s.action_state==(first_idle?2u:1u)){
            ++s.action_frame;
            // Alle drei Aktionsroutinen senken den Gegenstand um 0.05 je
            // Durchlauf, bis -0.76 erreicht ist.
            if(s.offset.y>kActionFloor){s.offset.y-=kActionDrop;if(s.offset.y<kActionFloor)s.offset.y=kActionFloor;}
            if(int(s.action_frame)*kLidStep>=kLidEnd)s.action_state=first_idle?3u:2u;
            return;
        }
        s.phase=5;s.count=32;s.spin_ticks=0;return;
    }
    case 5:
        // ItemModelChangeZoomOut: 32 Durchläufe mit 3.2 Kamerabewegung je
        // Durchlauf, dann steht das Ergebnisitem mit seiner Startpose da.
        ++s.spin_ticks;s.camera_z-=kZoomStep;
        if(--s.count)return;
        s.mesid=itemflg[hide_row(s.mesid)][6];
        s.result=idsettbl[s.mesid];
        if(s.result>=156)throw std::runtime_error("original transformation result outside the item table");
        s.item=s.source=s.result;s.action_frame=0;
        // Das Ergebnis startet auf seinem eigenen hoseipos, der Blick wieder
        // ohne Anwendernähe (pos_1.z = 0).
        s.offset=inspection_model_offset(s.result);s.target_z=0.f;
        s.display_reset=fixed_exit_pose(s.result)
            ?(s.result==105?euler_from_rotations(50245,51519,1457)
             :s.result==30?euler_from_rotations(50245,57709,42235)
             :euler_from_rotations(21846,0,0))
            :inspection_default_angles(s.result);
        s.phase=1;s.count=32;s.spin_ticks=0;return;
    case 1:
        // ItemModelChangeZoomIn: 32 Durchläufe zurück, dann steht das Ergebnis
        // im Slot, seine Meldung kommt aus getmestbl. Ergebnis 85 wechselt in
        // den Dateibildschirm.
        ++s.spin_ticks;s.camera_z+=kZoomStep;
        if(--s.count)return;
        if(s.slot>=1&&s.slot<12)inventory.slots()[s.slot]=Inventory::pack(s.result,itemdata[s.result].max);
        s.index=getmestbl[s.mesid];s.message=true;
        s.phase=s.result==85?7u:2u;return;
    default:return;
    }
}
std::optional<std::array<Angle,3>> ItemInspection::take_display_reset(){auto result=state_->display_reset;state_->display_reset.reset();return result;}
bool ItemInspection::question()const{return state_->phase==6;}
bool ItemInspection::running()const{return phase()==4||phase()==5||phase()==1;}
bool ItemInspection::message_pending()const{return state_->message;}
unsigned ItemInspection::take_message(){state_->message=false;return state_->index;}
unsigned ItemInspection::source()const{return state_->source;}
unsigned ItemInspection::action_frame()const{return state_->action_frame;}
unsigned ItemInspection::item()const{return state_->item;}
unsigned ItemInspection::model()const{return inspection_model_id(state_->item);}
unsigned ItemInspection::phase()const{return state_->phase;}
unsigned ItemInspection::completed_file()const{return phase()==7?original_file_number(85):24u;}
float ItemInspection::camera_distance()const{
    const auto& s=*state_;
    return kCameraDis-s.camera_z+s.target_z;
}
NJS_POINT3 ItemInspection::model_offset()const{
    const auto& s=*state_;
    return s.item?s.offset:NJS_POINT3{};
}
// mw.ay1 += 4095, mw.az1 += 2047 je Zoomdurchlauf.
std::array<Angle,3> ItemInspection::spin()const{
    const auto& s=*state_;
    if(s.phase!=1&&s.phase!=5)return {0,0,0};
    return {0,int(4095u*s.spin_ticks)&65535,int(2047u*s.spin_ticks)&65535};
}
// --- Tabellen- und Matrixarbeit ---
unsigned inspection_action(unsigned item){if(item>=156)throw std::runtime_error("invalid item action");return itemflg[dsptbl[item].hide][1];}
std::array<Angle,3> inspection_default_angles(unsigned item){
    if(item>=156)throw std::runtime_error("invalid display item");
    // DrawSubItem subcsr 0/1: transpose the negative DSP_WORK X/Y/Z
    // rotations. These inspection angles describe rotmat, not a room camera.
    const auto& d=dsptbl[item];
    return euler_from_rotations(int(-d.ax*182.04445f)&65535,int(-d.ay*182.04445f)&65535,int(-d.az*182.04445f)&65535);
}
void inspection_rotation(NJS_MATRIX& matrix,std::array<Angle,3> angles,std::array<Angle,3> spin){
    njUnitMatrix(&matrix);njRotateXYZ(&matrix,angles[0],angles[1],angles[2]);
    // DrawSubItem applies the action rotations after rotmat, in X/Y/Z order.
    njRotateX(&matrix,spin[0]);njRotateY(&matrix,spin[1]);njRotateZ(&matrix,spin[2]);
}
unsigned inspection_model_id(unsigned item){if(item>=156)throw std::runtime_error("invalid inspection item");const auto model=itemidtbl[itemflg[dsptbl[item].hide][6]];const auto id=model?model:item;return id==131?5:id==136?33:id;}
ModelGeometryOptions inspection_geometry(const std::vector<std::uint8_t>& packet,unsigned item,unsigned frame){
    if(item>=156)throw std::runtime_error("invalid inspection item");
    NodeTree tree(packet);
    ModelGeometryOptions options;
    // FlagErase löscht an jedem Knoten evalflags 0x1 und 0x2, Position und
    // Winkel wirken danach immer. MdlEvalflagsSet versteckt anschließend die
    // Teilknoten der Maske — aber nur, wenn dsptbl.hide gesetzt ist.
    options.evalflags.resize(tree.count);
    for(unsigned i=0;i<tree.count;++i)options.evalflags[i]=tree.view.u32(tree.at(i))&~0x3u;
    const auto row=hide_row(item);
    if(row)hide_check(options.evalflags,tree,0,itemflg[row][0],1);
    const unsigned action=itemflg[row][1];
    const bool lighter=item==55;
    // ModelScaleSet mit itemflg[...][3]: Wurzel und erster Kindknoten werden
    // vor dem Messen auf Position 0 gesetzt; das verschiebt auch das Modell.
    const bool zero_roots=itemflg[row][3]!=0;
    if(!frame&&!lighter&&!zero_roots)return options;
    options.pose.resize(tree.count);
    for(unsigned i=0;i<tree.count;++i){
        const auto node=tree.at(i);
        options.pose[i]={{tree.view.f32(node+8),tree.view.f32(node+12),tree.view.f32(node+16)},
                         {tree.view.i32(node+20),tree.view.i32(node+24),tree.view.i32(node+28)}};
    }
    if(zero_roots){options.pose[0].position={0,0,0};const auto first=tree.descend(1);if(first!=~0u)options.pose[first].position={0,0,0};}
    // LighterOpen: der Deckel des Feuerzeugs steht in der Ansicht offen.
    if(lighter){const auto node=tree.descend(2);if(node!=~0u)options.pose[node].angles[2]=29127;}
    if(frame&&action){
        const auto steps=std::min(frame,lid_steps());
        const auto axis=action_axis(action);
        const auto lid=tree.descend(action_depth(action,item)),hinge=tree.descend(1);
        if(lid!=~0u)options.pose[lid].angles[axis]+=int(steps)*kLidStep;
        if(hinge!=~0u)options.pose[hinge].angles[axis]-=int(steps)*kCounterStep;
    }
    return options;
}
// --- Dateizuordnung und -tabellen ---
unsigned original_file_number(unsigned item,unsigned stage,unsigned room){
    if(item>=256)throw std::runtime_error("invalid file item");
    const auto roomno=room+stage*100;
    unsigned file=0;
    if(roomno==551)file=kRoom551File;
    else for(const auto& entry:kFileItems)if(entry.room==roomno&&entry.item==item){file=entry.file;break;}
    if(item==85)file=2; // TG-01 trägt seine Datei in jedem Raum
    return file;
}
unsigned original_file_model_mask(unsigned item){
    // Items, die nur als Raummodell einer Datei vorkommen. Die Datei des Raums
    // 551 hängt an keinem Item, TG-01 ist ein Inventaritem.
    if(item==85||item>=256)return 0;
    unsigned mask=0;
    for(const auto& entry:kFileItems)if(entry.item==item)mask|=1u<<entry.file;
    return mask;
}
unsigned original_file_pages(unsigned file){
    if(file>=24)throw std::runtime_error("file outside table");
    return (kFilePages[file]&0xf)+1; // Seiten 0..fstbl
}
unsigned original_file_message(unsigned file,unsigned page){
    if(file>=24)throw std::runtime_error("file outside table");
    if(page>=original_file_pages(file))throw std::runtime_error("file page outside its original page count");
    return kPageMessageBase+kFileHeader[file]+page;
}
unsigned original_file_wallpaper(unsigned file){
    if(file>=24)throw std::runtime_error("file outside table");
    return kFileWallpaper[file];
}
NJS_POINT3 inspection_model_offset(unsigned item){
    if(item>=156)throw std::runtime_error("invalid inspection item");
    const auto index=itemflg[hide_row(item)][2];
    if(index>=10)throw std::runtime_error("hoseipos index outside the original table");
    return hoseipos[index];
}
float inspection_model_scale(unsigned item,const NJS_VECTOR& whd){
    if(item>=156)throw std::runtime_error("invalid inspection item");
    // ModelScaleSet: 1.84 für die acht grossen Lese-IDs, sonst 1.52.
    const auto model=inspection_model_id(item);
    const bool large=model==1||model==2||model==3||model==4||model==6||model==10||model==11||model==103;
    NJS_VECTOR size=whd;const auto length=njScalor(&size);
    if(!(length>0))throw std::runtime_error("item model has no measurable extent");
    return (large?1.84f:1.52f)/length;
}
float inspection_rest_camera_distance(){return kCameraDis-kCameraRest;}
unsigned inventory_name_id(std::uint32_t item){
    // Statusbildschirm: das unbekannte Objekt trägt nach der Untersuchung
    // seinen Namen (Bit 0x80000000), der leere Feuerlöscher einen eigenen, und
    // das Handbuch den Titel seiner Datei.
    const auto id=Inventory::id(item);
    if(id==72)return (item&0x80000000u)?158u:72u;
    if(id==82)return (item&0xff)?82u:149u;
    if(id==145)return 159u;
    return id;
}
}

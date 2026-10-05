#include "inspection.h"
#include "binary_view.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace recvx {
namespace original_inventory {
struct NJS_CNK_OBJECT {unsigned evalflags=0;NJS_POINT3 pos{};int ang[3]{};NJS_CNK_OBJECT* child=nullptr;NJS_CNK_OBJECT* sibling=nullptr;};
struct S_WORK {unsigned* pip=nullptr;unsigned listcsr_0=0,itemid=0,mesid=0,ips1=0,testmode=2,maincsr=0,mode=8,wn=1,subcsr=0,statusflg=0,dnum=0,wn_num=0;int actioncount=0;};
struct System {unsigned st_flg=0,pad_ps=0,pad_on=0,cb_flg=0,mes_sel=0,rom_no=0,stg_no=0,ssd_flg=0;};
struct SITEM {struct {NJS_CNK_OBJECT* objP=nullptr;} mdl;struct {NJS_POINT3 pos{};int ax1=0,ay1=0,az1=0;unsigned rdid=0;std::array<Angle,3> ang{};}mw;};
struct STCAM_WRK {int cax=0,cay=0,caz=0;NJS_POINT3 pos_0{},pos_1{};NJS_MATRIX rotmat{};};
static thread_local S_WORK swork;static thread_local System system;static thread_local System* sys=&system;static thread_local SITEM sitem;static thread_local STCAM_WRK st_cam;
static thread_local unsigned* wp_=nullptr;static thread_local unsigned char flg[3]{};static thread_local float testf=1;static thread_local bool has_message=false;static thread_local unsigned message=0;
void MdlHideCheck(NJS_CNK_OBJECT*,unsigned char,unsigned char);unsigned char MdlAction00(SITEM*);unsigned char MdlAction01(SITEM*);unsigned char MdlAction02(SITEM*);
struct Part {struct {float r=1,g=1,b=1;} col;};
static thread_local Part parts_10b[16];
static unsigned char (*actionprg[3])(SITEM*)={MdlAction00,MdlAction01,MdlAction02};
static void bhSetMessage(int,unsigned index){has_message=true;message=index;sys->st_flg|=0x200;}
static void MdlEvalflagsSet(unsigned group){MdlHideCheck(sitem.mdl.objP,itemflg[group][0],1);}
// Host framing measures the decoded visible mesh; retain original scale-reset
// side effects here. No item identity/action rule is implemented in this binding.
static float ModelScaleSet(SITEM* item,int reset){if(reset){item->mdl.objP->pos={};if(item->mdl.objP->child)item->mdl.objP->child->pos={};}return 1;}
#include "inventory_engine.inc"
}
struct ItemInspection::State {
    original_inventory::S_WORK work;original_inventory::System system;original_inventory::SITEM item;original_inventory::STCAM_WRK camera;
    std::array<original_inventory::NJS_CNK_OBJECT,6> nodes{};unsigned* pointer=nullptr;bool message=false;unsigned index=0,source=0,action_frame=0;std::optional<std::array<Angle,3>> display_reset;
    State(){for(unsigned i=0;i+1<nodes.size();++i)nodes[i].child=&nodes[i+1];item.mdl.objP=nodes.data();}
    void enter(Inventory& inventory){using namespace original_inventory;work.pip=inventory.slots();swork=work;system.st_flg&=~0x200u;original_inventory::system=system;sys=&original_inventory::system;sitem=item;st_cam=camera;wp_=pointer;has_message=false;}
    void leave(){using namespace original_inventory;work=swork;system=original_inventory::system;item=sitem;camera=st_cam;pointer=wp_;if(has_message){message=true;index=original_inventory::message;}}
};
ItemInspection::ItemInspection():state_(std::make_shared<State>()){}
void ItemInspection::check(Inventory& inventory,unsigned slot,std::array<Angle,3> angles){
    if(slot<1||slot>=12||!Inventory::id(inventory.slots()[slot]))throw std::runtime_error("invalid examination slot");state_=std::make_shared<State>();auto& s=*state_;s.source=s.work.itemid=Inventory::id(inventory.slots()[slot]);s.work.listcsr_0=slot;s.item.mw.ang=angles;s.item.mw.rdid=inspection_model_id(s.source);s.system.pad_ps=0x800;s.enter(inventory);original_inventory::ItemModelCheck();s.leave();
}
void ItemInspection::answer(Inventory& inventory,bool yes){auto& s=*state_;if(!question())return;s.system.pad_ps=0;s.system.st_flg|=0x4000;s.system.cb_flg|=0x1000;s.system.mes_sel=yes?100:101;s.enter(inventory);original_inventory::ItemModelMessageWait();s.leave();s.system.st_flg&=~0x4000u;}
void ItemInspection::cancel(Inventory& inventory,unsigned slot){state_=std::make_shared<State>();auto& s=*state_;s.source=s.work.itemid=Inventory::id(inventory.slots()[slot]);s.work.listcsr_0=slot;s.work.testmode=2;s.system.pad_ps=0x1000;s.enter(inventory);original_inventory::ItemModelCheck();s.leave();s.work.testmode=2;}
void ItemInspection::tick(Inventory& inventory){auto& s=*state_;if(!running())return;const auto before=phase();s.system.pad_ps=0;s.enter(inventory);switch(s.work.testmode){case 4:++s.action_frame;original_inventory::ItemModelActionSet();break;case 5:original_inventory::ItemModelChangeZoomOut();break;case 1:original_inventory::ItemModelChangeZoomIn();break;default:break;}s.leave();
    if(before==5&&phase()==1){if(s.work.subcsr==3){
        // DrawSubItem's original subcsr 3 presets, after ChangeZoomOut has
        // reset rotmat. Preserve the returned item pose through its message.
        NJS_MATRIX m;njUnitMatrix(&m);const auto item=s.work.itemid;
        const std::array<Angle,3> preset=item==105?std::array<Angle,3>{50245,51519,1457}:item==30?std::array<Angle,3>{50245,57709,42235}:std::array<Angle,3>{21846,0,0};
        njRotateX(&m,preset[0]);njRotateY(&m,preset[1]);njRotateZ(&m,preset[2]);njTransposeMatrix(&m);
        s.display_reset=std::array<Angle,3>{int(10430.381f*std::atan2(m[6],m[10]))&65535,int(10430.381f*std::asin(std::clamp(-m[2],-1.f,1.f)))&65535,int(10430.381f*std::atan2(m[1],m[0]))&65535};
    }else s.display_reset=inspection_default_angles(s.work.itemid);}}
std::optional<std::array<Angle,3>> ItemInspection::take_display_reset(){auto result=state_->display_reset;state_->display_reset.reset();return result;}
bool ItemInspection::question()const{return state_->work.testmode==6;}
bool ItemInspection::running()const{return phase()==4||phase()==5||phase()==1;}
bool ItemInspection::message_pending()const{return state_->message;}
unsigned ItemInspection::take_message(){state_->message=false;return state_->index;}
unsigned ItemInspection::source()const{return state_->source;}
unsigned ItemInspection::action_frame()const{return state_->action_frame;}
unsigned ItemInspection::item()const{return state_->work.itemid;}
unsigned ItemInspection::model()const{return inspection_model_id(state_->work.itemid);}
unsigned ItemInspection::phase()const{return state_->work.testmode;}
unsigned ItemInspection::completed_file()const{return phase()==7?2u:24u;}
float ItemInspection::zoom()const{return phase()==5?1.f-float(32-state_->work.actioncount)/32:phase()==1?float(32-state_->work.actioncount)/32:1.f;}
std::array<Angle,3> ItemInspection::spin()const{return {state_->item.mw.ax1,state_->item.mw.ay1,state_->item.mw.az1};}
unsigned inventory_name_id(std::uint32_t item){return original_inventory::item_name_id(item);}
unsigned inspection_action(unsigned item){if(item>=156)throw std::runtime_error("invalid item action");return itemflg[dsptbl[item].hide][1];}
std::array<Angle,3> inspection_default_angles(unsigned item){
    if(item>=156)throw std::runtime_error("invalid display item");
    // DrawSubItem subcsr 0/1: transpose the negative DSP_WORK X/Y/Z
    // rotations. These inspection angles describe rotmat, not a room camera.
    const auto& d=dsptbl[item];NJS_MATRIX m;njUnitMatrix(&m);
    njRotateX(&m,int(-d.ax*182.04445f)&65535);njRotateY(&m,int(-d.ay*182.04445f)&65535);njRotateZ(&m,int(-d.az*182.04445f)&65535);njTransposeMatrix(&m);
    // At +/-90 degrees Y, X and Z are coupled. Keep a stable equivalent
    // pose instead of magnifying the PS2 sine table's tiny residual cos(Y).
    if(std::abs(m[2])>.999999f)return {0,m[2]<0?16384:49152,int(10430.381f*std::atan2(-m[4],m[5]))&65535};
    return {int(10430.381f*std::atan2(m[6],m[10]))&65535,int(10430.381f*std::asin(std::clamp(-m[2],-1.f,1.f)))&65535,int(10430.381f*std::atan2(m[1],m[0]))&65535};
}
void inspection_rotation(NJS_MATRIX& matrix,std::array<Angle,3> angles,std::array<Angle,3> spin){
    njUnitMatrix(&matrix);njRotateXYZ(&matrix,angles[0],angles[1],angles[2]);
    // DrawSubItem applies the action rotations after rotmat, in X/Y/Z order.
    njRotateX(&matrix,spin[0]);njRotateY(&matrix,spin[1]);njRotateZ(&matrix,spin[2]);
}
unsigned inspection_model_id(unsigned item){if(item>=156)throw std::runtime_error("invalid inspection item");const auto model=itemidtbl[itemflg[dsptbl[item].hide][6]];const auto id=model?model:item;return id==131?5:id==136?33:id;}
ModelGeometryOptions inspection_geometry(const std::vector<std::uint8_t>& packet,unsigned item,unsigned frame){
    BinaryView v(packet);const auto base=v.relative(4,v.u16(8),0);const auto count=v.u16(10);const auto nodes=v.relative(base,v.u32(16),count*52);if(!count||count>256)throw std::runtime_error("invalid item model count");
    std::vector<original_inventory::NJS_CNK_OBJECT> objects(count);ModelGeometryOptions result;
    auto link=[&](unsigned offset)->original_inventory::NJS_CNK_OBJECT*{if(offset==0xffffffff)return nullptr;const auto p=v.relative(base,offset,52);if(p<nodes||(p-nodes)%52||(p-nodes)/52>=count)throw std::runtime_error("invalid item model link");return &objects[(p-nodes)/52];};
    for(unsigned i=0;i<count;++i){const auto p=nodes+i*52;auto& o=objects[i];o.evalflags=v.u32(p);o.pos={v.f32(p+8),v.f32(p+12),v.f32(p+16)};for(unsigned a=0;a<3;++a)o.ang[a]=v.i32(p+20+a*4);o.child=link(v.u32(p+44));o.sibling=link(v.u32(p+48));}
    // Validate the packet's graph before invoking original recursive functions.
    (void)decode_model_asset(packet,4,0);original_inventory::FlagErase(objects.data());original_inventory::MdlHideCheck(objects.data(),itemflg[dsptbl[item].hide][0],1);
    const auto action=inspection_action(item);if(frame&&action){unsigned depth=action==2?(item==128?5:item==63?4:3):3;auto* node=objects.data();for(unsigned i=0;i<depth;++i){if(!node->child)throw std::runtime_error("item action hierarchy is incomplete");node=node->child;}
        original_inventory::SITEM si;si.mdl.objP=objects.data();const auto saved=original_inventory::swork;original_inventory::swork={};original_inventory::swork.itemid=item;
        for(unsigned i=0;i<std::min(frame,64u);++i)if(original_inventory::actionprg[action-1](&si))break;original_inventory::swork=saved;
    }
    for(const auto& o:objects){result.pose.push_back({o.pos,{o.ang[0],o.ang[1],o.ang[2]}});result.evalflags.push_back(o.evalflags);}return result;
}
unsigned original_file_number(unsigned item,unsigned stage,unsigned room){original_inventory::system.stg_no=stage;original_inventory::system.rom_no=room;return original_inventory::FileNumberSwitch(item);}
unsigned original_file_pages(unsigned file){if(file>=24)throw std::runtime_error("file outside table");return original_inventory::fstbl[file]+1;}
unsigned original_file_message(unsigned file,unsigned page){if(page>=original_file_pages(file))throw std::runtime_error("file page outside table");return 280+original_inventory::fsheader[file]+page;}
unsigned original_file_wallpaper(unsigned file){if(file>=24)throw std::runtime_error("file outside table");return original_inventory::wallpaper[file];}
}

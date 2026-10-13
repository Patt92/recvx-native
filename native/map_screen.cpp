#include "map_screen.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace recvx {
namespace {
/* NJS_MATRIX is Float[16]; std::array<Float,16> has the same layout, so the
   stack can hold it and hand out NJS_MATRIX* to the ninja_matrix routines.
   The map screen keeps its own stack instead of njPushMatrixEx so it never
   disturbs the stack the renderer and world use. */
using MatrixStorage=std::array<Float,16>;
NJS_MATRIX* as_matrix(MatrixStorage& m){return reinterpret_cast<NJS_MATRIX*>(m.data());}
struct Stack {
    std::vector<MatrixStorage> entries;
    void push(){entries.push_back(entries.empty()?MatrixStorage{}:entries.back());}
    void push(const NJS_MATRIX& m){MatrixStorage s{};std::memcpy(s.data(),m,sizeof(MatrixStorage));entries.push_back(s);}
    void pop(){if(!entries.empty())entries.pop_back();}
    NJS_MATRIX* top(){if(entries.empty())push();return as_matrix(entries.back());}
};
void copy_matrix(NJS_MATRIX& dest,const NJS_MATRIX& source){std::memcpy(dest,source,sizeof(NJS_MATRIX));}
std::int16_t read_s16(const std::vector<std::uint8_t>& data,std::size_t offset){
    if(offset+2>data.size())throw std::runtime_error("map code outside the loaded file");
    return std::int16_t(std::uint16_t(data[offset])|std::uint16_t(data[offset+1])<<8);
}
std::uint32_t read_u32(const std::vector<std::uint8_t>& data,std::size_t offset){
    if(offset+4>data.size())throw std::runtime_error("map chunk outside the loaded file");
    return std::uint32_t(data[offset])|std::uint32_t(data[offset+1])<<8|std::uint32_t(data[offset+2])<<16|std::uint32_t(data[offset+3])<<24;
}
constexpr std::uint32_t kMpac=0x4341504Du;  /* 'MPAC', map.c:378/470 */
constexpr std::uint32_t kMdl=0x4C444Du;     /* 'MDL', map.c:386 low 24 bits */
} // namespace

std::uint32_t map_argb_to_color(const RECVX_MAP_ARGB& argb){
    /* MapCnvArgb2Color (map.c:2563): each channel scaled by 255 and truncated
       by the NJS_COLOR byte fields. */
    const auto channel=[](float value){return std::uint32_t(std::uint8_t(std::int32_t(255.0f*value)));};
    return channel(argb.a)<<24|channel(argb.r)<<16|channel(argb.g)<<8|channel(argb.b);
}

/* ---------------------------------------------------------------- lifecycle */

void MapScreen::init(int set_mod,const MapHost& host,const MapServices& services){
    /* bhInitMap (map.c:64). npSetMemory clears the whole work first, so every
       field starts at zero and only the original assignments follow. */
    /* Resource revisions belong to this native owner, not the cleared
       original map work. Keep them monotonic so a renderer surviving exit
       cannot reuse the previous player's/index/floor assets on re-entry. */
    const unsigned index_revision=index_generation_+1,stage_revision=stage_generation_+1;
    *this=MapScreen{};
    index_generation_=index_revision;stage_generation_=stage_revision;
    host_=host;services_=services;
    status_=(1<<set_mod)&7;
    ply_stg_=host.stg_no;
    ply_rom_=host.rom_no+host.stg_no*100;
    vew_max_.y=600.0f;vew_min_.y=600.0f;
    dst_zom_=600.0f;vew_zom_=600.0f;
    vew_ang_={-16384,0,0};
    bck_depth_=-1600.0f;
    bck_p0_[0]=0;bck_p0_[1]=0;
    bck_p1_[0]=640.0f;bck_p1_[1]=480.0f;
    ar_=ag_=ab_=0;pr_=pg_=pb_=0.6f;
    pitch_=-16384;yaw_=0;
    map_scale_=0.001f;
    if(status_&0x2){
        bnk_mde_=host.mp_prm[0];
        bnk_stg_=host.mp_prm[1];
        bnk_flr_=host.mp_prm[2];
        bnk_rom_=host.mp_prm[3]+bnk_stg_*100;
    }
    /* map.c:121: without status bit 2 the room texture memory is saved and
       given back in bhExitMap. Natively the room texture list stays loaded,
       so this is resource preservation, not a texture-memory copy. */
    restores_room_texture_=!(status_&0x4);
    func_init(24);
    convert_status_to_flags(host,services);
    prti_no_=host.dor_partid;
    if(!(status_&0x4)&&services_.call_system_se){services_.call_system_se(0,3);sounds_.push_back({0,3});}
    else if(!(status_&0x4))sounds_.push_back({0,3});
}

void MapScreen::exit(){
    /* bhExitMap (map.c:179). */
    released_map_texture_=bool(status_&0x8);
    released_stage_texture_=bool(status_&0x10);
    game_map_flag_cleared_=true;
}

/* ------------------------------------------------------- flags and inventory */

void MapScreen::convert_status_to_flags(const MapHost& host,const MapServices& services){
    /* MapCnvStatus2Flag (map.c:256). */
    int id=host.ply_id;
    if(id>2){
        if(id==3)id=2;
        else if(id==4)id=0;
        else id=0;
    }
    if(!services.flag_set)throw std::runtime_error("map screen needs the native flag owner");
    services.flag_set(8,1,1);
    services.flag_set(8,2,1);
    services.flag_set(8,3,1);
    services.flag_set(8,unsigned(id)+1,0);
    for(int chk=0;chk<22;++chk)services.flag_set(9,recvx_map_itm_dat[chk].flg_no,1);
    const std::uint32_t* base=services.inventory_slots?services.inventory_slots():nullptr;
    if(!base)throw std::runtime_error("map screen needs the native inventory owner");
    for(int chk=0;chk<22;++chk){
        const auto& entry=recvx_map_itm_dat[chk];
        for(int slot=0;slot<16;++slot){
            const unsigned item=(base[slot]>>16)&0xFF;
            if(item==entry.itm_no){services.flag_set(9,entry.flg_no,0);break;}
        }
    }
}

/* ------------------------------------------------------------- file handling */

int MapScreen::read_map_data(unsigned file){
    /* bhReadMapData (map.c:208). The native file service delivers the whole
       file in one call, which is the original fil_mode 0 -> 2 -> status 0
       sequence without the DMA wait; a file the partition cannot open is the
       original RequestReadInsideFile != 0 path (fil_mode 1, code -1). */
    switch(fil_mode_){
    case 0:{
        if(!services_.read_inside_file)throw std::runtime_error("map screen needs the archive reader");
        requested_files_.push_back(file);
        buffer_=services_.read_inside_file(prti_no_,file);
        if(buffer_.empty()){fil_mode_=1;return -1;}
        fil_mode_=2;
        return 0;
    }
    case 1:return -1;
    case 2:return 1;
    default:return 0;
    }
}

void MapScreen::analyze_index(){
    /* MP_MOD_IDX_ANALYZE (map.c:375). MRY/000<ply_id>: 'MPAC', a byte offset
       to the first chunk, then a chain of chunks whose first word is the byte
       distance from the chunk payload to the next chunk header: the marker
       models ('MDL'), the map sprite texture, ten message blocks and finally
       the floor table MapGetFloorNo reads. */
    marker_models_.clear();
    if(buffer_.size()<8||read_u32(buffer_,0)!=kMpac)return;
    std::size_t cursor=4+read_u32(buffer_,4);
    std::size_t next=read_u32(buffer_,cursor);
    cursor+=4;
    while(cursor+4<=buffer_.size()&&(read_u32(buffer_,cursor)&0xFFFFFFu)==kMdl){
        marker_models_.push_back({cursor,next});
        cursor+=next;
        if(cursor+4>buffer_.size())return;
        next=read_u32(buffer_,cursor);
        cursor+=4;
    }
    status_|=0x8;
    map_texture_={cursor,next==~0u?buffer_.size()-cursor:next};
    if(next!=~0u){
        cursor+=next;
        if(cursor+4>buffer_.size())return;
        next=read_u32(buffer_,cursor);
        cursor+=4;
    }
    for(int i=0;i<10;++i){
        mes_buf_[i]=cursor;
        if(next!=~0u){
            cursor+=next;
            if(cursor+4>buffer_.size())return;
            next=read_u32(buffer_,cursor);
            cursor+=4;
        }
    }
    index_floor_table_=cursor;
    const int packed=get_floor_no(buffer_.data()+index_floor_table_,buffer_.size()-index_floor_table_,ply_rom_,host_.player_y);
    if(packed==-1){map_no_=-1;ply_flr_=-1;}
    else{map_no_=(unsigned(packed)>>16)&0xFFFF;ply_flr_=packed&0xFFFF;}
    /* map.c:420: `map_bufP = pacP` advances the load pointer past the index
       so the floor file lands behind it and the marker models, the map
       texture and the message blocks stay in memory. The native reader
       delivers whole files, so the index bytes move to their own buffer and
       the chunk spans above keep addressing them. */
    index_buffer_=std::move(buffer_);
    buffer_.clear();
    ++index_generation_;
}

int MapScreen::get_floor_no(const std::uint8_t* data,std::size_t size,int rom_no,float pos_y){
    /* MapGetFloorNo (map.c:1207): binary search over FT_WRK {float label,
       short map_no, short flr_no} with label = 100 * rom_no + pos_y. */
    const auto s16=[&](std::size_t index)->std::int16_t{
        if(index*2+2>size)throw std::runtime_error("map floor table outside the loaded file");
        return std::int16_t(std::uint16_t(data[index*2])|std::uint16_t(data[index*2+1])<<8);
    };
    const std::size_t table=std::size_t(std::uint16_t(s16(2)));
    const int count=s16(3);
    if(count<2)return -1;
    const auto label=[&](int index)->float{
        const std::size_t offset=table+std::size_t(index)*8;
        if(offset+8>size)throw std::runtime_error("map floor entry outside the loaded file");
        float value=0;std::uint32_t raw=std::uint32_t(data[offset])|std::uint32_t(data[offset+1])<<8|
            std::uint32_t(data[offset+2])<<16|std::uint32_t(data[offset+3])<<24;
        std::memcpy(&value,&raw,sizeof value);return value;
    };
    const auto entry=[&](int index,int field)->std::int16_t{
        const std::size_t offset=table+std::size_t(index)*8+4+std::size_t(field)*2;
        return std::int16_t(std::uint16_t(data[offset])|std::uint16_t(data[offset+1])<<8);
    };
    int low=0,high=count-1;
    const float key=100.0f*float(rom_no);
    int middle=0;
    do{
        middle=(low+high)/2;
        if(key+pos_y<=label(middle))high=middle;
        else low=middle;
    }while((high-low)!=1);
    if(label(high)<100.0f*float(rom_no))return -1;
    return (int(entry(high,0))<<16)|int(std::uint16_t(entry(high,1)));
}

void MapScreen::analyze_stage(){
    /* MP_MOD_DAT_SET (map.c:464): 'MPAC', the offset to the opcode stream,
       then the stage map model and its texture. */
    map_code_=0;stage_model_={};stage_texture_={};
    if(buffer_.size()<8||read_u32(buffer_,0)!=kMpac){map_mode_=RECVX_MP_MOD_WAIT_NOMAP;++stage_generation_;return;}
    std::size_t cursor=4+read_u32(buffer_,4);
    if(cursor+4>buffer_.size()){map_mode_=RECVX_MP_MOD_WAIT_NOMAP;++stage_generation_;return;}
    std::size_t next=read_u32(buffer_,cursor);
    cursor+=4;
    map_code_=cursor;
    if(next!=~0u){
        cursor+=next;
        if(cursor+4>buffer_.size()){map_mode_=RECVX_MP_MOD_WAIT_NOMAP;++stage_generation_;return;}
        next=read_u32(buffer_,cursor);
        cursor+=4;
        stage_model_={cursor,next==~0u?buffer_.size()-cursor:next};
        if(next!=~0u){
            status_|=0x10;
            /* map.c:482 adds four bytes: the texture follows the model chunk header. */
            stage_texture_={cursor+next+4,buffer_.size()>cursor+next+4?buffer_.size()-(cursor+next+4):0};
        }
    }
    func_alloc(ProcMapDraw,0);
    tag_init();
    map_mode_=RECVX_MP_MOD_VEW_NORMAL;
    ++stage_generation_;
}

void MapScreen::set_map_objects(std::vector<MapObject> objects){
    objects_=std::move(objects);
    purge_tree();
    cur_obj_=-1;
}

void MapScreen::purge_tree(){
    /* MapPurgeTree (map.c:1245): walks the object array in order, keeping the
       matrix stack for siblings, and rewrites every node to its absolute
       translation and Euler angles; child and sibling links are dropped. */
    Stack stack;stack.push();njUnitMatrix(stack.top());
    MatrixStorage local{};
    for(std::size_t index=0;index<objects_.size();++index){
        auto& object=objects_[index];
        if(object.sibling!=-1)stack.push();
        njTranslate(stack.top(),object.pos[0],object.pos[1],object.pos[2]);
        njRotateXYZ(stack.top(),object.ang[0],object.ang[1],object.ang[2]);
        std::memcpy(local.data(),*stack.top(),sizeof local);
        const Float* m=local.data();
        object.ang[0]=std::int32_t(10430.381f*std::atan2(m[6],m[10]));
        object.ang[1]=std::int32_t(10430.381f*std::asin(-m[2]));
        object.ang[2]=std::int32_t(10430.381f*std::atan2(m[1],m[0]));
        object.pos[0]=m[12];object.pos[1]=m[13];object.pos[2]=m[14];
        if(object.child==-1)stack.pop();
        object.child=-1;object.sibling=-1;
    }
}

/* -------------------------------------------------------- function/task list */

void MapScreen::func_init(int func_num){
    /* MapFuncInit (map.c:1291): slot 0 is the busy ring head, slot 1 the free
       ring head, slots 2.. are inserted into the free ring. */
    funcs_.assign(std::size_t(func_num)+2,Func{});
    funcs_[kBusy].next=funcs_[kBusy].prev=kBusy;
    funcs_[kFree].next=funcs_[kFree].prev=kFree;
    for(int index=2;index<func_num+2;++index)func_ins(kFree,index);
}

void MapScreen::func_ins(int base,int index){
    /* MapFuncIns (map.c:1352): insert directly behind the head, so the newest
       entry is executed first. */
    funcs_[index].next=funcs_[base].next;
    funcs_[index].prev=base;
    funcs_[funcs_[base].next].prev=index;
    funcs_[base].next=index;
}

void MapScreen::func_del(int index){
    funcs_[funcs_[index].prev].next=funcs_[index].next;
    funcs_[funcs_[index].next].prev=funcs_[index].prev;
}

int MapScreen::func_alloc(int procedure,int param0){
    /* MapFuncAlloc (map.c:1312). */
    const int index=funcs_[kFree].next;
    if(index==kFree)return -1;
    func_del(index);
    func_ins(kBusy,index);
    auto& func=funcs_[index];
    func.procedure=procedure;
    func.mode=0;
    func.w={};
    func.w[0].i=param0;
    func.allocated=true;
    return index;
}

void MapScreen::func_free(int index){
    if(index<2||index>=int(funcs_.size()))return;
    funcs_[index].allocated=false;
    funcs_[index].procedure=-1;
    func_del(index);
    func_ins(kFree,index);
}

bool MapScreen::func_live(int index)const{
    return index>=2&&index<int(funcs_.size())&&funcs_[index].allocated;
}

void MapScreen::func_exec(){
    /* MapFuncExec (map.c:1362): the successor is captured before the call so a
       procedure may free itself. */
    for(int current=funcs_[kBusy].next;current!=kBusy;){
        const int following=funcs_[current].next;
        if(funcs_[current].procedure>=0)run_procedure(current);
        current=following;
    }
}

int MapScreen::entry_task(int task,int chg_mde,int param0){
    /* MapEntryTask (map.c:1719): FT_WORK aliases the func slot - param0 is the
       map_mode pointer, param1 the mode to switch to, param2 the task, and
       FreeWrk[0] (w[3]) the task's own param0. */
    const int index=func_alloc(ProcTaskMain,0);
    if(index<0)return -1;
    auto& func=funcs_[index];
    func.w[1].i=chg_mde;
    func.w[2].i=task;
    func.w[3].i=param0;
    return index;
}

void MapScreen::entry_sprite(int set_no,int mode){
    /* MapEntrySprite (map.c:1420). */
    const auto& set=recvx_map_spr_set[set_no];
    int procedure=ProcSpriteDraw;
    switch(set.function){
    case RECVX_MAP_FSPR_SILHOUETTE:procedure=ProcSilhouetteDraw;break;
    case RECVX_MAP_FSPR_ARROW:procedure=ProcArrowDraw;break;
    case RECVX_MAP_FSPR_ARROW2:procedure=ProcArrowDraw2;break;
    default:procedure=ProcSpriteDraw;break;
    }
    const int index=func_alloc(procedure,0);
    if(index<0)return;
    auto& func=funcs_[index];
    func.w[1].i=set.act_bit;
    func.w[2].i=set.spr_no;
    func.w[3].u=set.spr_col;
    func.w[4].f=set.x;func.w[5].f=set.y;func.w[6].f=set.z;
    func.w[7].i=mode;
}

/* ------------------------------------------------------------------- control */

bool MapScreen::control(const MapHost& host){
    /* bhControlMap (map.c:319). */
    host_=host;
    draws_.clear();
    sounds_.clear();
    bool running=true;
    const auto se=[&](int type,int number){
        if(services_.call_system_se)services_.call_system_se(type,number);
        sounds_.push_back({type,number});
    };
    switch(map_mode_){
    case RECVX_MP_MOD_FIRST:
        if(services_.set_screen_fade)services_.set_screen_fade(host.fade_pbk,8.0f);
        func_alloc(ProcBackDraw,0);
        func_alloc(ProcZoomScreen,0);
        entry_task(TaskWait,RECVX_MP_MOD_IDX_READ,0);
        map_mode_=RECVX_MP_MOD_WAIT;
        connection_init(11,6);
        if(game_mode(host.stg_no)==1)connection_connect(recvx_map_cnc_dat_a,81,services_);
        else connection_connect(recvx_map_cnc_dat_b,49,services_);
        break;
    case RECVX_MP_MOD_IDX_READ:
        /* njClipZ(-2, -20000), njSetAspect(1.174, 1), njSetScreen dist 500,
           640x480 centred at 320/240: the clip/aspect/screen contract the
           integrator applies to the camera (map.c:355). */
        entry_task(TaskRead,RECVX_MP_MOD_IDX_ANALYZE,host.ply_id);
        map_mode_=RECVX_MP_MOD_WAIT;
        break;
    case RECVX_MP_MOD_IDX_ANALYZE:
        analyze_index();
        if(status_&0x8){
            entry_sprite(RECVX_MP_SET_SILHOUETTE,0);
            func_alloc(ProcCompass,0);
            func_alloc(ProcModeMessage,0);
        }
        if(status_&0x2){stg_no_=bnk_stg_;rom_no_=bnk_rom_;flr_no_=bnk_flr_;}
        else{stg_no_=map_no_;rom_no_=ply_rom_;flr_no_=ply_flr_;}
        [[fallthrough]];
    case RECVX_MP_MOD_MAP_READ:
        status_&=~0x40;
        status_&=~0x100;
        map_flr_=flr_no_;
        if(status_&0x10){status_&=~0x10;released_stage_texture_=true;}
        fil_mode_=0;
        entry_task(TaskRead,RECVX_MP_MOD_DAT_SET,(map_flr_+stg_no_*6)+5);
        map_mode_=RECVX_MP_MOD_WAIT;
        break;
    case RECVX_MP_MOD_DAT_SET:
        analyze_stage();
        break;
    case RECVX_MP_MOD_VEW_NORMAL:
        entry_task(TaskNormal,RECVX_MP_MOD_VEW_ZOOM,0);
        map_mode_=RECVX_MP_MOD_WAIT_NORMAL;
        break;
    case RECVX_MP_MOD_VEW_ZOOM:
        entry_task(TaskZoom,RECVX_MP_MOD_VEW_NORMAL,0);
        map_mode_=RECVX_MP_MOD_WAIT_ZOOM;
        break;
    case RECVX_MP_MOD_WAIT_NOMAP:
        draw_message(ply_rom_,160.0f,220.0f);
        [[fallthrough]];
    case RECVX_MP_MOD_WAIT_NORMAL:
    case RECVX_MP_MOD_WAIT_ZOOM:
        vew_pos_.x+=0.5f*(dst_pos_.x-vew_pos_.x);
        vew_zom_+=0.5f*(dst_zom_-vew_zom_);
        vew_pos_.z+=0.5f*(dst_pos_.z-vew_pos_.z);
        if(pad_ps_&0x3000){
            se(0,0);
            if(services_.set_screen_fade)services_.set_screen_fade(0xFF000000u,8.0f);
            map_mode_=RECVX_MP_MOD_WAIT;
            entry_task(TaskExit,RECVX_MP_MOD_EXIT,0);
            status_|=0x400;
        }
        break;
    case RECVX_MP_MOD_EXIT:
        running=false;
        break;
    default:break;
    }
    ++time_;
    pad_main();
    view_main();
    light_main();
    palette_main();
    func_exec();
    return running;
}

void MapScreen::pad_main(){
    /* MapPadMain (map.c:569). */
    if(status_&0x400){
        pad_ps_=0;pad_ax_=pad_ay_=0;pad_al_=pad_ar_=0;
        return;
    }
    pad_ps_=host_.pad_ps;
    pad_al_=host_.pad_al;
    pad_ar_=host_.pad_ar;
    pad_ax_=(host_.pad_ax>40.0f||host_.pad_ax<-40.0f)?host_.pad_ax:0.0f;
    pad_ay_=(host_.pad_ay>40.0f||host_.pad_ay<-40.0f)?host_.pad_ay:0.0f;
}

void MapScreen::view_main(){
    /* MapViewMain (map.c:605). */
    njUnitMatrix(&vew_mtx_);
    njRotateX(&vew_mtx_,32768);
    njTranslate(&vew_mtx_,0,0,-vew_zom_);
    njRotateZ(&vew_mtx_,-vew_ang_[2]);
    njRotateX(&vew_mtx_,-vew_ang_[0]);
    njRotateY(&vew_mtx_,-vew_ang_[1]);
    njTranslate(&vew_mtx_,-vew_pos_.x,-vew_pos_.y,-vew_pos_.z);
}

void MapScreen::light_main(){
    /* MapLightMain (map.c:625): njCnkSetEasyMultiLightSwitch(1,1), ambient
       (ar,ag,ab), lgt_scale = map_scale, the light vector from yaw/pitch, the
       colour (pr,pg,pb) / map_scale and njCnkSetEasyMultiLightMatrices with
       the view matrix pushed. njCnkSetEasyMultiLightVector keeps -v
       (ps2_NinjaCnk.c:328-336); the matrices routine rotates it with the
       translation removed (:365-391), which njCalcVector does. */
    lgt_scale_=map_scale_;
    const int yaw=yaw_,pitch=pitch_;
    const float scl=1.0f/map_scale_;
    NJS_VECTOR v{-njSin(yaw)*njCos(pitch),njSin(pitch),-njCos(yaw)*njCos(pitch)};
    light_world_={-v.x,-v.y,-v.z};
    light_color_={pr_*scl,pg_*scl,pb_*scl};
    ambient_={ar_,ag_,ab_};
    njCalcVector(&vew_mtx_,&light_world_,&light_camera_);
}

void MapScreen::palette_main(){
    /* MapPaletteMain (map.c:651): the first 16 entries are the plain table,
       entries 16..31 pulse in four groups of four with 1024/2048/4096 per
       tick; the last group scales alpha as well. */
    for(int i=0;i<32;++i)pal_[i]=recvx_map_pal[i];
    for(int i=0;i<3;++i)col_[i]=recvx_map_col[i];
    int index=16;
    const auto group=[&](int step,bool fixed_time,bool scale_alpha){
        for(int i=4;i>0;--i,++index){
            const auto& source=recvx_map_pal[index];
            auto& dest=pal_[index];
            const float rate=0.3333f*(2.0f+njSin(fixed_time?time_*step:(time_+i*4)*step));
            dest.a=scale_alpha?source.a*rate:source.a;
            dest.r=source.r*rate;
            dest.g=source.g*rate;
            dest.b=source.b*rate;
        }
    };
    group(1024,false,false);
    group(2048,false,false);
    group(4096,false,false);
    group(4096,true,true);
}

void MapScreen::bool_set(int value,int mod){
    /* MapBoolSet (map.c:1022). */
    switch(mod){
    case 0:cde_bol_=value;break;
    case 0x4000:cde_bol_&=value;break;
    case 0x8000:cde_bol_|=value;break;
    default:break;
    }
}

void MapScreen::code_process(){
    /* MapCodeProcess (map.c:718). `ocP` runs over shorts of the opcode stream;
       the loop ends on the full value 2, not on the masked opcode. The high
       nibble selects the MapBoolSet mode. */
    if(!map_code_)return;
    Stack stack;
    stack.push();
    njUnitMatrix(stack.top());
    njScale(stack.top(),1.0f,lgt_scale_,1.0f);
    std::size_t pc=0;
    const auto s=[&](std::size_t index){return read_s16(buffer_,map_code_+index*2);};
    int mode=0;
    unsigned guard=0;
    while((mode=s(pc))!=2){
        ++pc;
        if(++guard>0x20000u)throw std::runtime_error("map opcode stream does not terminate");
        switch(mode&0xFFF){
        case 3:{
            const int object=s(pc++);
            cur_obj_=object;
            stack.push();
            if(object>=0&&object<int(objects_.size())){
                const auto& node=objects_[std::size_t(object)];
                njTranslate(stack.top(),node.pos[0],node.pos[1],node.pos[2]);
                njRotateXYZ(stack.top(),node.ang[0],node.ang[1],node.ang[2]);
            }
            copy_matrix(cur_mtx_,*stack.top());
            stack.pop();
            break;
        }
        case 4:{
            int palette=s(pc++);
            MatrixStorage world{};
            std::memcpy(world.data(),vew_mtx_,sizeof world);
            njMultiMatrix(as_matrix(world),&cur_mtx_);
            const bool has_model=cur_obj_>=0&&cur_obj_<int(objects_.size())&&objects_[std::size_t(cur_obj_)].has_model;
            if(has_model){
                if((status_&0x200)&&chk_rom_==bnk_tag_rom_){
                    if(palette==1||palette==2||palette==24)palette=bnk_tag_pal_;
                    else if(palette==7||palette==8||palette==25)palette=bnk_tag_pal_wal_;
                }
                MapDraw draw;
                draw.kind=MapDraw::Kind::MapObject;
                draw.model=cur_obj_;
                draw.palette=palette;
                if(palette>=0&&palette<32)draw.material=pal_[std::size_t(palette)];
                std::memcpy(draw.matrix,world.data(),sizeof draw.matrix);
                draws_.push_back(draw);
            }
            break;
        }
        case 5:{
            NJS_POINT3 pos{};
            const int marker=s(pc);
            pos.x=float(s(pc+1))/32.0f;
            pos.y=float(s(pc+2))/32.0f;
            pos.z=float(s(pc+3))/32.0f;
            pc+=4;
            draw_marker(marker,pos,-1);
            break;
        }
        case 7:{
            const int ax=s(pc),ay=s(pc+1),az=s(pc+2);
            pc+=3;
            njRotateXYZ(&cur_mtx_,ax,ay,az);
            break;
        }
        case 6:{
            const float tx=float(s(pc))/32.0f,ty=float(s(pc+1))/32.0f,tz=float(s(pc+2))/32.0f;
            pc+=3;
            njTranslate(&cur_mtx_,tx,ty,tz);
            break;
        }
        case 11:{
            const int check=s(pc++);
            bool_set((ply_flr_==map_flr_&&rom_no_==check)?1:0,mode&0xF000);
            chk_rom_=check;
            break;
        }
        case 12:{
            /* bhFlagCk(group, index, 0). The stream order is confirmed in the
               original data: MRY/0005 opcode 12 carries 8 then 48, and no flag
               matrix 48 exists. */
            const int group=s(pc),index=s(pc+1);
            pc+=2;
            bool_set(services_.flag_check&&services_.flag_check(unsigned(group),unsigned(index))?1:0,mode&0xF000);
            break;
        }
        case 13:{
            const float x=float(s(pc))/32.0f,y=float(s(pc+1))/32.0f,z=float(s(pc+2))/32.0f;
            const float xx=float(s(pc+3))/32.0f,yy=float(s(pc+4))/32.0f,zz=float(s(pc+5))/32.0f;
            pc+=6;
            const bool inside=x<=host_.player_x&&host_.player_x<=xx&&y<=host_.player_y&&host_.player_y<=yy&&
                z<=host_.player_z&&host_.player_z<=zz;
            bool_set(inside?1:0,mode&0xF000);
            break;
        }
        case 8:
            if(cde_bol_==0)++pc;
            else pc=std::size_t(std::uint16_t(s(pc)));
            break;
        case 9:
            if(cde_bol_!=0)++pc;
            else pc=std::size_t(std::uint16_t(s(pc)));
            break;
        case 10:
            pc=std::size_t(std::uint16_t(s(pc)));
            break;
        case 14:{
            /* bhFlagSet(group, index, 0) returns the flag's previous state. */
            const int group=s(pc),index=s(pc+1);
            pc+=2;
            const bool previous=services_.flag_check&&services_.flag_check(unsigned(group),unsigned(index));
            if(services_.flag_set)services_.flag_set(unsigned(group),unsigned(index),0);
            bool_set(previous?1:0,mode&0xF000);
            break;
        }
        case 15:
            if(!(status_&0x40)){
                status_|=0x40;
                vew_pos_.x=10.0f*(float(s(pc))/32.0f);
                vew_pos_.y=10.0f*(float(s(pc+1))/32.0f);
                vew_pos_.z=10.0f*(float(s(pc+2))/32.0f);
                vew_zom_=10.0f*(float(s(pc+3))/32.0f);
                pc+=4;
                vew_min_.y=0.3333f*vew_zom_;
                vew_max_.y=1.5f*vew_zom_;
                view_main();
                const float scl=vew_mtx_[14]/host_.screen_dist;
                vew_min_.x=vew_pos_.x-320.0f*scl;
                vew_max_.x=vew_pos_.x+320.0f*scl;
                vew_min_.z=vew_pos_.z-240.0f*scl;
                vew_max_.z=vew_pos_.z+240.0f*scl;
                vew_pos_bak_=vew_pos_;
                vew_zom_bak_=vew_zom_;
            }
            else pc+=4;
            break;
        case 16:
            if(!(status_&0x100)){
                NJS_POINT3 pos{};
                const int room=s(pc);
                pos.x=float(s(pc+1))/32.0f;
                pos.y=float(s(pc+2))/32.0f;
                pos.z=float(s(pc+3))/32.0f;
                pc+=4;
                tag_entry(&cur_mtx_,room,pos);
            }
            else pc+=4;
            break;
        case 17:{
            NJS_POINT3 pos{};
            const int marker=s(pc),palette=s(pc+1);
            pos.x=float(s(pc+2))/32.0f;
            pos.y=float(s(pc+3))/32.0f;
            pos.z=float(s(pc+4))/32.0f;
            pc+=5;
            draw_marker(marker,pos,palette);
            break;
        }
        case 18:{
            const int sound=s(pc),bank_time=s(pc+1),offset_time=s(pc+2);
            pc+=3;
            if(bank_time!=0&&((offset_time+time_)%bank_time)==0){
                if(services_.call_system_se)services_.call_system_se(0,recvx_map_snd_tbl[sound]);
                sounds_.push_back({0,recvx_map_snd_tbl[sound]});
            }
            break;
        }
        case 0:
        case 1:
        case 2:
            break;
        default:break;
        }
    }
}

/* -------------------------------------------------------------- draw helpers */

void MapScreen::draw_marker(int mrk_no,NJS_POINT3 pos,int pal_no){
    /* MapDrawMarker (map.c:1039). */
    if(mrk_no==6){
        pos.z-=1.0f;
        draw_sprite(pos,-1,RECVX_MP_SPR_TITLE);
        return;
    }
    if(mrk_no<0||mrk_no>5)return;
    const auto& attributes=recvx_map_mrk_atr[std::size_t(mrk_no)];
    if(pal_no==-1)pal_no=attributes.pal_no;
    MatrixStorage world{};
    std::memcpy(world.data(),vew_mtx_,sizeof world);
    njMultiMatrix(as_matrix(world),&cur_mtx_);
    if(mrk_no==0){
        if(ply_flr_!=map_flr_)return;
        njTranslate(as_matrix(world),host_.player_x,host_.player_y+attributes.offset,host_.player_z);
        njRotateXYZ(as_matrix(world),host_.player_ax,host_.player_ay,host_.player_az);
    }
    else njTranslate(as_matrix(world),pos.x,pos.y+attributes.offset,pos.z);
    njScale(as_matrix(world),attributes.scale,1.0f,attributes.scale);
    MapDraw draw;
    draw.kind=MapDraw::Kind::Marker;
    draw.model=attributes.mrk_no;
    draw.palette=pal_no;
    if(pal_no>=0&&pal_no<32)draw.material=pal_[std::size_t(pal_no)];
    std::memcpy(draw.matrix,world.data(),sizeof draw.matrix);
    draws_.push_back(draw);
}

void MapScreen::draw_background(float depth,const float* p0,const float* p1){
    /* MapDrawBackground (map.c:1101): the filled plate, then the 32 pixel grid
       in both directions, each line 20 units in front of the plate. */
    draw_fill(p0,p1,depth,col_[0]);
    float a[2]={std::ceil(-16.0f+p0[0]),std::ceil(p0[1])};
    float b[2]={std::floor(p1[0]),a[1]};
    float limit=std::floor(p1[1]);
    while(a[1]<=limit){
        draw_line(a,b,20.0f+depth,col_[1]);
        a[1]+=32.0f;b[1]+=32.0f;
    }
    a[0]=std::ceil(-16.0f+p0[0]);a[1]=std::ceil(p0[1]);
    b[0]=a[0];b[1]=std::floor(p1[1]);
    limit=std::floor(p1[0]);
    while(a[0]<=limit){
        draw_line(a,b,20.0f+depth,col_[1]);
        a[0]+=32.0f;b[0]+=32.0f;
    }
}

void MapScreen::draw_sprite(const NJS_POINT3& pos,int col,int spr_no){
    /* MapDrawSprite (map.c:1142): njDrawSprite2D with attribute 0x22, the
       TexAnm entry of spr_no and the colour as constant material. */
    if(spr_no<0||spr_no>=18)return;
    MapDraw draw;
    draw.kind=MapDraw::Kind::Sprite;
    draw.points[0]=pos.x;draw.points[1]=pos.y;
    draw.priority=pos.z;
    draw.color=std::uint32_t(col);
    draw.attribute=0x22;
    draw.sprite=spr_no;
    draw.stage_texture=spr_no>=17;
    const std::uint32_t value=std::uint32_t(col);
    draw.material.b=float(value&0xFF)*(1.0f/255.0f);
    draw.material.g=float((value>>8)&0xFF)*(1.0f/255.0f);
    draw.material.r=float((value>>16)&0xFF)*(1.0f/255.0f);
    draw.material.a=float((value>>24)&0xFF)*(1.0f/255.0f);
    draws_.push_back(draw);
}

void MapScreen::draw_line(const float* src,const float* dst,float pri,std::uint32_t pal){
    /* MapDrawLine (map.c:2337): pixel centres, attribute 0. */
    MapDraw draw;
    draw.kind=MapDraw::Kind::Line;
    draw.points[0]=0.5f+std::floor(src[0]);
    draw.points[1]=0.5f+std::floor(src[1]);
    draw.points[2]=0.5f+std::floor(dst[0]);
    draw.points[3]=0.5f+std::floor(dst[1]);
    draw.priority=pri;draw.color=pal;draw.attribute=0;
    draws_.push_back(draw);
}

void MapScreen::draw_line2(const float* src,const float* dst,float pri,std::uint32_t pal){
    /* MapDrawLine2 (map.c:2319): unrounded, attribute 0x40. */
    MapDraw draw;
    draw.kind=MapDraw::Kind::Line;
    draw.points[0]=src[0];draw.points[1]=src[1];
    draw.points[2]=dst[0];draw.points[3]=dst[1];
    draw.priority=pri;draw.color=pal;draw.attribute=0x40;
    draws_.push_back(draw);
}

void MapScreen::draw_fill(const float* src,const float* dst,float pri,std::uint32_t pal){
    /* MapDrawFill (map.c:2355). */
    const float points[8]={src[0],src[1],dst[0],src[1],dst[0],dst[1],src[0],dst[1]};
    draw_poly_fill(points,pri,pal);
}

void MapScreen::draw_poly_fill(const float* pnt,float pri,std::uint32_t pal){
    /* MapDrawPolyFill (map.c:2375): njDrawPolygon2D, attribute 0x60. */
    MapDraw draw;
    draw.kind=MapDraw::Kind::Fill;
    for(int i=0;i<8;++i)draw.points[std::size_t(i)]=pnt[i];
    draw.priority=pri;draw.color=pal;draw.attribute=0x60;
    draws_.push_back(draw);
}

void MapScreen::draw_message(int rom,float x,float y){
    /* MapDrawMessage (map.c:2388) has an EMPTY body in the original: neither
       the no-map notice nor the zoomed room name produces any output. The call
       sites are kept so the port does not invent a room-name renderer; the ten
       message blocks of the index file (mes_bufPP) stay unused, exactly as in
       the original. */
    (void)rom;(void)x;(void)y;
}

/* ----------------------------------------------------------------- tag graph */

void MapScreen::tag_init(){
    tags_.clear();
    tags_.reserve(64);
    cur_tag_=-1;
    fcs_tag_=-1;
}

void MapScreen::tag_entry(const NJS_MATRIX* base,int rom_no,const NJS_POINT3& pos){
    /* MapTagEntry (map.c:2202): the position is transformed into the current
       object space; MapTagInit reserves 64 entries. */
    if(tags_.size()>=64)return;
    MapTag tag;
    tag.rom_no=rom_no;
    if(base){
        NJS_POINT3 source=pos;
        njCalcPoint(const_cast<NJS_MATRIX*>(base),&source,&tag.pos);
    }
    else tag.pos=pos;
    tags_.push_back(tag);
}

int MapScreen::tag_connect(int rom_no){
    /* MapTagConnect (map.c:2221): for every tag the nearest tag per quadrant,
       and the tag of `rom_no` as the result. */
    int found=-1;
    for(std::size_t base=0;base<tags_.size();++base){
        float distance[4]={999999.0f,999999.0f,999999.0f,999999.0f};
        tags_[base].neighbour={-1,-1,-1,-1};
        for(std::size_t other=0;other<tags_.size();++other){
            if(base==other)continue;
            const float dx=tags_[other].pos.x-tags_[base].pos.x;
            const float dz=tags_[other].pos.z-tags_[base].pos.z;
            const float d=dx*dx+dz*dz;
            const int direction=((std::int32_t(10430.381f*std::atan2(dz,dx))+8192)/16384)&3;
            if(d<distance[direction]){
                distance[direction]=d;
                tags_[base].neighbour[std::size_t(direction)]=int(other);
            }
        }
        if(tags_[base].rom_no==rom_no)found=int(base);
    }
    return found;
}

int MapScreen::tag_center()const{
    /* MapTagCenter (map.c:2270): the tag closest to the average position. */
    if(tags_.empty())return -1;
    const float scale=1.0f/float(tags_.size());
    float cx=0,cz=0;
    for(const auto& tag:tags_){cx+=tag.pos.x;cz+=tag.pos.z;}
    cx*=scale;cz*=scale;
    float best=999999.0f;
    int found=-1;
    for(std::size_t index=0;index<tags_.size();++index){
        const float dx=tags_[index].pos.x-cx,dz=tags_[index].pos.z-cz;
        const float d=dx*dx+dz*dz;
        if(d<best){best=d;found=int(index);}
    }
    return found;
}

/* ---------------------------------------------------------- connection graph */

void MapScreen::connection_init(int map_num,int flr_num){
    /* MapCncInit (map.c:2795). The original loops map_no over map_num*flr_num
       and flr_no over flr_num, so it writes through the whole array. */
    cnc_map_=map_num;
    cnc_flr_=flr_num;
    cnc_.assign(std::size_t(map_num)*std::size_t(flr_num),MapConnection{});
    for(int map_no=0;map_no<map_num;++map_no)
        for(int flr_no=0;flr_no<flr_num;++flr_no){
            auto* node=connection_get(map_no,flr_no);
            if(!node)continue;
            node->map_no=map_no;
            node->flr_no=flr_no;
            node->status=0;
        }
}

MapConnection* MapScreen::connection_get(int map_no,int flr_no){
    /* MapCncGet (map.c:2822). */
    const long index=long(map_no)*cnc_flr_+flr_no;
    if(index<0||index>=long(cnc_.size()))return nullptr;
    return &cnc_[std::size_t(index)];
}

const MapConnection* MapScreen::connection(int map_no,int flr_no)const{
    const long index=long(map_no)*cnc_flr_+flr_no;
    if(index<0||index>=long(cnc_.size()))return nullptr;
    return &cnc_[std::size_t(index)];
}

void MapScreen::connection_set(int dst,int src,int status){
    /* MapCnc (map.c:2831). mp_no packs map_no * 16 + flr_no; the original
       local names dst_flr/src_flr hold the map number. The two adjustments
       (AdjMapA for map 10, 896 for map 2) are original special cases. */
    int dst_map_no=dst/16,dst_slot=dst&0xF;
    int src_map_no=src/16,src_slot=src&0xF;
    auto* dest=connection_get(dst_map_no,dst_slot);
    auto* source=connection_get(src_map_no,src_slot);
    if(!dest||!source)return;
    int dst_key=dst_map_no*256,src_key=src_map_no*256;
    /* map.c:2852 indexes AdjMapA[3] with the 0..15 slot of map 10. CncDatA
       references map 10 only with slots 0, 1 and 2 (MPa00..MPa02, map.h:39)
       and CncDatB not at all, so the original never reads past the table.
       A slot >= 3 would be an out-of-bounds read in the original; it is
       reported here instead of being masked into a made-up value. */
    const auto adjusted=[](int slot){
        if(slot<0||slot>2)throw std::runtime_error("map connection table references map 10 beyond AdjMapA");
        return recvx_map_adj_map_a[std::size_t(slot)];};
    if(src_key==2560)src_key=adjusted(src_slot);
    if(dst_key==2560)dst_key=adjusted(dst_slot);
    if(src_key==512)src_key=896;
    if(dst_key==512)dst_key=896;
    const int dest_index=int(dest-cnc_.data());
    if(dst_key==src_key){
        if(dst_slot<src_slot)source->flr_prev=dest_index;
        else if(dst_slot>src_slot)source->flr_next=dest_index;
    }
    else if(dst_key<src_key)source->map_prev=dest_index;
    else source->map_next=dest_index;
    dest->status=status;
}

void MapScreen::connection_connect(const std::uint16_t* data,std::size_t count,const MapServices& services){
    /* MapCncConnect (map.c:2896): 0xFFxx switches the status from flag group 8,
       0xFFFF ends the table, every other word is a MapCnc pair. */
    int status=0;
    for(std::size_t index=0;index<count;++index){
        const std::uint16_t value=data[index];
        const int high=value>>8;
        const int low=value&0xFF;
        if(high==255){
            if(low==255)break;
            status=(services.flag_check&&services.flag_check(8,unsigned(low)))?1:0;
        }
        else connection_set(high,low,status);
    }
}

MapNext MapScreen::check_next_map()const{
    /* MapCheckNextMap (map.c:2936): the first node with status != 0 along each
       of the four chains. */
    MapNext next{};
    const auto* current=connection(stg_no_,flr_no_);
    if(!current)return next;
    const auto walk=[&](int start,int MapConnection::*link,int& map_out,int& flr_out){
        const MapConnection* node=start==-1?nullptr:&cnc_[std::size_t(start)];
        while(node&&node->status==0){
            const int following=node->*link;
            node=following==-1?nullptr:&cnc_[std::size_t(following)];
        }
        if(!node)map_out=-1;
        else{map_out=node->map_no;flr_out=node->flr_no;}
    };
    walk(current->flr_next,&MapConnection::flr_next,next.map_up,next.flr_up);
    walk(current->flr_prev,&MapConnection::flr_prev,next.map_down,next.flr_down);
    walk(current->map_prev,&MapConnection::map_prev,next.map_left,next.flr_left);
    walk(current->map_next,&MapConnection::map_next,next.map_right,next.flr_right);
    return next;
}

/* ----------------------------------------------------------------- procedures */

int MapScreen::run_procedure(int index){
    switch(funcs_[index].procedure){
    case ProcBackDraw:return fsub_back_draw();
    case ProcZoomScreen:return fsub_zoom_screen(index);
    case ProcTaskMain:return fsub_task_main(index);
    case ProcMapDraw:return fsub_map_draw(index);
    case ProcCompass:return fsub_compass(index);
    case ProcModeMessage:return fsub_mode_message(index);
    case ProcZoomCursor:return fsub_zoom_cursor(index);
    case ProcZoomInformation:return fsub_zoom_information(index);
    case ProcGaugeDraw:return fsub_gauge_draw(index);
    case ProcSpriteDraw:return fspr_sprite_draw(index);
    case ProcSilhouetteDraw:return fspr_silhouette_draw(index);
    case ProcArrowDraw:return fspr_arrow_draw(index);
    case ProcArrowDraw2:return fspr_arrow_draw2(index);
    default:return 1;
    }
}

int MapScreen::run_task(int task,int index){
    switch(task){
    case TaskWait:return ftsk_map_wait();
    case TaskExit:return ftsk_map_exit();
    case TaskRead:return ftsk_map_read(index);
    case TaskNormal:return ftsk_map_normal(index);
    case TaskZoom:return ftsk_map_zoom(index);
    default:return 0;
    }
}

int MapScreen::fsub_task_main(int index){
    /* FsubTaskMain (map.c:1744): when the task reports 0 the stored mode is
       installed and the slot is released. */
    auto& func=funcs_[index];
    if(run_task(func.w[2].i,index)==0){
        map_mode_=func.w[1].i;
        func_free(index);
    }
    return 1;
}

int MapScreen::ftsk_map_wait(){
    /* FtskMapWait (map.c:1757). */
    return (host_.st_flg&0x8)?1:0;
}

int MapScreen::ftsk_map_exit(){
    /* FtskMapExit (map.c:1771). */
    return (services_.screen_fade_pending?services_.screen_fade_pending():(host_.cb_flg&0x2)!=0)?1:0;
}

int MapScreen::ftsk_map_read(int index){
    /* FtskMapRead (map.c:1786). */
    auto& func=funcs_[index];
    switch(func.w[6].i){
    case 0:
        fil_mode_=0;
        func.w[6].i=1;
        [[fallthrough]];
    case 1:
        return read_map_data(unsigned(func.w[3].i))!=0?0:1;
    default:
        return 1;
    }
}

int MapScreen::ftsk_map_normal(int index){
    /* FtskMapNormal (map.c:1811). */
    auto& func=funcs_[index];
    switch(func.w[6].i){
    case 0:{
        if(services_.call_system_se)services_.call_system_se(0,5);
        sounds_.push_back({0,5});
        dst_pos_=vew_pos_bak_;
        dst_zom_=vew_zom_bak_;
        map_nxt_=check_next_map();
        if(map_nxt_.map_up!=-1)entry_sprite(RECVX_MP_SET_ARROW_UP,0);
        if(map_nxt_.map_down!=-1)entry_sprite(RECVX_MP_SET_ARROW_DOWN,1);
        if(map_nxt_.map_left!=-1)entry_sprite(RECVX_MP_SET_ARROW_LEFT,2);
        if(map_nxt_.map_right!=-1)entry_sprite(RECVX_MP_SET_ARROW_RIGHT,3);
        funcs_[index].w[6].i=1;
        return 1;
    }
    case 1:
        if(map_mode_==RECVX_MP_MOD_MAP_READ){func_free(index);return 1;}
        if((pad_ps_&0x100)&&cur_tag_!=-1)return 0;
        return 1;
    default:return 1;
    }
}

int MapScreen::ftsk_map_zoom(int index){
    /* FtskMapZoom (map.c:1867). */
    switch(funcs_[index].w[6].i){
    case 0:{
        if(services_.call_system_se)services_.call_system_se(0,5);
        sounds_.push_back({0,5});
        fcs_tag_=cur_tag_;
        status_|=0x200;
        if(fcs_tag_>=0&&fcs_tag_<int(tags_.size())){
            const auto& tag=tags_[std::size_t(fcs_tag_)];
            bnk_tag_rom_=tag.rom_no;
            dst_pos_.x=tag.pos.x;
            dst_pos_.y=tag.pos.y;
            dst_pos_.z=tag.pos.z;
        }
        bnk_tag_pal_=3;
        bnk_tag_pal_wal_=9;
        dst_zom_=0.5f*vew_zom_bak_;
        func_alloc(ProcZoomCursor,0);
        func_alloc(ProcZoomInformation,0);
        entry_sprite(RECVX_MP_SET_LR_ZOOM,0);
        entry_sprite(RECVX_MP_SET_ARROW2_UP,0);
        entry_sprite(RECVX_MP_SET_ARROW2_DOWN,1);
        entry_sprite(RECVX_MP_SET_ARROW2_LEFT,2);
        entry_sprite(RECVX_MP_SET_ARROW2_RIGHT,3);
        const int gauge=func_alloc(ProcGaugeDraw,0);
        if(gauge>=0){
            funcs_[gauge].w[1].f=57.0f;
            funcs_[gauge].w[2].f=416.0f;
            funcs_[gauge].w[3].f=-6.0f;
        }
        funcs_[index].w[6].i=1;
        return 1;
    }
    case 1:{
        const unsigned pressed=pad_ps_;
        if(map_mode_==RECVX_MP_MOD_MAP_READ){func_free(index);return 1;}
        if(pressed&0x100){status_&=~0x200;return 0;}
        int direction=-1;
        if(pressed&0x8)direction=0;
        else if(pressed&0x2)direction=1;
        else if(pressed&0x4)direction=2;
        else if(pressed&0x1)direction=3;
        if(direction!=-1&&fcs_tag_>=0&&fcs_tag_<int(tags_.size())){
            const int target=tags_[std::size_t(fcs_tag_)].neighbour[std::size_t(direction)];
            if(target!=-1){
                fcs_tag_=target;
                const auto& tag=tags_[std::size_t(target)];
                bnk_tag_rom_=tag.rom_no;
                dst_pos_.x=tag.pos.x;
                dst_pos_.y=tag.pos.y;
                dst_pos_.z=tag.pos.z;
                if(services_.call_system_se)services_.call_system_se(0,2);
                sounds_.push_back({0,2});
            }
        }
        dst_zom_+=0.05f*pad_al_;
        dst_zom_-=0.05f*pad_ar_;
        if(dst_zom_<vew_min_.y)dst_zom_=vew_min_.y;
        if(dst_zom_>vew_max_.y)dst_zom_=vew_max_.y;
        return 1;
    }
    default:return 1;
    }
}

int MapScreen::fsub_map_draw(int index){
    /* FsubMapDraw (map.c:1378). */
    if(map_mode_==RECVX_MP_MOD_MAP_READ){
        objects_.clear();
        map_code_=0;
        func_free(index);
        return 1;
    }
    if(!objects_.empty()&&map_code_){
        code_process();
        if(!(status_&0x100)){
            status_|=0x100;
            cur_tag_=tag_connect(rom_no_);
            if(cur_tag_==-1)cur_tag_=tag_center();
        }
    }
    return 1;
}

int MapScreen::fsub_back_draw(){
    /* FsubBackDraw (map.c:1412). */
    draw_background(bck_depth_,bck_p0_,bck_p1_);
    return 1;
}

int MapScreen::fspr_sprite_draw(int index){
    /* FsprSpriteDraw (map.c:1456): act_bit selects the modes the sprite is
       shown in; outside them the slot is released. */
    auto& func=funcs_[index];
    if(!(func.w[1].i&(1<<map_mode_))){func_free(index);return 1;}
    if(func.mode==0){
        NJS_POINT3 position{func.w[4].f,func.w[5].f,func.w[6].f};
        draw_sprite(position,func.w[3].i,func.w[2].i);
    }
    return 1;
}

int MapScreen::fspr_silhouette_draw(int index){
    /* FsprSilhouetteDraw (map.c:1475): the alpha ramps in with TWO increments
       per tick (the original increments count and then pre-increments it in
       the comparison) and stops at 0xFF. */
    auto& func=funcs_[index];
    int result=1;
    switch(func.w[7].i){
    case 0:{
        const std::int32_t backup=func.w[3].i;
        func.w[8].i++;
        if(++func.w[8].i>=0xFF){func.w[8].i=0xFF;func.w[7].i=1;}
        func.w[3].u=(func.w[3].u&0xFFFFFFu)|(std::uint32_t(func.w[8].i)<<24);
        result=fspr_sprite_draw(index);
        if(func_live(index))funcs_[index].w[3].i=backup;
        break;
    }
    case 1:
        result=fspr_sprite_draw(index);
        break;
    default:break;
    }
    return result;
}

int MapScreen::fspr_arrow_draw(int index){
    /* FsprArrowDraw (map.c:1514): the arrow bobs by 1.9 * sin(count), count
       advances by 910 per tick, and in MP_MOD_WAIT_NORMAL the matching pad
       direction switches the map. */
    auto& func=funcs_[index];
    const std::int32_t count=func.w[8].i;
    const float x=func.w[4].f,y=func.w[5].f,z=func.w[6].f;
    switch(func.w[7].i){
    case 0:func.w[5].f=y-1.9f*njSin(count);break;
    case 1:func.w[5].f=y+1.9f*njSin(count);break;
    case 2:func.w[4].f=x-1.9f*njSin(count);break;
    case 3:func.w[4].f=x+1.9f*njSin(count);break;
    default:break;
    }
    func.w[4].f=std::floor(func.w[4].f);
    func.w[5].f=std::floor(func.w[5].f);
    fspr_sprite_draw(index);
    if(!func_live(index))return 1;
    auto& live=funcs_[index];
    live.w[4].f=x;live.w[5].f=y;live.w[6].f=z;
    live.w[8].i=count+910;
    if(live.w[8].i>=32768)live.w[8].i-=32768;
    if(map_mode_==RECVX_MP_MOD_WAIT_NORMAL){
        const unsigned pressed=pad_ps_;
        int map=-1,floor_no=0;
        bool selected=false;
        switch(live.w[7].i){
        case 0:if(pressed&0x1){map=map_nxt_.map_up;floor_no=map_nxt_.flr_up;selected=true;}break;
        case 1:if(pressed&0x2){map=map_nxt_.map_down;floor_no=map_nxt_.flr_down;selected=true;}break;
        case 2:if(pressed&0x4){map=map_nxt_.map_left;floor_no=map_nxt_.flr_left;selected=true;}break;
        case 3:if(pressed&0x8){map=map_nxt_.map_right;floor_no=map_nxt_.flr_right;selected=true;}break;
        default:break;
        }
        if(selected){
            if(services_.call_system_se)services_.call_system_se(0,2);
            sounds_.push_back({0,2});
            const int top=map!=10?0:floor_no;
            const int mode=live.w[7].i;
            if(mode==2||mode==3){
                const auto* source=connection(map,top);
                int slot=top;
                while(source){
                    connection_set(stg_no_*16|flr_no_,map*16|slot,
                        connection(stg_no_,flr_no_)?connection(stg_no_,flr_no_)->status:0);
                    const int following=source->flr_next;
                    source=following==-1?nullptr:&cnc_[std::size_t(following)];
                    ++slot;
                }
            }
            stg_no_=map;
            flr_no_=floor_no;
            map_mode_=RECVX_MP_MOD_MAP_READ;
        }
    }
    return 1;
}

int MapScreen::fspr_arrow_draw2(int index){
    /* FsprArrowDraw2 (map.c:1649): the zoom arrows point at the focused tag's
       neighbour; without a neighbour the sprite is drawn in mode -1, i.e. not
       at all. */
    auto& func=funcs_[index];
    const std::int32_t count=func.w[8].i;
    const float x=func.w[4].f,y=func.w[5].f,z=func.w[6].f;
    int tab=0;
    switch(func.w[7].i){
    case 0:func.w[5].f=y-1.9f*njSin(count);tab=3;break;
    case 1:func.w[5].f=y+1.9f*njSin(count);tab=1;break;
    case 2:func.w[4].f=x-1.9f*njSin(count);tab=2;break;
    case 3:func.w[4].f=x+1.9f*njSin(count);tab=0;break;
    default:break;
    }
    func.w[4].f=std::floor(func.w[4].f);
    func.w[5].f=std::floor(func.w[5].f);
    const bool has_neighbour=fcs_tag_>=0&&fcs_tag_<int(tags_.size())&&
        tags_[std::size_t(fcs_tag_)].neighbour[std::size_t(tab)]!=-1;
    func.mode=has_neighbour?0:-1;
    fspr_sprite_draw(index);
    if(!func_live(index))return 1;
    auto& live=funcs_[index];
    live.w[4].f=x;live.w[5].f=y;live.w[6].f=z;
    live.w[8].i=count+910;
    if(live.w[8].i>=32768)live.w[8].i-=32768;
    return 1;
}

int MapScreen::fsub_compass(int index){
    /* FsubCompass (map.c:2712): the needle turns 1456 per tick. */
    auto& func=funcs_[index];
    if(func.mode==0){
        func.w[2].f=-4.0f;
        func.w[5].f=std::fabs(-4.0f)/host_.screen_dist;
        func.w[4].f=func.w[5].f/1.174f;
        func.w[0].f=(585.0f-320.0f)*func.w[4].f;
        func.w[1].f=(240.0f-62.0f)*func.w[5].f;
        func.w[3].i=0;
        func.mode++;
    }
    MatrixStorage world{};
    njUnitMatrix(as_matrix(world));
    njRotateX(as_matrix(world),32768);
    njTranslate(as_matrix(world),func.w[0].f,func.w[1].f,func.w[2].f);
    njScale(as_matrix(world),func.w[5].f,func.w[5].f,func.w[5].f);
    func.w[3].i+=1456;
    njRotateY(as_matrix(world),func.w[3].i);
    MapDraw draw;
    draw.kind=MapDraw::Kind::Compass;
    std::memcpy(draw.matrix,world.data(),sizeof draw.matrix);
    draw.color=recvx_map_cmp_arw[0].col;
    draws_.push_back(draw);
    return 1;
}

int MapScreen::fsub_mode_message(int index){
    /* FsubModeMessage (map.c:2759): the button sprite follows sys->keytype. */
    auto& func=funcs_[index];
    int type=host_.keytype;
    if(type>2)type=0;
    if(func.mode==0){
        const NJS_POINT3 first{recvx_map_nml_mes[0].x,recvx_map_nml_mes[0].y,recvx_map_nml_mes[0].z};
        const NJS_POINT3 second{recvx_map_nml_mes[1].x,recvx_map_nml_mes[1].y,recvx_map_nml_mes[1].z};
        draw_sprite(first,-1,recvx_map_nml_mes[0].spr+type);
        draw_sprite(second,-1,recvx_map_nml_mes[1].spr);
    }
    return 1;
}

int MapScreen::fsub_zoom_cursor(int index){
    /* FsubZoomCursor (map.c:2394): the four frame lines slide to the target
       table, 0.5 of the remaining distance rounded up per tick. */
    auto& func=funcs_[index];
    const RECVX_MAP_POINT2* target=recvx_map_zoom_dst_pos1;
    switch(func.mode){
    case 0:
        func.w[0].i=0;
        func.w[2].f=recvx_map_zoom_dst_pos0[0].x;func.w[3].f=recvx_map_zoom_dst_pos0[0].y;
        func.w[4].f=recvx_map_zoom_dst_pos0[1].x;func.w[5].f=recvx_map_zoom_dst_pos0[1].y;
        func.w[6].f=recvx_map_zoom_dst_pos0[2].x;func.w[7].f=recvx_map_zoom_dst_pos0[2].y;
        func.w[8].f=recvx_map_zoom_dst_pos0[3].x;func.w[9].f=recvx_map_zoom_dst_pos0[3].y;
        func.w[1].i=1;
        func.mode++;
        [[fallthrough]];
    case 1:
        if(func.w[0].i<=0){
            if(fcs_tag_>=0&&fcs_tag_<int(tags_.size()))draw_message(tags_[std::size_t(fcs_tag_)].rom_no,48.0f,425.0f);
        }
        else func.w[0].i--;
        if(map_mode_!=RECVX_MP_MOD_WAIT_ZOOM){
            func.w[1].i=2;
            func.w[0].i=5;
            func.mode=2;
        }
        break;
    case 2:
        if(func.w[0].i<0){func_free(index);return 1;}
        func.w[0].i--;
        break;
    default:break;
    }
    target=func.w[1].i==2?recvx_map_zoom_dst_pos2:recvx_map_zoom_dst_pos1;
    const std::uint32_t color=map_argb_to_color(pal_[19]);
    const float line_a[4]={func.w[2].f,func.w[3].f,func.w[4].f,func.w[5].f};
    const float line_b[4]={func.w[6].f,func.w[7].f,func.w[8].f,func.w[9].f};
    draw_line(line_a,line_a+2,-7.0f,color);
    draw_line(line_b,line_b+2,-7.0f,color);
    const auto advance=[](float& value,float destination){value+=std::ceil(0.5f*(destination-value));};
    advance(func.w[2].f,target[0].x);advance(func.w[3].f,target[0].y);
    advance(func.w[4].f,target[1].x);advance(func.w[5].f,target[1].y);
    advance(func.w[6].f,target[2].x);advance(func.w[7].f,target[2].y);
    advance(func.w[8].f,target[3].x);advance(func.w[9].f,target[3].y);
    return 1;
}

int MapScreen::fsub_zoom_information(int index){
    /* FsubZoomInfomation (map.c:2488): the legend plate with the three
       diamonds, the save point, the item and the box sprite. */
    auto& func=funcs_[index];
    if(func.mode==0){
        func.w[0].f=recvx_map_info_set_pos[0];
        func.w[1].f=recvx_map_info_set_pos[1];
        func.w[2].f=recvx_map_info_set_pos[2];
        func.w[3].f=585.0f;
        func.w[4].f=417.0f;
        func.w[5].i=0;
        func.mode++;
    }
    if(func.mode==1){
        if(func.w[5].i<=0){
            const float plate[2]={func.w[0].f,func.w[1].f};
            const float corner[2]={func.w[3].f,func.w[4].f};
            draw_fill(plate,corner,func.w[2].f,0x80000000u);
            NJS_POINT3 position{16.0f+func.w[0].f,16.0f+func.w[1].f,1.0f+func.w[2].f};
            draw_sprite(position,int(map_argb_to_color(pal_[28])),RECVX_MP_SPR_DIAMOND);
            position.x+=16.0f;
            draw_sprite(position,-1,RECVX_MP_SPR_ITEM);
            position.y+=16.0f;position.x-=16.0f;
            draw_sprite(position,int(map_argb_to_color(pal_[13])),RECVX_MP_SPR_DIAMOND);
            position.x+=16.0f;
            draw_sprite(position,-1,RECVX_MP_SPR_SAVEPOINT);
            position.y+=16.0f;position.x-=16.0f;
            draw_sprite(position,int(map_argb_to_color(pal_[14])),RECVX_MP_SPR_DIAMOND);
            position.x+=16.0f;
            draw_sprite(position,-1,RECVX_MP_SPR_ITEM);
            position.x+=48.0f;
            draw_sprite(position,-1,RECVX_MP_SPR_BOX);
        }
        else func.w[5].i--;
        if(map_mode_!=RECVX_MP_MOD_WAIT_ZOOM){func_free(index);return 1;}
    }
    return 1;
}

int MapScreen::fsub_zoom_screen(int index){
    /* FsubZoomScreen (map.c:2576): the masking plates and the rounded corner
       around the zoom viewport. */
    auto& func=funcs_[index];
    if(func.mode==0){
        func.w[0].f=recvx_map_scr_in[0].x;func.w[1].f=recvx_map_scr_in[0].y;
        func.w[2].f=recvx_map_scr_in[1].x;func.w[3].f=recvx_map_scr_in[1].y;
        func.w[4].f=32.0f;
        func.w[5].i=8192;
        func.mode++;
    }
    if(func.mode!=1)return 1;
    const float x0=func.w[0].f,y0=func.w[1].f,x1=func.w[2].f,y1=func.w[3].f;
    const float radius=func.w[4].f;
    const std::uint32_t plate=0x80000000u;
    const std::uint32_t edge=std::uint32_t(-0xFFA000);
    float pnt[8]={};
    pnt[0]=recvx_map_scr_out[0].x;pnt[1]=recvx_map_scr_out[0].y;
    pnt[2]=x1-radius;pnt[3]=recvx_map_scr_out[0].y;
    pnt[4]=x1-radius;pnt[5]=y0;
    pnt[6]=recvx_map_scr_out[0].x;pnt[7]=y0;
    draw_poly_fill(pnt,-8.0f,plate);
    const float corner_from[2]={x0,y0};
    const float corner_to[2]={pnt[4],pnt[5]};
    draw_line2(corner_from,corner_to,-7.0f,edge);
    std::int32_t angle=16384,next=angle;
    do{
        pnt[0]=pnt[6]=x1+radius*njCos(angle);
        pnt[1]=pnt[7]=y0+radius*njSin(angle);
        next=angle+func.w[5].i;
        pnt[2]=pnt[4]=x1+radius*njCos(next);
        pnt[3]=pnt[5]=y0+radius*njSin(next);
        draw_line2(pnt,pnt+2,-7.0f,edge);
        if(angle>=32768){
            if(std::uint16_t(angle)<32768)pnt[5]=pnt[7]=recvx_map_scr_out[1].y;
            else pnt[1]=pnt[3]=recvx_map_scr_out[0].y;
            draw_poly_fill(pnt,-8.0f,plate);
        }
        angle=next;
    }while(next<81920);
    const float last[2]={pnt[2],pnt[3]};
    const float inner[2]={x1,y1};
    draw_line2(last,inner,-7.0f,edge);
    pnt[0]=pnt[6]=x1+radius;
    pnt[1]=pnt[3]=recvx_map_scr_out[0].y;
    pnt[2]=pnt[4]=recvx_map_scr_out[1].x;
    pnt[5]=pnt[7]=recvx_map_scr_out[1].y;
    draw_poly_fill(pnt,-8.0f,plate);
    pnt[0]=pnt[6]=x0;
    pnt[1]=pnt[3]=y1;
    pnt[2]=pnt[4]=x1;
    pnt[5]=pnt[7]=recvx_map_scr_out[1].y;
    draw_poly_fill(pnt,-8.0f,plate);
    {
        const float from[2]={pnt[2],pnt[3]};
        const float to[2]={pnt[0],pnt[1]};
        draw_line2(from,to,-7.0f,edge);
    }
    pnt[0]=pnt[6]=recvx_map_scr_out[0].x;
    pnt[1]=pnt[3]=y0;
    pnt[2]=pnt[4]=x0;
    pnt[5]=pnt[7]=recvx_map_scr_out[1].y;
    draw_poly_fill(pnt,-8.0f,plate);
    pnt[5]=y1;
    {
        const float from[2]={pnt[2],pnt[3]};
        const float to[2]={pnt[4],pnt[5]};
        draw_line2(from,to,-7.0f,edge);
    }
    {
        float a[2]={recvx_map_scr_out[0].x,240.0f},b[2]={x0,240.0f};
        draw_line(a,b,-7.0f,edge);
        a[0]=x1;b[0]=recvx_map_scr_out[1].x;
        draw_line(a,b,-7.0f,edge);
        a[0]=b[0]=320.0f;
        a[1]=recvx_map_scr_out[0].y;b[1]=y0;
        draw_line(a,b,-7.0f,edge);
        a[1]=y1;b[1]=recvx_map_scr_out[1].y;
        draw_line(a,b,-7.0f,edge);
    }
    return 1;
}

int MapScreen::fsub_gauge_draw_z(int index){
    /* FsubGaugeDrawZ (map.c:1992): the vertical ruler, ticks every 5 units,
       long tick every 10 and medium every 2 counts. */
    auto& func=funcs_[index];
    const float gx=func.w[1].f,gy=func.w[2].f,gz=func.w[3].f;
    const float dist=(host_.screen_dist*(5.0f/vew_mtx_[14]))/5.0f;
    const std::uint32_t color=col_[2];
    const float yy=gy-100.0f*dist;
    float a[2]={std::floor(gx-1.0f),0.5f+std::floor(yy)};
    float b[2]={0.5f+std::floor(gx-8.0f),a[1]};
    const auto emit=[&](const float* p0,const float* p1){
        MapDraw draw;
        draw.kind=MapDraw::Kind::Line;
        draw.points[0]=p0[0];draw.points[1]=p0[1];
        draw.points[2]=p1[0];draw.points[3]=p1[1];
        draw.priority=gz;draw.color=color;draw.attribute=0;
        draws_.push_back(draw);
    };
    emit(a,b);
    b[1]=0.5f+std::floor(yy+100.0f*dist);
    b[0]=a[0];
    emit(a,b);
    a[1]=b[1];
    a[0]=0.5f+std::floor(gx-8.0f);
    emit(a,b);
    const float tmp=1000.0f+((vew_pos_.z-50.0f)/5.0f);
    int count=0;float position=0;
    if(njFraction(tmp)==0.0f){count=int(tmp);position=0;}
    else{count=int(std::ceil(tmp));position=5.0f*(1.0f-njFraction(tmp));}
    for(;position<=100.0f;position+=5.0f,++count){
        const float size=(count%10)==0?9.0f:((count%2)==0?5.0f:3.0f);
        float p0[2]={gx,0.5f+std::floor(yy+position*dist)};
        float p1[2]={gx+size,p0[1]};
        emit(p0,p1);
    }
    return 1;
}

int MapScreen::fsub_gauge_draw_x(int index){
    /* FsubGaugeDrawX (map.c:2075): the horizontal ruler, aspect corrected. */
    auto& func=funcs_[index];
    const float gx=func.w[1].f,gy=func.w[2].f,gz=func.w[3].f;
    const float dist=1.174f*((host_.screen_dist*(5.0f/vew_mtx_[14]))/5.0f);
    const std::uint32_t color=col_[2];
    const auto emit=[&](const float* p0,const float* p1){
        MapDraw draw;
        draw.kind=MapDraw::Kind::Line;
        draw.points[0]=p0[0];draw.points[1]=p0[1];
        draw.points[2]=p1[0];draw.points[3]=p1[1];
        draw.priority=gz;draw.color=color;draw.attribute=0;
        draws_.push_back(draw);
    };
    float a[2]={std::floor(gx)-0.5f,std::floor(1.0f+gy)};
    float b[2]={a[0],std::floor(8.0f+gy)-0.5f};
    emit(a,b);
    b[0]=0.5f+std::floor(gx+100.0f*dist);
    b[1]=a[1];
    emit(a,b);
    a[0]=b[0];
    b[1]=std::floor(8.0f+gy)-0.5f;
    emit(a,b);
    const float tmp=1000.0f+((vew_pos_.x-50.0f)/5.0f);
    int count=0;float position=0;
    if(njFraction(tmp)==0.0f){count=int(tmp);position=0;}
    else{count=int(std::ceil(tmp));position=5.0f*(1.0f-njFraction(tmp));}
    for(;position<=100.0f;position+=5.0f,++count){
        const float size=(count%10)==0?9.0f:((count%2)==0?5.0f:3.0f);
        float p0[2]={0.5f+std::floor(gx+position*dist),gy-size};
        float p1[2]={p0[0],gy};
        emit(p0,p1);
    }
    return 1;
}

int MapScreen::fsub_gauge_draw(int index){
    /* FsubGaugeDraw (map.c:2157): both rulers plus the three scale sprites;
       the slot is released as soon as the zoom mode ends. */
    fsub_gauge_draw_z(index);
    fsub_gauge_draw_x(index);
    auto& func=funcs_[index];
    const float gx=func.w[1].f,gy=func.w[2].f,gz=func.w[3].f;
    const float scale=(host_.screen_dist*(5.0f/vew_mtx_[14]))/5.0f;
    NJS_POINT3 position{2.0f+gx,8.0f+(gy-100.0f*scale),gz};
    draw_sprite(position,-1,RECVX_MP_SPR_10M_L);
    position={gx-2.0f,1.0f+gy,gz};
    draw_sprite(position,-1,RECVX_MP_SPR_0M);
    position={(gx+100.0f*(1.174f*scale))-8.0f,gy-2.0f,gz};
    draw_sprite(position,-1,RECVX_MP_SPR_10M_D);
    if(map_mode_!=RECVX_MP_MOD_WAIT_ZOOM)func_free(index);
    return 1;
}

} // namespace recvx

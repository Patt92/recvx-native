#pragma once
#include "map_tables.h"
#include "ninja_matrix.h"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace recvx {

/* Native port of the map screen (src/ps2/veronica/prog/map.c, entered from
   system.c:785/789/853).  The mechanism is reimplemented here on the existing
   native owners: flags come from EventVm (groups 8/9), the item list from
   Inventory, stage/room/player values and the pad from RoomWorld/Runtime,
   sound from AudioMixer.  No second sys/plp/rom work is created and no
   original function body is compiled or linked.

   Drawing: map.c emits its picture through njDrawLine2D / njDrawPolygon2D /
   njDrawSprite2D / njCnkEasyMultiDrawObject / njDrawPolygon3DEx.  native/scene.h
   has SceneTriangle and SceneLine but no 2D sprite, no screen-space quad fill
   and no constant-material model call, so the port records every primitive with
   its original geometry, priority, blend attribute and resolved material in
   `MapDrawList` instead of silently dropping it.  Binding that list to the
   renderer is the integrator contract (archived/work/native_port_D10_map.patch). */

/* One primitive as the original draw call produced it. */
struct MapDraw {
    enum class Kind {Fill,Line,Sprite,Marker,MapObject,Compass};
    Kind kind=Kind::Fill;
    /* Fill: four screen points; Line: two screen points (x,y pairs). */
    std::array<float,8> points{};
    float priority=0;             /* njDraw*2D `pri` */
    std::uint32_t color=0;        /* NJS_COLOR of the line/fill, or the sprite colour */
    unsigned attribute=0;         /* njDraw* attribute: 0, 0x40, 0x60 (fill), 0x22 (sprite) */
    int sprite=-1;                /* mp_spr index into TexAnm, for Kind::Sprite */
    bool stage_texture=false;     /* MapDrawSprite: spr_no >= 17 draws from stg_texP */
    int model=-1;                 /* Marker: MrkMdl slot; MapObject: map model object index */
    int palette=-1;               /* MapPal index that produced `material` (-1: none) */
    RECVX_MAP_ARGB material{};    /* njSetConstantMaterial argument */
    NJS_MATRIX matrix{};          /* world matrix for Marker/MapObject/Compass */
};
using MapDrawList = std::vector<MapDraw>;

/* sys/plp values map.c reads, handed over by their existing native owners. */
struct MapHost {
    int ply_id=0;                 /* sys->ply_id */
    int stg_no=0,rom_no=0;        /* sys->stg_no, sys->rom_no */
    float player_x=0,player_y=0,player_z=0;     /* plp->px/py/pz */
    std::int32_t player_ax=0,player_ay=0,player_az=0; /* plp->ax/ay/az */
    unsigned pad_ps=0;            /* sys->pad_ps */
    float pad_al=0,pad_ar=0,pad_ax=0,pad_ay=0;  /* sys->pad_al/ar/ax/ay */
    int keytype=0;                /* sys->keytype (FsubModeMessage) */
    unsigned st_flg=0,cb_flg=0;   /* sys->st_flg (FtskMapWait), sys->cb_flg (FtskMapExit) */
    std::uint32_t fade_pbk=0;     /* sys->fade_pbk */
    std::array<int,4> mp_prm{};   /* sys->mp_prm[0..3], MP_SET_GAME entry */
    int dor_partid=5;             /* sys->dor_partid, sdfunc.c:441 -> MRY.AFS partition */
    float screen_dist=500.0f;     /* _nj_screen_.dist, njSetScreen in MP_MOD_IDX_READ */
};

/* The services map.c calls out to, bound to the existing native owners. */
struct MapServices {
    std::function<bool(unsigned group,unsigned index)> flag_check;            /* bhFlagCk */
    std::function<void(unsigned group,unsigned index,unsigned mode)> flag_set;/* bhFlagSet */
    std::function<const std::uint32_t*()> inventory_slots;   /* &sys->itm[ply_id*16] */
    /* RequestReadInsideFile/GetInsideFileSize on one AFS partition. An empty
       result is the original "file could not be opened" case. */
    std::function<std::vector<std::uint8_t>(unsigned partition,unsigned file)> read_inside_file;
    std::function<void(int,int)> call_system_se;              /* CallSystemSe */
    std::function<void(std::uint32_t,float)> set_screen_fade; /* bhSetScreenFade */
    /* FtskMapExit reads sys->cb_flg live, including a fade started earlier in
       the SAME bhControlMap call; the entry snapshot cannot provide that. */
    std::function<bool()> screen_fade_pending;
};

/* One MPAC chunk of a loaded map file, as a span of the loaded bytes. The
   integrator decodes models and textures with the existing native decoders
   (bhMlbBinRealize / bhSetMemPvpTexture equivalents in world.cpp/scene.h). */
struct MapChunk {std::size_t offset=0,size=0;};

/* One NJS_CNK_OBJECT of the realized stage map model (map.c:464 DAT_SET).
   The integrator decodes the MDL chunk with the existing native decoder and
   hands the object array over; MapScreen::set_map_objects then runs the
   original MapPurgeTree (map.c:1245) that flattens the tree to absolute
   transforms, exactly as the original does before MapCodeProcess reads it. */
struct MapObject {
    std::array<float,3> pos{};
    std::array<std::int32_t,3> ang{};
    int child=-1,sibling=-1;
    bool has_model=false;        /* objP->model != NULL */
    std::uint32_t node=0;        /* index into the file's NJS_CNK_OBJECT array (model geometry lookup) */
};

/* map.c tag_wrk_typ: one room marker of the current floor plus its four
   nearest neighbours by direction (MapTagConnect). */
struct MapTag {
    int rom_no=0;
    NJS_POINT3 pos{};
    std::array<int,4> neighbour{-1,-1,-1,-1};
};

/* map.c cnc_wrk_typ: one map/floor node of the connection graph. */
struct MapConnection {
    int status=0,map_no=0,flr_no=0;
    int flr_next=-1,flr_prev=-1,map_next=-1,map_prev=-1;
};

/* map.c map_nxt: the reachable neighbour map/floor per direction. */
struct MapNext {
    int map_up=-1,flr_up=0,map_down=-1,flr_down=0;
    int map_left=-1,flr_left=0,map_right=-1,flr_right=0;
};

class MapScreen {
public:
    /* bhInitMap(set_mod), set_mod = RECVX_MP_SET_EVENT/GAME/SUB. */
    void init(int set_mod,const MapHost& host,const MapServices& services);
    void set_map(){}                 /* bhSetMap (map.c:173) is empty in the original */
    /* bhControlMap: one tick. Returns false once MP_MOD_EXIT is reached, i.e.
       when the original returns 0 and system.c leaves the task. */
    bool control(const MapHost& host);
    /* bhExitMap: releases the map/stage textures and clears gm_flg 0x80000.
       `release_texture` is called for each texture list the original releases
       (0 = map_texP, 1 = stg_texP); `restore_room_texture` is true when the
       original copies the saved room textures back (rom_texP != NULL). */
    void exit();
    bool released_map_texture()const{return released_map_texture_;}
    bool released_stage_texture()const{return released_stage_texture_;}
    bool restores_room_texture()const{return restores_room_texture_;}
    bool game_map_flag_cleared()const{return game_map_flag_cleared_;}

    /* State the acceptance test and the runtime observe. */
    int mode()const{return map_mode_;}
    int status()const{return status_;}
    int time()const{return time_;}
    int map_no()const{return map_no_;}
    int stage()const{return stg_no_;}
    int room()const{return rom_no_;}
    int floor()const{return flr_no_;}
    int map_floor()const{return map_flr_;}
    int player_room()const{return ply_rom_;}
    int player_floor()const{return ply_flr_;}
    int bnk_mode()const{return bnk_mde_;}
    int bnk_stage()const{return bnk_stg_;}
    int bnk_floor()const{return bnk_flr_;}
    int bnk_room()const{return bnk_rom_;}
    bool has_map_data()const{return map_code_!=0;}
    bool no_map()const{return map_mode_==RECVX_MP_MOD_WAIT_NOMAP;}
    float zoom()const{return vew_zom_;}
    const NJS_POINT3& view_position()const{return vew_pos_;}
    const MapNext& next_maps()const{return map_nxt_;}
    const std::vector<MapTag>& tags()const{return tags_;}
    int current_tag()const{return cur_tag_;}
    int focus_tag()const{return fcs_tag_;}
    const std::vector<MapConnection>& connections()const{return cnc_;}
    const MapConnection* connection(int map_no,int flr_no)const;
    const std::array<RECVX_MAP_ARGB,32>& palette()const{return pal_;}
    const std::array<std::uint32_t,3>& colors()const{return col_;}
    const MapDrawList& draw_list()const{return draws_;}
    /* Marker models and textures of the index file, and of the stage file. */
    const std::vector<MapChunk>& marker_models()const{return marker_models_;}
    const MapChunk& map_texture()const{return map_texture_;}
    const MapChunk& stage_texture()const{return stage_texture_;}
    const MapChunk& stage_model()const{return stage_model_;}
    /* The floor file (map.c:420 map_bufP after the index). */
    const std::vector<std::uint8_t>& buffer()const{return buffer_;}
    /* The player's index file MRY/000<ply_id>: marker_models() and
       map_texture() address these bytes. */
    const std::vector<std::uint8_t>& index_buffer()const{return index_buffer_;}
    /* Bumped whenever the index / floor resources change (reload keys). */
    unsigned index_generation()const{return index_generation_;}
    unsigned stage_generation()const{return stage_generation_;}
    /* MapViewMain / MapLightMain state of the last control() tick: the view
       matrix every draw matrix already contains, the easy-multi light in
       camera space (fCx..fCz), its colour (pr,pg,pb)/map_scale and ambient. */
    const NJS_MATRIX& view_matrix()const{return vew_mtx_;}
    const NJS_VECTOR& light_camera_direction()const{return light_camera_;}
    const std::array<float,3>& light_color()const{return light_color_;}
    const std::array<float,3>& ambient()const{return ambient_;}
    /* The sound effects requested this tick, in call order (CallSystemSe). */
    const std::vector<std::pair<int,int>>& sounds()const{return sounds_;}
    /* Files requested through bhReadMapData, in call order. */
    const std::vector<unsigned>& requested_files()const{return requested_files_;}

    /* The realized stage map model. Runs MapPurgeTree (map.c:1245) on the
       handed-over objects, as the original does in MP_MOD_DAT_SET. */
    void set_map_objects(std::vector<MapObject> objects);
    const std::vector<MapObject>& map_objects()const{return objects_;}

    /* MapCnvStatus2Flag (map.c:256): flag groups 8 and 9 from player id and
       inventory. Exposed for the component test and run by init(). */
    void convert_status_to_flags(const MapHost& host,const MapServices& services);
    /* MapGetFloorNo (map.c:1207) on the floor table of a loaded index file. */
    static int get_floor_no(const std::uint8_t* data,std::size_t size,int rom_no,float pos_y);
    /* MapCncInit/MapCncConnect (map.c:2795/2896) and MapCheckNextMap (map.c:2936). */
    void connection_init(int map_num,int flr_num);
    void connection_connect(const std::uint16_t* data,std::size_t count,const MapServices& services);
    void connection_set(int dst,int src,int status);
    MapNext check_next_map()const;
    /* GetGameMode (map.c:3018). */
    static int game_mode(int stg_no){return stg_no>=7?0:1;}

private:
    /* func_wrk_typ slot; the typed works (FT/FS/FG/FZ/FI/FS_WRK/FC/FM) alias
       the same 64 bytes in the original, so one word array carries them all:
       w[0..2] = param0..2, w[3..11] = FreeWrk[0..8] (map.h:109). */
    union Word {std::int32_t i;float f;std::uint32_t u;Word():i(0){}};
    struct Func {
        int mode=0;
        std::array<Word,12> w{};
        int procedure=-1;
        int prev=-1,next=-1;
        bool allocated=false;
    };
    enum Procedure {
        ProcBackDraw,ProcZoomScreen,ProcTaskMain,ProcMapDraw,ProcCompass,
        ProcModeMessage,ProcZoomCursor,ProcZoomInformation,ProcGaugeDraw,
        ProcSpriteDraw,ProcSilhouetteDraw,ProcArrowDraw,ProcArrowDraw2
    };
    enum Task {TaskWait,TaskRead,TaskNormal,TaskZoom,TaskExit};

    /* map_wrk (map.h:146) held natively, field for field. */
    int status_=0,map_mode_=RECVX_MP_MOD_FIRST,time_=0,fil_mode_=0,prti_no_=5;
    unsigned pad_ps_=0;
    float pad_al_=0,pad_ar_=0,pad_ax_=0,pad_ay_=0;
    int map_no_=0,stg_no_=0,rom_no_=0,flr_no_=0,map_flr_=0;
    int ply_stg_=0,ply_rom_=0,ply_flr_=0;
    NJS_POINT3 vew_pos_{},vew_max_{},vew_min_{},dst_pos_{},vew_pos_bak_{};
    std::array<std::int32_t,3> vew_ang_{};
    float vew_zom_=600.0f,vew_zom_bak_=600.0f,dst_zom_=600.0f;
    NJS_MATRIX vew_mtx_{},cur_mtx_{};
    /* map_wrk::cur_rom (map.h:181, offset 0x1B0) is never read by any routine
       in map.c, so the port keeps no field for it. */
    int chk_rom_=0,cur_obj_=-1;
    std::size_t map_code_=0;         /* map_cdeP as a byte offset into buffer_ */
    int cde_bol_=0;
    float bck_p0_[2]={0,0},bck_p1_[2]={640.0f,480.0f},bck_depth_=-1600.0f;
    float map_scale_=0.001f,lgt_scale_=0.001f;
    float ar_=0,ag_=0,ab_=0,pr_=0.6f,pg_=0.6f,pb_=0.6f;
    std::int32_t pitch_=-16384,yaw_=0;
    std::array<RECVX_MAP_ARGB,32> pal_{};
    std::array<std::uint32_t,3> col_{};
    int bnk_mde_=0,bnk_stg_=0,bnk_flr_=0,bnk_rom_=0;
    int bnk_tag_rom_=0,bnk_tag_pal_=0,bnk_tag_pal_wal_=0;
    std::array<std::size_t,10> mes_buf_{};
    std::vector<MapTag> tags_;
    int cur_tag_=-1,fcs_tag_=-1;
    MapNext map_nxt_{};
    std::vector<MapConnection> cnc_;
    int cnc_map_=0,cnc_flr_=0;

    /* Host/service bindings and the loaded file. */
    MapHost host_{};
    MapServices services_{};
    std::vector<std::uint8_t> buffer_,index_buffer_;
    unsigned index_generation_=0,stage_generation_=0;
    NJS_VECTOR light_world_{},light_camera_{};
    std::array<float,3> light_color_{},ambient_{};
    std::size_t index_floor_table_=0;
    std::vector<MapObject> objects_;
    void purge_tree();               /* MapPurgeTree (map.c:1245) */
    std::vector<MapChunk> marker_models_;
    MapChunk map_texture_{},stage_texture_{},stage_model_{};
    MapDrawList draws_;
    std::vector<std::pair<int,int>> sounds_;
    std::vector<unsigned> requested_files_;
    bool released_map_texture_=false,released_stage_texture_=false;
    bool restores_room_texture_=false,game_map_flag_cleared_=false;

    /* Func list: the original ring of (func_num + 2) slots with the free list
       head at index 1 and the busy list head at index 0 (map.c:1291). */
    std::vector<Func> funcs_;
    static constexpr int kBusy=0,kFree=1;
    void func_init(int func_num);
    int func_alloc(int procedure,int param0);
    void func_free(int index);
    void func_del(int index);
    void func_ins(int base,int index);
    void func_exec();
    bool func_live(int index)const;

    int entry_task(int task,int chg_mde,int param0);
    void entry_sprite(int set_no,int mode);

    void pad_main();
    void view_main();
    void light_main();
    void palette_main();
    void code_process();
    void bool_set(int value,int mod);

    void draw_marker(int mrk_no,NJS_POINT3 pos,int pal_no);
    void draw_background(float depth,const float* p0,const float* p1);
    void draw_sprite(const NJS_POINT3& pos,int col,int spr_no);
    void draw_line(const float* src,const float* dst,float pri,std::uint32_t pal);
    void draw_line2(const float* src,const float* dst,float pri,std::uint32_t pal);
    void draw_fill(const float* src,const float* dst,float pri,std::uint32_t pal);
    void draw_poly_fill(const float* pnt,float pri,std::uint32_t pal);
    void draw_message(int rom,float x,float y);

    int read_map_data(unsigned file);
    void analyze_index();
    void analyze_stage();
    void tag_init();
    void tag_entry(const NJS_MATRIX* base,int rom_no,const NJS_POINT3& pos);
    int tag_connect(int rom_no);
    int tag_center()const;
    MapConnection* connection_get(int map_no,int flr_no);

    /* The function and task procedures, one per original Fsub, Fspr and Ftsk. */
    int run_procedure(int index);
    int run_task(int task,int index);
    int fsub_map_draw(int index);
    int fsub_back_draw();
    int fsub_zoom_screen(int index);
    int fsub_task_main(int index);
    int fsub_compass(int index);
    int fsub_mode_message(int index);
    int fsub_zoom_cursor(int index);
    int fsub_zoom_information(int index);
    int fsub_gauge_draw(int index);
    int fsub_gauge_draw_x(int index);
    int fsub_gauge_draw_z(int index);
    int fspr_sprite_draw(int index);
    int fspr_silhouette_draw(int index);
    int fspr_arrow_draw(int index);
    int fspr_arrow_draw2(int index);
    int ftsk_map_wait();
    int ftsk_map_exit();
    int ftsk_map_read(int index);
    int ftsk_map_normal(int index);
    int ftsk_map_zoom(int index);
};

/* MapCnvArgb2Color (map.c:2563). */
std::uint32_t map_argb_to_color(const RECVX_MAP_ARGB& argb);

} // namespace recvx

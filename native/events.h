#pragma once
#include "inventory.h"
#include "messages.h"
#include "doors.h"
#include "scene.h"
#include <array>
#include <functional>
#include <set>
namespace recvx {
struct EventCommand {unsigned opcode=0,task=0,event=0;std::size_t offset=0;std::vector<std::uint8_t> bytes;unsigned loop_remaining=0,loop_total=0;unsigned simulation_flags=~0u;};
struct Interaction {unsigned flags=0,type=0,attribute=0;int floor=0;NJS_POINT3 position{},extent{};std::array<unsigned,4> parameters{};};
struct EventTaskState {unsigned event,level,delay;std::size_t offset;bool blocked;unsigned wpnl_no=0;};
class EventVm {
public:
    EventVm(const std::vector<std::uint8_t>& room,Inventory& inventory,MessageDialog& dialog);
    std::array<std::array<std::uint32_t,32>,17> flags{};
    std::array<unsigned,256> values{};
    std::vector<Interaction> interactions;
    std::vector<EventCommand> pending_bindings;
    std::vector<std::string> errors;
    // event.c:36 bhIfelFlg unter null: ein ENDIF/ELSE ohne IF (event.c:413/425) zieht den
    // globalen Zaehler ins Minus. bhScenarioCheck (:13068) und bhEnd (:388) setzen ihn auf 0.
    // event.c bhIfelFlg/G_Sp: globale Werte aller Tasks eines Durchlaufs (bhScenarioCheck setzt bhIfelFlg 0).
    int ifel_=0;std::int64_t gsp_=0;
    std::vector<unsigned> dispatched;
    bool menu_open=false,status_requested=false;
    bool game_over_requested=false; // *(int*)&sys->gov_md0 = 0 aus bhCheckEvtTimer (player.c:1305/1326); der Host verbraucht es
    unsigned change_player=0; // sys->cng_pid (bhPlayerChangeSet, cb_flg 0x80)
    std::array<unsigned,4> map_parameters{}; // sys->mp_prm (bhMapSystemOn, cb_flg 0x10000)
    unsigned tick_count=0,event_timer=0,com_number=0; // sys->com_num
    // sys->evt_tmd/evt_tim/evt_tdg/evt_fcd/evt_fcdct.  This is distinct
    // from Event_T_timer above: it is the visible, gameplay-critical timer
    // installed by bhEvtTimerSet.
    unsigned event_deadline_mode=0,event_deadline_ticks=0,event_deadline_danger=0;
    unsigned event_deadline_voice_tick=0,event_deadline_voice_count=0;
    unsigned player_flags=0; // plp->flg, used by bhCheckEvtTimer modes 2/3
    // bhEtask[1].wpnr_no: nur bhPlyHandChange (event.c:7418/7449) schreibt und liest es. bhEtask ist
    // global; bhInitEvent (:345-348) und Event_init (:12956) lassen wpnr_no stehen, es ueberlebt
    // also jeden Raumwechsel (inherit_progress).
    unsigned parked_wpnr_no=0;
    // sdfunc.c:90 NextSoundInfo: die Vorgaben fuer Musik und Raumton des NAECHSTEN Raums,
    // die bhSetNextRoomBgm (event.c:2574) und bhSetNextRoomBgSe (:2612) schreiben. In der
    // ausgelieferten Fassung hat die Struktur KEINEN Leser: ueber alle 154 Originalquellen
    // steht sie nur in diesen beiden Befehlen und in ihrer Definition. Die Befehle sind
    // damit im Original ohne hoerbare Wirkung; der Port fuehrt den Zustand trotzdem an
    // einer Stelle, damit sie gebunden sind und die Werte nachweisbar bleiben.
    struct NextSound {
        unsigned bgm_com=0,bgm_point=0,bgm_offset=0,bgm_number=0,bgm_fade=0;
        // BgSe-Felder sind im Original zweielementig (sdfunc.c:39/40/105).
        std::array<unsigned,2> se_com{},se_point{},se_offset{},se_number{},se_fade{},se_set{};
    } next_sound;
    // Renderer/world bindings are distinct from opcode parsing. Unbound
    // commands are retained with exact operands and reported by the runtime.
    std::function<bool(const EventCommand&)> binding;
    std::function<void(unsigned)> task_started; // Event_init resets interpolation keys only.
    std::function<bool(unsigned)> enemy_dead;
    std::function<std::optional<int>(unsigned)> enemy_health;
    std::function<void(unsigned,int)> set_enemy_health;
    std::function<void(unsigned)> enemy_disable; // bhEneSetCk
    std::function<void(unsigned)> item_disable; // bhItmCk/bhItmSetCk: sys->itwp[n].stflg |= 0x1000000
    // bhCheckExmAtari case 4 liest sys->itwp[prm0].flg/id; false = kein Raumitem (itm_n ueberschritten).
    std::function<bool(unsigned,unsigned& flg,unsigned& id)> item_work;
    std::function<void(unsigned)> set_player_mode; // *(int*)&pp->mode0 = mode0|mode1<<8|mode2<<16|mode3<<24
    // bhCamCheck: active event camera (cam.flg & 2) with evc_no/keyf_no.
    std::function<bool(unsigned&,unsigned&)> event_camera;
    std::function<unsigned(unsigned,unsigned)> animation_frame;
    std::function<std::optional<NJS_POINT3>(unsigned,unsigned)> object_position;
    // event.c:7794 bhEtcAtariEnePosSet: ene[enep[n].wrk_no].mlwP->owP[node].mtx[12..14].
    std::function<std::optional<NJS_POINT3>(unsigned enemy,unsigned node)> enemy_joint_position;
    // event.c:7865 bhEtcAtariEvtPosSet: sys->itwp[n].px/py/pz.
    std::function<void(unsigned item,NJS_POINT3)> set_item_position;
    // sys->evt_posno[4] (types.h:599, SAVEFILE 0x820): Platz in rom->posp, den 0xb3 waehlt und
    // 0xb4 sowie Common_controll 51/52 lesen; global wie die Ereignisflags.
    std::array<std::uint8_t,4> evt_posno{};
    // rom->posp (room +0x24, pos_n +0xA4; POS 16 Bytes).
    const std::vector<NJS_POINT3>& room_positions()const{return positions_;}
    std::function<bool(unsigned)> audio_active;
    // sdfunc.c:2178 GetTimeAdx -> adxwrap.c:650 GetAdxPlayTime: Abspielzeit des ADX-Stroms
    // in Hundertstelsekunden, fuer bhAdxTimeCk (0x1E).
    std::function<unsigned(unsigned)> adx_time;
    std::function<void(unsigned)> audio_stop;
    // CallSystemVoice (ADV/PatId[3], VoiceNo + 16) for timer mode 3.
    std::function<void(unsigned)> system_voice;
    std::function<void(unsigned)> system_se; // CallSystemSe(0, n) of bhControlMessage/GetItem
    std::function<Message(unsigned,unsigned)> item_message;
    // RoomWorld::start_item_box: obwp[object] becomes type 100 (bhObjItmBox).
    std::function<bool(unsigned)> item_box_object;
    std::function<bool()> movie_active;
    std::function<unsigned()> movie_time; // GetMwPlayTimeEx, centiseconds.
    // bhPadCheck: (pad_ps, pad_on) of the current 30 Hz pass. pad_ps holds the
    // newly pressed logical bits (edge), pad_on the held ones (bhSetPad).
    std::function<std::pair<unsigned,unsigned>()> pad_state;
    std::function<unsigned()> player_yaw; // plp->ay
    // bhObjDposCk (event.c:7605): obwp[n].ay und rom->enep[n] -> mdl->objP[node].ang[0].
    std::function<bool(unsigned,int&)> object_ay_get;
    std::function<void(unsigned,int)> object_ay_set;
    std::function<bool(unsigned,unsigned,int&)> enemy_node_ax_get;
    std::function<void(unsigned,unsigned,int)> enemy_node_ax_set;
    std::function<void(unsigned)> set_player_yaw;
    /* system.c:742 (bhCheckSubTask, pass in which cb_flg 4 falls): plp->mode3 = 0. */
    std::function<void()> release_cinematic_player;
    std::function<bool(const DoorRequest&)> door;
    void inherit_progress(const EventVm& previous);
    static constexpr unsigned room_change_gm_mask=0x9B8C00CBu; // system.c:185 bhInitRoomChangeSystem
    /* bhInitEvent, event.c:352: enemy pass before scd0 and scheduler. */
    std::function<void()> initialize_enemies;
    void initialize();
    void tick();
    void finish_room_exit(); // final bhControlEvent pass before bhStartDoorDemo
    void advance_game_clock(unsigned vertical_blanks=2);
    // bhCheckEvtTimer, called by bhMainSequence after the VSync countdown.
    void check_event_deadline();
    bool event_deadline_visible()const;
    void start(unsigned level,unsigned event);
    // Whole-instruction length of Common_controll `command` (0x69 included); 0 = no original case.
    static std::size_t common_controll_length(unsigned command,unsigned byte2,unsigned byte3);
    // Whole-instruction length of Obj_controll `command` (0x66 included): 2/4 for cases 1/20, 1 for every
    // other byte (default branch leaves bhScePtr on the command byte, which runs as the next opcode).
    static std::size_t obj_controll_length(unsigned command);
    // hitchk.c bhCheckExmAtari, types 0/3/4. `position` is plp->px/pz after the wall
    // passes; `front`/`front_offset` are EXP_WORK fpx/fpz and arn (player.c:1585),
    // the source of the type 0 and type 3 probes pxn[0]/pxn[3] (hitchk.c:4605/:4614).
    // The three-argument form is a standing player: fpx == px, arn == 0.
    bool interact(NJS_POINT3 position,Angle yaw,int floor=0);
    bool interact(NJS_POINT3 position,Angle yaw,int floor,NJS_POINT3 front,float front_offset);
    // player.c:1678, the flag part of the bhCheckExmAtari gate: !(plp->stflg & 0x10080),
    // !(plp->flg & 0x4000004), !(sys->cb_flg & 0x4017). mode0 == 1 and pad_ps 0x200
    // stay with the caller (OK edge of the free player).
    bool exm_flags_clear()const{return !(flags[14][0]&0x10080u)&&!(flags[13][0]&0x4000004u)&&!(flags[10][0]&0x4017u);}
    bool use(unsigned item,unsigned* message=nullptr,unsigned slot=~0u);
    void begin_status(); // StatusMain clears the previous ItemUse request.
    // player.c:1072 bhCheckSubPack: gm_flg 0x8000000 (10 Plaetze) aus ev_flg 6/7/8 fuer ply_id 0/1/2.
    void check_sub_pack();
    void check_floor(NJS_POINT3 position,Angle yaw,int floor=0);
    std::function<void(NJS_POINT3,Angle,int)> floor_binding;
    const std::vector<Interaction>& floor_areas()const{return floors_;}
    // rom->flrp[i].prm0..3 schreiben (en27.c:101/:160 prm2). false = Index ausserhalb.
    bool set_floor_parameter(std::size_t index,unsigned which,unsigned value){if(index>=floors_.size()||which>=4)return false;floors_[index].parameters[which]=value;return true;}
    // sys->exm_attr / fixcno / fixkno of the last bhCheckExmAtari camera (hitchk.c:4790).
    unsigned exm_attr=0,fixcno=0,fixkno=0;
    // Native owners for game.c's exm handling: sys->pt_flg (clear,set), rom->lgtp[1].flg 0x2,
    // ef_flg 0x10 from cam.flg 0x20, bhSetFixedCut, bhSetEventFixedCut, bhCheckCut(1).
    std::function<void(unsigned,unsigned)> presentation_flags;
    std::function<void()> exm_light_off,exm_cut_effect,return_cut;
    std::function<void(unsigned)> fixed_cut;
    std::function<void(unsigned,unsigned)> event_fixed_cut;
    void control_exm(); // bhMainSequence, game.c:115-286, before bhControlMessage/bhControlEvent
    void confirm_dialog();
    void cancel_dialog();
    void close_dialog(bool answered);
    void choose_dialog(int direction);
    bool skip_cinematic();
    void service_item_request(); // bhCheckSubTask cb_flg 0x4010 -> GetItem
    // swork.statusflg 0x40: the status screen is still sliding, so GetItem's
    // testmode 1..4 wait. The runtime binds this to the status slide.
    std::function<bool()> status_busy;
    // Host debug menu: an inventory change from outside GetItem drops a pending
    // acquisition instead of letting GetItem decide twice on a changed stock.
    void cancel_item_request();
    // sub1.c StatusMain: GetItem runs once per status pass. Call it per frame
    // while an acquisition screen is up (message 158 and testmode 3 land in the
    // pass after the insertion).
    void service_pickup();
    Message room_message(unsigned index)const; // rom->mesp[index]
    unsigned pickup_display()const{return pickup_display_;}
    unsigned acquired_file()const{return acquired_file_;}
    // sub1.c ItemTaskCheck (~3116): completing the status task consumes both
    // GetItem and GetFile requests. Scripts may set both for a file pickup.
    void finish_file(){acquired_file_=~0u;flags[10][0]&=~0x20010u;}
    std::vector<EventTaskState> task_states()const;
    bool flag(unsigned matrix,unsigned index)const;
    void set_flag(unsigned matrix,unsigned index,unsigned mode);
private:
    bool door_scenario_pending_=false;
    bool pad_cleared_=false; // hitchk.c sys->pad_ps = 0 for the rest of the pass
    bool request_door(const DoorRequest& request);
    struct Loop {std::size_t condition=0,body=0;unsigned count=0;bool repeat=false;};
    struct Task {bool active=false,blocked=false;unsigned event=0,level=0,delay=0,cnt2=0,cnt3=0,wpnl_no=0;float addpx=0;std::size_t pc=0;std::vector<Loop> loops;std::array<unsigned,32> loop_counts{};};
    std::vector<std::uint8_t> room_;
    std::size_t base_=0,scd0_=0,scd1_=0,messages_=0;
    std::vector<std::size_t> scripts_;
    std::vector<Interaction> floors_;
    Inventory& inventory_;MessageDialog& dialog_;
    bool opening_hint_=false,status_use_pending_=false;
    unsigned pickup_item_=0,pickup_display_=0,pickup_mode_=0;
    bool pickup_decision_pending_=false; // GetItem testmode 0 still owed (sub1.c:3452 gate)
    unsigned acquired_file_=~0u;
    std::vector<unsigned> room_items_,room_item_flags_; // rom->itmp id/flg
    std::vector<NJS_POINT3> positions_;
    void disable_item(const Task& task,unsigned op,std::size_t pc,unsigned item);
    void begin_pickup(unsigned item);
    void begin_file(unsigned item);
    void pickup_result(const PickupResult& result);
    void finish_pickup();
    void show_dialog(Message message,unsigned index); // bhSetMessage resets response work
    std::array<Task,16> tasks_{};
    std::set<std::pair<unsigned,std::size_t>> reported_;
    void run(Task& task,bool scenario=false);
    int instruction(Task& task);
    void world(Task& task,unsigned opcode,std::size_t length);
};
}

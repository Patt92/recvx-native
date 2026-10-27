#pragma once
#include "events.h"
#include "actor.h"
#include "ui.h"
#include "audio.h"
#include <functional>
#include <memory>
namespace recvx {
// gameover.c dispatcher and GV_WORK as a native hand port (gameover.cpp); no generated or linked original body.
class GameOver {
public:
    enum class Result {Running,Retry,Title};
    GameOver(const std::filesystem::path& data,const std::filesystem::path& mods={});
    ~GameOver();
    void begin(EventVm& events,unsigned character,int mode,unsigned weapon_type=0,unsigned damage_level=0,const PlayerAsset* body=nullptr);
    // Task 14 (bhSysCallGameover). Die Fades von gameover.c (:122/262/330/424/485/556/564) gehen
    // als bhSetScreenFade auf den einen prozessweiten sys->fade_* (system_screen_fade, screen_fade.h)
    // und setzen cb_flg 0x2 in events.flags[10][0]; md1 1/3 lesen dieses Bit (:422/514). Die
    // Steuerung (game.c:38-41) gehoert dem Spieltask: der Aufrufer ruft control_screen_fade vor tick.
    Result tick(unsigned pressed,EventVm& events,AudioMixer& audio);
    // bhDrawGameOver und in derselben Bildfolge bhDrawScreenFade des Spieltasks (game.c:109-112)
    // auf dem gemeinsamen Fade: Netz, dann Fade, dann die Aufforderung von md1 2.
    void draw(SoftwareRenderer& image,const GameUi& ui)const;
    bool active()const;
    // Host: ein Snapshot-Laden ersetzt den Spielstand wie bhPopGameData und beendet Task 14 ohne Rueckgabe.
    void abort();
    unsigned phase()const;
    // Host metrics: continue prompt shown (md1 2 with count at 0xFF) and its cursor (0 = continue).
    bool prompt()const;
    int choice()const;
    unsigned count()const;
    // gov_etb explosions requested so far (gameover.c:535-541), and how many of those the
    // world owner could not place: death_blast unbound or it returned false.
    unsigned blasts()const;
    unsigned unsupported_blasts()const;
    // gameover.c:539 bhSetExplosionEffect(&gov_etb[exp_ct].px): the effect package of the world
    // owner (RoomWorld::explode_effect, weapon.c:1812) under the scene camera cam.flg 0x40.
    // Returns false when a work set could not be placed; the owner reports the gap.
    std::function<bool(NJS_POINT3 position)> death_blast;
    // gameover.c:313-327: cam.flg 0x40, pers 11832, ln 0, angles 0, cam at (0, 10, 35), and
    // :296-297 pt_flg 0x15 (player 0x1, objects 0x4, effects 0x10); :567 pt_flg 0 on leave.
    std::function<void()> death_scene_leave;
    // game.c:318 bhAllDrawModel with pt_flg 0x15 under the scene camera: the world owner appends
    // the effects (and objects) of this pass to the scene bhDrawGameOver is drawn into.
    // body: der Spielerkoerper dieses Passes (game.c:357 bhPutModel(plp), eigene Texturtafel); der Weltbesitzer
    // lichtet ihn in seinem append()-Pass mit (gm_flg 0x80: bhSetEasyDirLight(0.6), light.c:279/992).
    std::function<void(Scene& scene,const SceneCamera& camera,Scene& body)> death_scene_append;
    // game.c:357 bhPutModel(plp) draws the body with plp->mlwP->texP = plp->txp[0] (gameover.c:313): the
    // player's own texture bundle as the runtime loaded it (loose-file overrides included). Without the
    // hook the asset's bundle is used; PlayerAsset::sample carries no textures of its own.
    std::function<std::vector<SceneTexture>()> death_body_textures;
    // bhMainGameOver md1 10 (gameover.c:287-299): Eintritt der Todesszene, plp->mnwP = mnwPb,
    // Lage 0/0/0 mit ay 35499, Motion PlMtnAct[wpntp][dmlvl][3] ab Bild 0 ohne Mischung.
    // gameover.c:289: PlMtnAct[((EXP_WORK*)plp->exp0)->wpntp][...->dmlvl][3] wird beim Eintritt der
    // Szene gelesen, nicht beim Start des Game Over; der Spielerbesitzer liefert die EXP_WORK-Werte.
    std::function<void(unsigned& wpntp,unsigned& dmlvl)> death_scene_levels;
    std::function<void(unsigned motion)> death_scene_enter;
    // gameover.c:446 bhSetMotion(plp, mtn_add, 0, NULL) in jedem Pass der Szene: der Knotenteil
    // (SetMtn*Hokan auf plp->mlwP->objP) beim Besitzer des Spielerbaums; die Uhr fuehrt GameOver.
    std::function<void(unsigned motion,std::int64_t frame,unsigned hokan_count,int hokan_rate)> death_motion_nodes;
    // gameover.c:453 bhCalcModel(plp): die objP-Knoten, die bhDrawGameOver zeichnet.
    std::function<ModelNodeOverrides()> death_pose;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}

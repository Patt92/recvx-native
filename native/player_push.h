#pragma once
#include "actor.h"
#include "collision.h"
#include "player_push_core.h"
namespace recvx {
// Owner of the crate O_WRKs (bhObj001) with their sys->mwalp/metcp/mflrp
// entries and of the running bhCPM2_act_psh state. Player fields come in as
// views of the one native player work and go back out; see player_push_core.c.
class PlayerPush {
public:
 PlayerPush(float room_height,const std::array<float,32>& grand,const std::vector<RECVX_PUSH_OBJECT>&,const PlayerAsset&,const RECVX_PUSH_HOST&);
 ~PlayerPush();
 PlayerPush(const PlayerPush&)=delete;PlayerPush& operator=(const PlayerPush&)=delete;
 void enabled(unsigned sp_flg){recvx_push_enabled(core_,sp_flg);}
 // rom->walp followed by the sys->mwalp entries allocated before the crates.
 void update_walls(const std::vector<CollisionWall>& prefix);
 void update_object(unsigned i,NJS_POINT3 p);
 // bhControlObjItm -> bhObj001 for every crate (cb_flg, plp->psh_idx, plp->stflg).
 void control(unsigned cb_flg,int psh_idx,unsigned stflg);
 // The player's bhCheckWallEx marks attr 0x20000 on crate walls; `combined`
 // is the pass's wall list and `base` the index of the first crate entry.
 void store_walls(const std::vector<CollisionWall>& combined,std::size_t base);
 // bhCPM2_act_wlk: psh_ct > 5 without stflg 0x80 enters mode2 18.
 bool begin(int psh_ct,const SceneSpawn& actor,int dmlvl,int wpntp,unsigned& flg,unsigned& stflg,unsigned& st_flg);
 // bhControlPlayerPad + bhCPM2_act_psh + bhSetMotion; writes the player view back.
 bool tick(bool forward,unsigned& flg,unsigned& stflg,unsigned& st_flg,unsigned& flg2,SceneSpawn&,std::size_t&,std::int64_t&);
 bool active()const{return state_.active;}
 const RECVX_PUSH_STATE& state()const{return state_;}
 std::vector<CollisionWall> walls()const;
 std::vector<RECVX_STAIR_AREA> steps()const;
 // hitchk.c:4965/5003 bhSetUseKaidanFlag/bhClrUseKaidanFlag on a sys->metcp entry: attr 0x400000.
 void mark_step(unsigned index,bool used){recvx_push_step_attr(core_,index,used?0x400000u:0u,used?0u:0x400000u);}
 std::vector<RECVX_STAIR_AREA> floors()const;
 NJS_POINT3 object(unsigned i)const;
 // SetMtnSlow (mtn_md 8): the half-step reverse of MTN49 samples between keys.
 ModelNodeOverrides pose()const;
private:
 const PlayerAsset& asset_;RECVX_PUSH_CORE* core_=nullptr;RECVX_PUSH_STATE state_{};
};
}

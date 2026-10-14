// effsub2.c:2014/2092 bhEff_E11_SearchLight(Draw), :2353/2390 bhEff_E16_LaserSight(Draw),
// :2296 bhEff_E11_CheckCollisionPlayer and hitchkl.c:735 bhCollisionCheckL2MDL on the
// player's original hit model (mdl[3]). The link/player providers below are test
// providers with stated values, not a played room.
#include "effect_sprites.h"
#include "hit_model.h"
#include "actor.h"
#include "ninja_math.h"
#include "enemies/en16.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();
    // --- player hit model -----------------------------------------------------
    {
        const auto player=PlayerAsset::load(argv[1],10);
        const auto* hit=player.model(3);check(hit!=nullptr,"plp->mdl[3] hit model missing");
        const auto objects=decode_hit_model(hit->encoded_model());
        check(objects.size()==player.bones(),"mdl[3] object count differs from plp->mlwP->obj_num");
        const auto nodes=player.pose(0,0);
        unsigned tested=0;
        for(std::size_t i=0;i<objects.size()&&i<nodes.size();++i){
            if(!objects[i]||objects[i]->vertices.empty())continue;
            NJS_MATRIX m;std::copy(nodes[i].value,nodes[i].value+16,m);
            NJS_POINT3 local=objects[i]->center,center;njCalcPoint(&m,&local,&center);
            // A segment through the bounding centre along z from both sides, and one far beside it.
            NJS_POINT3 end{center.x,center.y,center.z-50};
            const auto front=line_hit_model({center.x,center.y,center.z+50},end,*objects[i],nodes[i].value);
            check(front!=HitModelResult::IndexOutside,"hit model polygon index outside its point list");
            if(front==HitModelResult::Hit){check(end.z>center.z-50&&end.z<center.z+50,"L2PL hit did not shorten p2 onto the mesh");++tested;}
            NJS_POINT3 far_end{center.x+objects[i]->radius+10,center.y,center.z-50};
            check(line_hit_model({center.x+objects[i]->radius+10,center.y,center.z+50},far_end,*objects[i],nodes[i].value)==HitModelResult::Miss,"line outside mdl->r reported a hit");
        }
        check(tested>0,"no hit-model object is crossed by a line through its centre");
    }
    // --- shared test providers -----------------------------------------------
    NJS_MATRIX link;njUnitMatrix(&link);link[12]=1;link[13]=10;link[14]=2;   // node faces -z
    set_effect_link_lookup([&](unsigned kind,unsigned,unsigned,NJS_MATRIX& out){if(kind!=1)return false;std::copy(link,link+16,out);return true;});
    int player_index=-1;NJS_POINT3 player_point{1,10,-20};
    set_effect_player_hit([&](NJS_POINT3,NJS_POINT3& p2){if(player_index>=0)p2=player_point;return player_index;});
    static const std::vector<CollisionWall> no_walls;set_effect_walls(&no_walls,0);
    int next_extra=100;set_effect_extra_allocator([&](){return next_extra++;},[](int){});
    EffectContext context;context.sp_flg=8;
    // --- 270 laser sight -------------------------------------------------------
    {
        RECVX_ENEMY_WORK owner{};owner.flg=1;owner.type=1;owner.owner_index=0;
        unsigned char block[0x68]{};owner.exp0=block;
        EffectSpawn spawn;spawn.id=270;spawn.flg=1;spawn.scale={0,0,0};spawn.linked_work=&owner;
        EffectSprite s;initialize_effect_sprite(s,spawn);
        bool queued=true;step_effect_sprite(s,context,&queued);
        check(!queued&&s.mode0==1,"bhEff_E16_LaserSight case 0 must only arm func and mode0 1");
        step_effect_sprite(s,context,&queued);check(!queued&&s.mode0==1,"mode0 1 must stay idle");
        s.mode0=2;player_index=3;step_effect_sprite(s,context,&queued);
        check(queued&&s.ct0==3,"laser draw must store the CheckCollisionPlayer object index in ct0");
        check(s.gpx==player_point.x&&s.gpz==player_point.z,"laser gp must be the shortened player hit point");
        check(recvx_en16_eff_num(&owner)==1,"bhEne16_AddEffPos not called once per draw");
        check(s.quads.size()==16&&s.lines.size()==2,"laser draw: 16 cone polygons, main line plus one trail line (type != 0)");
        for(const auto& q:s.quads)check(q.pn==3&&q.gouraud&&!q.textured&&!q.fogged&&q.source==8&&q.destination==10,"laser polygon state differs from njDrawPolygon3D after 8/10");
        // The apex is black with alpha 0, the rim red 100 with 255*rate.
        check((s.quads[0].vertices[0].col>>24)==0&&((s.quads[0].vertices[1].col>>16)&255u)==100,"laser polygon colours differ");
        owner.stflg|=0x1000000u;step_effect_sprite(s,context,&queued);check(!queued,"stflg 0x1000000 must suppress the ef_fnc entry");
        owner.flg=0;step_effect_sprite(s,context,&queued);check(!s.flg,"laser must release when lkwkp->flg is 0");
    }
    // --- 261 search light ------------------------------------------------------
    {
        RECVX_ENEMY_WORK owner{};owner.flg=1;owner.owner_index=0;
        EffectSpawn spawn;spawn.id=261;spawn.flg=1;spawn.scale={0,0,0};spawn.linked_work=&owner;
        EffectSprite s;initialize_effect_sprite(s,spawn);
        bool queued=true;step_effect_sprite(s,context,&queued);
        check(!queued&&s.mode0==0xFF&&s.extra&&s.extra_slot==100,"case 0: mode0 0xFF with bhSetExtraEffectWork");
        s.mode0=1;s.axp=0x38e;player_index=-1;step_effect_sprite(s,context,&queued);
        check(queued&&s.ct0==1&&s.ax==0x38e/16&&s.ct1==10&&s.ct2==0,"case 1 ramp (ax += (short)(axp-ax)/16, ct1 += 10)");
        check(s.quads.size()==16&&s.lines.size()==2,"search light: 16 fan polygons and two sweep lines");
        s.mode0=2;player_index=0;step_effect_sprite(s,context,&queued);
        check(s.ct2==1&&s.ax==0x38e&&s.ct0==32,"player in the cone in mode0 2 must set ct2 (alarm)");
        s.mode0=3;s.ct0=3;step_effect_sprite(s,context,&queued);check(s.mode0==0&&s.ct0==0,"case 3 fade must return to mode0 0");
        // Gouraud alpha per vertex survives into the scene triangle.
        Scene scene;SceneCamera camera;camera.perspective=0x2000;
        EffectSprite triangle;triangle.id=302;triangle.pn=3;triangle.gouraud=true;triangle.untextured=true;triangle.bl_src=8;triangle.bl_dst=10;
        triangle.tv[0]={0,0,0,0,0,0xff102030u};triangle.tv[1]={1,0,0,0,0,0x00102030u};triangle.tv[2]={0,1,0,0,0,0x80102030u};
        append_effect_sprite(scene,triangle,{0,0,0},{1,1,1},camera,0);
        check(scene.triangles.size()==1&&scene.triangles[0].vertex_alpha,"per-vertex effect alpha not carried into the scene");
        check(std::fabs(scene.triangles[0].vertices[1].alpha)<1e-6f&&std::fabs(scene.triangles[0].vertices[2].alpha-128.f/255.f)<1e-6f,"vertex alpha values differ");
    }
    std::cout<<"hit model L2MDL, laser sight 270, search light 261 and per-vertex effect alpha PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

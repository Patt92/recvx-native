#include "weapon_flash.h"
#include "chunk_blend.h"
#include "core_random.h"
#include <algorithm>
#include <stdexcept>

namespace recvx {
namespace {
struct UV {float u,v,width,height;};
// effsub1.c bhDrawWeaponEffect: original atlas rectangles, not replacement artwork.
constexpr UV muzzle_uv[2][4]={
    {{.265625f,0,.09375f,.09375f},{.359375f,0,.1875f,.09375f},
     {0,0,.09375f,.09375f},{.09375f,0,.125f,.09375f}},
    {{.265625f,.109375f,.0625f,.0625f},{.34375f,.109375f,.0625f,.0625f},
     {0,.09375f,.09375f,.09375f},{.09375f,.09375f,.09375f,.09375f}}};
constexpr UV flare_uv[2][9]={
    {{.625f,0,.0625f,.0625f},{.796875f,.0625f,.09375f,.0625f},
     {.96875f,.125f,.03125f,.09375f},{.640625f,.0625f,.0625f,.0625f},
     {.6875f,0,.15625f,.0625f},{.78125f,.34375f,.0625f,.125f},
     {.21875f,.1875f,.0625f,.0625f},{0,.1875f,.09375f,.0625f},
     {.65625f,.34375f,.0625f,.125f}},
    {{.546875f,0,.0625f,.0625f},{.90625f,.0625f,.09375f,.0625f},
     {.921875f,.125f,.03125f,.09375f},{.71875f,.0625f,.0625f,.0625f},
     {.84375f,0,.15625f,.0625f},{.859375f,.34375f,.0625f,.125f},
     {.296875f,.1875f,.0625f,.0625f},{.109375f,.1875f,.09375f,.0625f},
     {.71875f,.34375f,.0625f,.125f}}};
constexpr UV trail_uv[2][4]={
    {{0,.3125f,.03125f,.03125f},{.046875f,.3125f,.03125f,.03125f},
     {.09375f,.3125f,.03125f,.03125f},{.140625f,.3125f,.03125f,.03125f}},
    {{0,.3125f,.03125f,.03125f},{.046875f,.3125f,.03125f,.03125f},
     {.0625f,.25f,.0625f,.0625f},{0,.25f,.0625f,.0625f}}};
constexpr unsigned palette[2][3]={{0,0,1},{0,1,1}};
using Quad=std::array<NJS_POINT3,4>;

// World vertices go to the existing Scene. Camera projection belongs to its
// renderer, not a second camera or a transported console primitive packet.
void emit(Scene& scene,NJS_MATRIX& matrix,const Quad& points,const UV& uv,
          unsigned texture,std::uint32_t color,int source,int destination,bool reverse=false) {
    const unsigned order[2][3]={{0,1,2},{1,3,2}};
    for (const auto& triangle:order) {
        SceneTriangle out;out.texture=texture;out.textured=true;out.camera_mask=false;
        out.color=color;out.world_lit=true;out.depth_write=false;out.blend=SceneBlend::Ninja;
        out.blend_register=ninja_blend_register(source,destination);
        for (unsigned i=0;i<3;++i) {
            const unsigned n=triangle[i];auto point=points[n];njCalcPoint(&matrix,&point,&point);
            const bool right=bool(n&1)^reverse;
            out.vertices[i]={point,uv.u+(right?uv.width:0),uv.v+(n>=2?uv.height:0),{}};
        }
        scene.triangles.push_back(out);
    }
}
}

void WeaponFlash::tick() {
    queued=false;
    if (type>5) throw std::runtime_error("weapon flash type outside original table");
    // bhEff003 init falls through to control in the same tick.
    if (mode0==0) {
        tex_id=3;
        vertex.col=type==5?std::uint32_t(-0x5fa0):~0u;
        bl_src=type==5?9:8;bl_dst=type==3?6:10;
        if (type==3) {
            unsigned cursor=0;
            for (int i=7;i>=0;--i) {
                pv[cursor++]=int(182.04445f*(45.0f*i))&0xffff;
                const float random=-recvx_core_random_next()/-2.1474836E9f;
                pv[cursor++]=-(int(182.04445f*(70.0f+15.0f*random))&0xffff);
                jno[i]=i%3==0?1:0;
            }
        }
        ani_ct=ct0=ct1=0;mode0=1;
    }
    if (mode0==1) {
        if (type==0&&mdlver!=0&&ct1==0) {++ct1;return;}
        if (type==1||type==2||type==4) spd=ct0!=1?.8f:1.0f;
        queued=true;
        if (++ct0>=flr_no) mode0=2;
    } else if (mode0==2) flg=0;
}

void WeaponFlash::append(Scene& scene,unsigned texture,unsigned game_flags,const WallQuery& wall,const NJS_MATRIX* node)const {
    if (!queued||!flg||(flg&0x1000000)||(stflg&0x1000000)||
        ((game_flags&0x4000)&&(mdflg&0x40))) return;
    if (type>5||mdlver>1||pn!=4||ct0==0) throw std::runtime_error("invalid original weapon flash draw work");
    if (flg&0x20000000) throw std::runtime_error("weapon flash point-filter binding pending");
    const unsigned animation=ct0-1;
    if (animation>=(type==0||type==5?2u:type==3?4u:3u))
        throw std::runtime_error("weapon flash frame outside original UV table");
    NJS_MATRIX draw;
    std::copy_n(matrix,16,draw);
    if (!(flg&0x1000)) {
        if (!(flg&0x80)||(flg&0x200000)) {
            njUnitMatrix(&draw);njTranslate(&draw,position.x,position.y,position.z);
        } else {
            const auto* linked=node?node:linked_matrix;
            if (!linked) throw std::runtime_error("weapon flash linked node binding pending");
            std::copy_n(*linked,16,draw);njTranslate(&draw,offset.x,offset.y,offset.z);
        }
        njRotateXYZ(&draw,angles[0],angles[1],angles[2]);
        std::copy_n(draw,16,matrix);
    }
    const auto color=vertex.col;
    auto quad=[&](const Quad& points,const UV& uv,unsigned tex,int source,int dest,bool reverse=false) {
        emit(scene,draw,points,uv,texture+tex,color,source,dest,reverse);
    };
    if (type==0||type==5) {
        float off=0;
        if (type==0) {
            float scale=sx;
            if (mdlver==0) off=animation==0?0:.6f;
            else {off=.9f;scale=(animation==0?.8f:.6f)*sx;}
            njScale(&draw,scale,scale,scale);
        } else njScale(&draw,sx,sy,sz);
        const auto& front=muzzle_uv[mdlver][animation*2];
        quad({{{.5f,-.45f,-.45f},{.5f,.45f,-.45f},{.5f,-.45f,.45f},{.5f,.45f,.45f}}},front,animation,bl_src,bl_dst);
        const auto& side=muzzle_uv[mdlver][animation*2+1];
        quad({{{0,0,-.45f},{1.8f-off,0,-.45f},{0,0,.45f},{1.8f-off,0,.45f}}},side,animation,bl_src,bl_dst,true);
        quad({{{0,-.45f,0},{1.8f-off,-.45f,0},{0,.45f,0},{1.8f-off,.45f,0}}},side,animation,bl_src,bl_dst,true);
        return;
    }
    if (type==3) {
        if (!wall) throw std::runtime_error("weapon flash bhCheckL2Wall native binding pending");
        NJS_MATRIX world;std::copy_n(draw,16,world);njScale(&draw,sx,sy,sz);
        for (unsigned i=0;i<8;++i) {
            if (jno[i]<0||jno[i]>1) throw std::runtime_error("weapon trail UV selector outside original table");
            spd=1.0f+((1.4f*ct0)+(i%3));
            NJS_MATRIX probe;std::copy_n(world,16,probe);
            njRotateX(&probe,Angle(pv[i*2]));njRotateY(&probe,Angle(pv[i*2+1]));
            NJS_POINT3 end{0,0,-1.0f-spd};njCalcPoint(&probe,&end,&end);
            const auto hit=wall(position,{end.x-position.x,end.y-position.y,end.z-position.z},0x400);
            const float length=hit?*hit:1.0f+spd;
            NJS_MATRIX ray;std::copy_n(draw,16,ray);
            njRotateX(&ray,Angle(pv[i*2]));njRotateY(&ray,Angle(pv[i*2+1]));
            const auto& uv=trail_uv[jno[i]][animation];
            emit(scene,ray,{{{0,-.05f,-length},{0,.05f,-length},{0,-.05f,-.5f},{0,.05f,-.5f}}},uv,texture+1,color,bl_src,bl_dst);
            emit(scene,ray,{{{-.05f,0,-length},{.05f,0,-length},{-.05f,0,-.5f},{.05f,0,-.5f}}},uv,texture+1,color,bl_src,bl_dst);
        }
        return;
    }
    const float scale=sx*spd;njScale(&draw,scale,scale,scale);
    const float off=mdlver==0?0:.2f;
    const auto& front=flare_uv[mdlver][animation*3];
    const auto& side=flare_uv[mdlver][animation*3+1];
    const auto& radial=flare_uv[mdlver][animation*3+2];
    const unsigned first=palette[0][animation],second=palette[1][animation];
    Quad front_points{{{.3f,-.25f-off,-.25f-off},{.3f,.25f+off,-.25f-off},
                       {.3f,-.25f-off,.25f+off},{.3f,.25f+off,.25f+off}}};
    Quad side_points{{{0,0,-.45f-off},{1.5f,0,-.45f-off},{0,0,.45f+off},{1.5f,0,.45f+off}}};
    Quad vertical{{{0,-.45f-off,0},{1.5f,-.45f-off,0},{0,.45f+off,0},{1.5f,.45f+off,0}}};
    Quad spokes{{{.3f,-.2f-off,-1.0f-off},{.3f,.2f+off,-1.0f-off},{0,-.2f-off,0},{0,.2f+off,0}}};
    Quad cross{{{.1f-off,0,-1.0f-off},{.5f+off,0,-1.0f-off},{-.2f-off,0,0},{.2f+off,0,0}}};
    if (type==4) {
        // The cross-spoke Z signs differ from a rotated type-1 branch.
        auto turn=[](Quad& q){for (auto& p:q) {const float x=p.x;p.x=p.z;p.z=-x;}};
        turn(front_points);turn(side_points);turn(vertical);turn(spokes);
        cross={{{-1.0f-off,0,-.1f-off},{-1.0f-off,0,-.5f+off},
                {0,0,-.2f-off},{0,0,-.2f+off}}};
    }
    quad(front_points,front,first,bl_src,bl_dst);
    quad(side_points,side,first,bl_src,bl_dst,true);
    quad(vertical,side,first,bl_src,bl_dst,true);
    const unsigned count=type==2||mdlver!=0?4:6;
    const Angle step=count==4?16384:10922;
    for (unsigned i=0;i<count;++i) {
        quad(spokes,radial,second,8,10);quad(cross,radial,second,8,10);
        if (type==4) njRotateZ(&draw,step);else njRotateX(&draw,step);
    }
}
}

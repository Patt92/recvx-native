#include "wall_capsule.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <cmath>
namespace recvx {
namespace {
using V=RECVX_CB_VEC;using L=RECVX_CB_LINE;using C=RECVX_CB_CAPSULE;using S=RECVX_CB_SPHERE;
struct Box {V v[8];};
struct Plane {float px,py,pz,vx,vy,vz;}; // NJS_PLANE
// ps2_NaColi.c njDistanceP2P
float distance_p2p(const V& p1,const V& p2){const float dx=p2.x-p1.x,dy=p2.y-p1.y,dz=p2.z-p1.z;return njSqrt(dx*dx+dy*dy+dz*dz);}
// ps2_NaColi.c:5 njIsParalellL2L
int is_paralell_l2l(const L& l1,const L& l2){
    float x1=l1.vx,y1=l1.vy,z1=l1.vz;float n=njInvertSqrt(x1*x1+y1*y1+z1*z1);x1*=n;y1*=n;z1*=n;
    float x2=l2.vx,y2=l2.vy,z2=l2.vz;n=njInvertSqrt(x2*x2+y2*y2+z2*z2);x2*=n;y2*=n;z2*=n;
    return 0.995f<=std::fabs(x1*x2+y1*y2+z1*z2)?1:0;
}
// ps2_NaColi.c:446 njCollisionCheckSC
int check_sc(const S& sphere,const C& capsule){
    L line{capsule.c1.x,capsule.c1.y,capsule.c1.z,capsule.c2.x-capsule.c1.x,capsule.c2.y-capsule.c1.y,capsule.c2.z-capsule.c1.z};
    float length=capsule.r+sphere.r;V point;
    if(length<=recvx_cb_distance_p2l(&sphere.c,&line,&point))return 0;
    length=distance_p2p(capsule.c1,capsule.c2);
    if(length<distance_p2p(point,capsule.c1))point=capsule.c2;
    else if(length<distance_p2p(point,capsule.c2))point=capsule.c1;
    return distance_p2p(sphere.c,point)<capsule.r+sphere.r?1:0;
}
// ps2_NaColi.c:346 njCollisionCheckCC (the Ninja routine, not njplus npCollisionCheckCC)
int check_cc(const C& h1,const C& h2){
    L l1{h1.c1.x,h1.c1.y,h1.c1.z,h1.c2.x-h1.c1.x,h1.c2.y-h1.c1.y,h1.c2.z-h1.c1.z};
    L l2{h2.c1.x,h2.c1.y,h2.c1.z,h2.c2.x-h2.c1.x,h2.c2.y-h2.c1.y,h2.c2.z-h2.c1.z};
    if(is_paralell_l2l(l1,l2)){S sphere{h1.c1,h1.r};if(check_sc(sphere,h2))return 1;sphere.c=h1.c2;return check_sc(sphere,h2)?1:0;}
    float length=h1.r+h2.r;V p1,p2;
    if(length<=recvx_cb_distance_l2l(&l1,&l2,&p1,&p2))return 0;
    length=distance_p2p(h1.c1,h1.c2);
    if(length<distance_p2p(p1,h1.c1))p1=h1.c2;else if(length<distance_p2p(p1,h1.c2))p1=h1.c1;
    length=distance_p2p(h2.c1,h2.c2);
    if(length<distance_p2p(p2,h2.c1))p2=h2.c2;else if(length<distance_p2p(p2,h2.c2))p2=h2.c1;
    return distance_p2p(p1,p2)<h1.r+h2.r?1:0;
}
// ps2_NaColi.c:866 njCheckPlane4AndLine
int check_plane4_and_line(const V& p1,const V& p2,const V& p3,const V& p4,const V& pn,const L& line){
    float vx2=line.vx,vy2=line.vy,vz2=line.vz,vx1=pn.x,vy1=pn.y,vz1=pn.z,vx3=line.px,vy3=line.py,vz3=line.pz;
    float t3=vx2*vx2+vy2*vy2+vz2*vz2,t1=njInvertSqrt(t3);vx2*=t1;vy2*=t1;vz2*=t1;
    float t2=vx1*vx2+vy1*vy2+vz1*vz2;if(std::fabs(t2)<=0.025f)return 0;
    t2=-(vx1*(vx3-p1.x)+vy1*(vy3-p1.y)+vz1*(vz3-p1.z))/t2;
    float vx4=vx3+vx2*t2,vy4=vy3+vy2*t2,vz4=vz3+vz2*t2;
    vx1=vx4-vx3;vy1=vy4-vy3;vz1=vz4-vz3;
    t2=vx1*vx1+vy1*vy1+vz1*vz1;if(t3<t2)return 0;
    vx1-=line.vx;vy1-=line.vy;vz1-=line.vz;
    if(t3<vx1*vx1+vy1*vy1+vz1*vz1)return 0;
    vx2=p1.x-vx4;vy2=p1.y-vy4;vz2=p1.z-vz4;t1=vx2*vx2+vy2*vy2+vz2*vz2;if(t1<0.001f)return 1;
    t1=njInvertSqrt(t1);vx2*=t1;vy2*=t1;vz2*=t1;
    vx3=p2.x-vx4;vy3=p2.y-vy4;vz3=p2.z-vz4;t2=vx3*vx3+vy3*vy3+vz3*vz3;if(t2<0.001f)return 1;
    t2=njInvertSqrt(t2);vx3*=t2;vy3*=t2;vz3*=t2;
    vx1=p3.x-vx4;vy1=p3.y-vy4;vz1=p3.z-vz4;t3=vx1*vx1+vy1*vy1+vz1*vz1;if(t3<0.001f)return 1;
    t3=njInvertSqrt(t3);vx1*=t3;vy1*=t3;vz1*=t3;
    vx4=p4.x-vx4;vy4=p4.y-vy4;vz4=p4.z-vz4;float t4=vx4*vx4+vy4*vy4+vz4*vz4;if(t4<0.001f)return 1;
    t4=njInvertSqrt(t4);vx4*=t4;vy4*=t4;vz4*=t4;
    t1=vx2*vx3+vy2*vy3+vz2*vz3;if(t1<-1.0f||1.0f<t1)return 0;
    t2=vx3*vx1+vy3*vy1+vz3*vz1;if(t2<-1.0f||1.0f<t2)return 0;
    t3=vx1*vx4+vy1*vy4+vz1*vz4;if(t3<-1.0f||1.0f<t3)return 0;
    t4=vx4*vx2+vy4*vy2+vz4*vz2;if(t4<-1.0f||1.0f<t4)return 0;
    if(std::acos(t1)+std::acos(t2)+std::acos(t3)+std::acos(t4)<6.25f)return 0;
    return 1;
}
// ps2_NaColi.c:646 njCollisionCheckBC
int check_bc(const Box& box,const C& capsule){
    static constexpr unsigned vertex[6][4]={{0,1,2,3},{4,7,6,5},{0,3,7,4},{2,1,5,6},{1,0,4,5},{3,2,6,7}};
    static constexpr V normal[6]={{0,1.0f,0},{0,-1.0f,0},{0,0,1.0f},{0,0,-1.0f},{-1.0f,0,0},{1.0f,0,0}};
    const float x1=capsule.c1.x,x2=capsule.c2.x,y1=capsule.c1.y,z1=capsule.c1.z,y2=capsule.c2.y,z2=capsule.c2.z,r=capsule.r;
    float minx,maxx,miny,maxy,minz,maxz;
    if(x1<x2){minx=x1-r;maxx=x2+r;}else{minx=x2-r;maxx=x1+r;}
    if(y1<y2){miny=y1-r;maxy=y2+r;}else{miny=y2-r;maxy=y1+r;}
    if(z1<z2){minz=z1-r;maxz=z2+r;}else{minz=z2-r;maxz=z1+r;}
    const V* v=box.v;
    if(maxx<=v[0].x||v[6].x<=minx||maxy<=v[6].y||v[0].y<=miny||maxz<=v[6].z||v[0].z<=minz)return 0;
    auto inside=[&](float x,float y,float z){
        return (v[0].x-r<x&&x<v[6].x+r&&v[6].y<=y&&y<=v[0].y&&v[6].z<=z&&z<=v[0].z)
            ||(v[0].x<=x&&x<=v[6].x&&v[6].y-r<y&&y<v[0].y+r&&v[6].z<=z&&z<=v[0].z)
            ||(v[0].x<=x&&x<=v[6].x&&v[6].y<=y&&y<=v[0].y&&v[6].z-r<z&&z<v[0].z+r);};
    if(inside(x1,y1,z1))return 1;
    if(inside(x2,y2,z2))return 1;
    // The twelve edges, in the original order of c1/c2 replacements.
    static constexpr unsigned edges[12][2]={{0,3},{0,4},{0,1},{5,1},{5,4},{5,6},{2,6},{2,1},{2,3},{7,3},{7,4},{7,6}};
    for(const auto& e:edges){const C edge{v[e[0]],v[e[1]],0};if(check_cc(capsule,edge))return 1;}
    const L line{x1,y1,z1,x2-x1,y2-y1,z2-z1};
    for(unsigned i=0;i<6;++i)if(check_plane4_and_line(v[vertex[i][0]],v[vertex[i][1]],v[vertex[i][2]],v[vertex[i][3]],normal[i],line))return 1;
    return 0;
}
// ps2_NaColi.c:125 njDistanceP2PL
float distance_p2pl(const V& p,const Plane& pl,V& cp){
    float vx=pl.vx,vy=pl.vy,vz=pl.vz,length=njInvertSqrt(vx*vx+vy*vy+vz*vz);vx*=length;vy*=length;vz*=length;
    length=vx*(p.x-pl.px)+vy*(p.y-pl.py)+vz*(p.z-pl.pz);
    cp={p.x-vx*length,p.y-vy*length,p.z-vz*length};return std::fabs(length);
}
// ps2_NaColi.c:246 njDistanceL2PL (cp != NULL)
float distance_l2pl(const L& l,const Plane& pl,V& cp){
    float a=pl.vx,b=pl.vy,c=pl.vz,t=njInvertSqrt(a*a+b*b+c*c);a*=t;b*=t;c*=t;
    float ll=l.vx,m=l.vy,n=l.vz;t=njInvertSqrt(ll*ll+m*m+n*n);ll*=t;m*=t;n*=t;
    t=a*ll+b*m+c*n;
    if(0.025f<std::fabs(t)){const float x=l.px,y=l.py,z=l.pz;t=-(a*(x-pl.px)+b*(y-pl.py)+c*(z-pl.pz))/t;cp={x+ll*t,y+m*t,z+n*t};return 0;}
    return distance_p2pl({l.px,l.py,l.pz},pl,cp);
}
// ps2_NaColi.c:301 njGetPlaneNormal
void plane_normal(const V p[3],Plane& pl){
    pl.vx=(p[1].y-p[0].y)*(p[2].z-p[1].z)-(p[1].z-p[0].z)*(p[2].y-p[1].y);
    pl.vy=(p[1].z-p[0].z)*(p[2].x-p[1].x)-(p[1].x-p[0].x)*(p[2].z-p[1].z);
    pl.vz=(p[1].x-p[0].x)*(p[2].y-p[1].y)-(p[1].y-p[0].y)*(p[2].x-p[1].x);
}
// ps2_NaColi.c:35 njIsParalellL2PL
int is_paralell_l2pl(const L& l,const Plane& pl){
    float px=pl.vx,py=pl.vy,pz=pl.vz,n=njInvertSqrt(px*px+py*py+pz*pz);px*=n;py*=n;pz*=n;
    float lx=l.vx,ly=l.vy,lz=l.vz;n=njInvertSqrt(lx*lx+ly*ly+lz*lz);lx*=n;ly*=n;lz*=n;
    return std::fabs(px*lx+py*ly+pz*lz)<=0.025f?1:0;
}
// hitchk.c:3699 bhCheckInnerTriangle(hp, pos, 0, 0) as bhCheckC2Wall calls it: the ar == 0 branch.
int inner_triangle(const CollisionWall& hp,float room_height,const V& pos){
    const float h=hp.extent.y?hp.extent.y:room_height;
    if(hp.position.y>pos.y+0.0f||hp.position.y+h<pos.y)return 0;
    const float w=hp.extent.x,d=hp.extent.z;float minx,maxx,minz,maxz;
    if(w>0){minx=hp.position.x;maxx=0.0f+(minx+w);}else{maxx=hp.position.x;minx=(maxx+w)-0.0f;}
    if(d>0){minz=hp.position.z;maxz=0.0f+(minz+d);}else{maxz=hp.position.z;minz=(maxz+d)-0.0f;}
    if(minx>pos.x||maxx<=pos.x||minz>pos.z||maxz<=pos.z)return 0;
    const float abx=std::fabs(d/(w/(pos.x-hp.position.x))),abz=std::fabs((hp.position.z+hp.extent.z)-pos.z);
    return abz<=abx?0:1;
}
}
int check_c2_wall(const std::vector<CollisionWall>& walls,float room_height,const RECVX_CB_CAPSULE& cap){
    const L lp{cap.c1.x,cap.c1.y,cap.c1.z,cap.c2.x-cap.c1.x,cap.c2.y-cap.c1.y,cap.c2.z-cap.c1.z};
    NJS_VECTOR lv{lp.vx,lp.vy,lp.vz};const float nr=njScalor(&lv);
    // Plane-hit test of the type 4 faces: in front of c1 and closer than c2, inside the triangle.
    auto face=[&](const CollisionWall& hp,const V& pt){
        NJS_VECTOR ps{pt.x-lp.px,pt.y-lp.py,pt.z-lp.pz};
        if(njInnerProduct(&lv,&ps)>0){const float ln=njScalor(&ps);if(inner_triangle(hp,room_height,pt)&&ln<nr)return true;}
        return false;};
    for(const auto& hp:walls){
        if(!(hp.flags&1)||(hp.attributes&0x10))continue;
        const float px=hp.position.x,py=hp.position.y,pz=hp.position.z,w=hp.extent.x,d=hp.extent.z;
        float h=hp.extent.y;if(!h)h=room_height;
        switch(hp.type){
        case 0:{const Box box{{{px,py+h,pz+d},{px,py+h,pz},{px+w,py+h,pz},{px+w,py+h,pz+d},{px,py,pz+d},{px,py,pz},{px+w,py,pz},{px+w,py,pz+d}}};
            if(check_bc(box,cap))return 1;break;}
        case 2:{const C cap2{{px,py,pz},{px,py+h,pz},w};if(recvx_cb_check_cc(&cap2,&cap))return 1;break;} // npCollisionCheckCC
        case 4:{V pt;
            Plane pl{px,(py+h)-0.1f,pz,0,1.0f,0};
            if(!distance_l2pl(lp,pl,pt)&&face(hp,pt))return 1;
            pl={px,py,pz,0,-1.0f,0};
            if(!distance_l2pl(lp,pl,pt)&&face(hp,pt))return 1;
            V p[3]={{px,py,pz},{px+w,py,pz+d},{px,py+h,pz}};
            plane_normal(p,pl);pl.px=px+0.5f*w;pl.py=py+0.5f*h;pl.pz=pz+0.5f*d;
            distance_l2pl(lp,pl,pt);if(face(hp,pt))return 1;
            p[0]={px+w,py,pz};p[1]={px,py,pz+d};p[2]={px+w,py+h,pz};
            plane_normal(p,pl);pl.px=px+0.49f*w;pl.py=py+0.5f*h;pl.pz=pz+0.49f*d;
            distance_l2pl(lp,pl,pt);if(face(hp,pt))return 1;
            break;}
        case 7:{const Box box{{{px,py-0.1f,pz+d},{px,py-0.1f,pz},{px+w,py-0.1f,pz},{px+w,py-0.1f,pz+d},{px,py+h,pz+d},{px,py+h,pz},{px+w,py+h,pz},{px+w,py+h,pz+d}}};
            if(check_bc(box,cap))return 1;break;}
        }
    }
    return 0;
}
// pwksub.c:718-1890 bhCheckL2Wall (100% matching source). A 0.01 capsule from lp->p to
// lp->p + lp->v pre-selects each record (njCollisionCheckBC / npCollisionCheckCC); its c2
// moves to every accepted hit, so later records only compete inside the shorter line.
// Each face plane is intersected with the infinite line (njDistanceL2PL), and the point
// counts when it lies inside the record and closer to lp->p than the best one so far.
int check_l2_wall(const std::vector<CollisionWall>& walls,float room_height,const RECVX_CB_LINE& line,unsigned flg,float* len,RECVX_CB_VEC* apos,int* ahtp){
    const L& lp=line;
    auto scalor=[](float x,float y,float z){NJS_VECTOR v{x,y,z};return njScalor(&v);};
    C cap{{lp.px,lp.py,lp.pz},{lp.px+lp.vx,lp.py+lp.vy,lp.pz+lp.vz},0.01f};
    float nr=scalor(lp.vx,lp.vy,lp.vz);
    int hit=0,found=-1;V at{};
    for(std::size_t n=0;n<walls.size();++n){
        const auto& hp=walls[n];if(!(hp.flags&1))continue;
        const float px=hp.position.x,py=hp.position.y,pz=hp.position.z,w=hp.extent.x,d=hp.extent.z;
        float h=hp.extent.y;if(!h)h=room_height;
        // sys->apos = pt; hit++; sys->ahtp = hp; cap.c2 = pt;
        auto take=[&](const V& pt,float ln){nr=ln;at=pt;++hit;found=int(n);cap.c2=pt;};
        auto origin_distance=[&](const V& pt){return scalor(pt.x-lp.px,pt.y-lp.py,pt.z-lp.pz);};
        // Faces of the box records (types 0/1, 6, 7): inside [0,w]x[0,h]x[0,d] of the
        // record; type 7 carries a negative h and tests ps.y <= 0 && ps.y >= h.
        auto box_face=[&](const Plane& pl,bool downward){V pt;if(distance_l2pl(lp,pl,pt))return;
            const float ln=origin_distance(pt);const float x=pt.x-px,y=pt.y-py,z=pt.z-pz;
            if(x>=0&&x<=w&&(downward?(y<=0&&y>=h):(y>=0&&y<=h))&&z>=0&&z<=d&&ln<nr)take(pt,ln);};
        // Faces of the triangle prisms (types 4/5): bhCheckInnerTriangle(hp, &pt, 0, 0).
        auto prism_face=[&](const Plane& pl){V pt;if(distance_l2pl(lp,pl,pt))return;
            const float ln=origin_distance(pt);if(inner_triangle(hp,room_height,pt)&&ln<nr)take(pt,ln);};
        const Box standard{{{px,py+h,pz+d},{px,py+h,pz},{px+w,py+h,pz},{px+w,py+h,pz+d},{px,py,pz+d},{px,py,pz},{px+w,py,pz},{px+w,py,pz+d}}};
        switch(hp.type){
        case 0:case 1:
            if(!(hp.type&1)||!(flg&0x400)){
                if(check_bc(standard,cap)){
                    box_face({px,(py+h)-0.1f,pz,0,1.0f,0},false);
                    box_face({px,0.1f+py,pz,0,-1.0f,0},false);
                    box_face({0.1f+px,py,pz,-1.0f,0,0},false);
                    box_face({(px+w)-0.1f,py,pz,1.0f,0,0},false);
                    box_face({px,py,0.1f+pz,0,0,-1.0f},false);
                    box_face({px,py,(pz+d)-0.1f,0,0,1.0f},false);
                }
            }
            break;
        case 2:case 3:
            if(!(hp.type&1)||!(flg&0x400)){
                const C cap2{{px,py,pz},{px,py+h,pz},w};
                if(recvx_cb_check_cc(&cap2,&cap)){
                    // Top and bottom discs, only when the line is not parallel to them.
                    auto disc=[&](const Plane& pl){V pt;if(distance_l2pl(lp,pl,pt))return;
                        const float ln=origin_distance(pt),lr=scalor(pt.x-px,0,pt.z-pz),y=pt.y-py;
                        if(lr<w&&ln<nr&&y>=0&&y<=h)take(pt,ln);};
                    const Plane top{px,(py+h)-0.1f,pz,0,1.0f,0};
                    if(!is_paralell_l2pl(lp,top)){disc(top);disc({px,0.1f+py,pz,0,-1.0f,0});}
                    // Mantle: the plane through the axis point facing lp->p, its hit pulled
                    // back onto the radius.
                    const L cln{px,py,pz,0,h,0};V pt,ps;
                    recvx_cb_distance_l2l(&lp,&cln,&pt,&ps);
                    if(scalor(pt.x-px,0,pt.z-pz)<w){
                        NJS_VECTOR u{lp.px-px,0,lp.pz-pz};njUnitVector(&u);
                        const Plane side{px+u.x*w,py,pz+u.z*w,u.x,0,u.z};
                        if(!distance_l2pl(lp,side,pt)){
                            NJS_VECTOR r{pt.x-px,0,pt.z-pz};njUnitVector(&r);
                            pt.x=px+r.x*w;pt.z=pz+r.z*w;
                            const float ln=origin_distance(pt),y=pt.y-py;
                            if(ln<nr&&y>=0&&y<=h)take(pt,ln);
                        }
                    }
                }
            }
            break;
        case 4:case 5:
            if(!(hp.type&1)||!(flg&0x400)){
                Box box;
                switch(hp.id){
                case 0:box=standard;break;
                case 1:box={{{px,py+h,pz},{px,py+h,pz+d},{px+w,py+h,pz+d},{px+w,py+h,pz},{px,py,pz},{px,py,pz+d},{px+w,py,pz+d},{px+w,py,pz}}};break;
                case 2:box={{{px+w,py+h,pz+d},{px+w,py+h,pz},{px,py+h,pz},{px,py+h,pz+d},{px+w,py,pz+d},{px+w,py,pz},{px,py,pz},{px,py,pz+d}}};break;
                case 3:box={{{px+w,py+h,pz},{px+w,py+h,pz+d},{px,py+h,pz+d},{px,py+h,pz},{px+w,py,pz},{px+w,py,pz+d},{px,py,pz+d},{px,py,pz}}};break;
                // id > 3 leaves the box of an earlier record in the original (no room
                // table carries one; types 4..6 use id 0..3 only).
                default:continue;
                }
                if(check_bc(box,cap)){
                    prism_face({px,(py+h)-0.1f,pz,0,1.0f,0});
                    prism_face({px,0.1f+py,pz,0,-1.0f,0});
                    prism_face({px,py,d>=0?0.1f+pz:pz-0.1f,0,0,1.0f});
                    prism_face({w>=0?0.1f+px:px-0.1f,py,pz,1.0f,0,0});
                    NJS_VECTOR n=(hp.id==0||hp.id==3)?NJS_VECTOR{d,0,w}:NJS_VECTOR{-d,0,-w};
                    njUnitVector(&n);
                    Plane hyp{px+w,py,pz,n.x,0,n.z};
                    hyp.px-=0.1f*hyp.vx;hyp.pz-=0.1f*hyp.vz;
                    prism_face(hyp);
                }
            }
            break;
        case 6:
            if(check_bc(standard,cap)){
                NJS_VECTOR n;Plane pl{};
                switch(hp.id){
                case 0:pl.px=px;pl.py=py;pl.pz=pz;n={-h,w,0};break;
                case 1:pl.px=px;pl.py=py;pl.pz=pz+d;n={0,d,h};break;
                case 2:pl.px=px;pl.py=py;pl.pz=pz;n={0,d,-h};break;
                case 3:pl.px=px+w;pl.py=py;pl.pz=pz;n={h,w,0};break;
                default:continue; // see case 4/5
                }
                njUnitVector(&n);pl.vx=n.x;pl.vy=n.y;pl.vz=n.z;
                box_face(pl,false);
            }
            break;
        case 7:{
            const Box slab{{{px,py-0.1f,pz+d},{px,py-0.1f,pz},{px+w,py-0.1f,pz},{px+w,py-0.1f,pz+d},{px,py+h,pz+d},{px,py+h,pz},{px+w,py+h,pz},{px+w,py+h,pz+d}}};
            if(check_bc(slab,cap)){
                box_face({px,py-0.1f,pz,0,1.0f,0},true);
                box_face({px,0.1f+(py+h),pz,0,-1.0f,0},true);
                box_face({0.1f+px,py,pz,-1.0f,0,0},true);
                box_face({(px+w)-0.1f,py,pz,1.0f,0,0},true);
                box_face({px,py,0.1f+pz,0,0,-1.0f},true);
                box_face({px,py,(pz+d)-0.1f,0,0,1.0f},true);
            }
            break;}
        }
    }
    if(len)*len=nr;
    if(hit==0){if(apos)*apos={lp.px+lp.vx,lp.py+lp.vy,lp.pz+lp.vz};if(ahtp)*ahtp=-1;return 0;}
    if(apos)*apos=at;if(ahtp)*ahtp=found;
    return 1;
}
int check_collision_4_sound(const std::vector<CollisionWall>& walls,float room_height,unsigned room_flags,NJS_POINT3 camera,NJS_POINT3 source){
    if(!(room_flags&2))return 0;
    const RECVX_CB_CAPSULE capsule{{camera.x,camera.y,camera.z},{source.x,source.y,source.z},0.1f};
    return check_c2_wall(walls,room_height,capsule)?-0x1C:0;
}
}

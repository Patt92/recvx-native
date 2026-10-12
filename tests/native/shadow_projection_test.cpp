#include "shadow_projection.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <cmath>

static void check(bool ok,const char* message){
    if(!ok)throw std::runtime_error(message);
}
int main(int argc,char** argv){
    try{
        check(argc==2,"SYSTEM asset required");
        std::ifstream input(argv[1],std::ios::binary);
        check(bool(input),"cannot open SYSTEM asset");
        const std::vector<std::uint8_t> bytes(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        const auto models=recvx::decode_shadow_geometry(bytes);
        check(models.size()==2,"expected two original models");
        NJS_MATRIX view;
        njUnitMatrix(&view);
        for(unsigned model=0;model<models.size();++model){
            recvx::SceneShadow draw;
            draw.geometry=std::make_shared<const recvx::ShadowGeometry>(models[model]);
            draw.transform={4,0,0,0,0,4,0,0,0,0,4,0,0,0,30,1};
            const auto projected=recvx::project_shadow(draw,view,320,240);
            check(projected.drawable,"in-view original model rejected");
            check(projected.faces.size()==models[model].triangles.size(),
                  "polygon count changed");
            unsigned front=0,back=0;
            for(std::size_t i=0;i<projected.faces.size();++i){
                const auto& face=projected.faces[i];
                if(face.original_normal>0)++front;
                if(face.original_normal<0)++back;
                for(unsigned corner=0;corner<3;++corner){
                    const auto index=models[model].triangles[i][corner];
                    const auto& local=models[model].vertices[index];
                    const float x=local[0]*4;
                    const float y=local[1]*4;
                    const float z=local[2]*4+30;
                    check(std::fabs(face.vertices[corner].x-(2048+x*320/z))<.001f,
                          "X projection or original corner order changed");
                    check(std::fabs(face.vertices[corner].y-(2048+y*240/z))<.001f,
                          "Y projection or original corner order changed");
                }
            }
            check(front>0&&back>0,"closed volume lost one face orientation");
            const auto last=models[model].triangles.back()[2];
            check(std::fabs(projected.last_vertex_z-
                  (models[model].vertices[last][2]*4+30))<.0001f,
                  "fog source is not last polygon's third vertex");

            draw.transform[14]=1;
            check(!recvx::project_shadow(draw,view,320,240).drawable,
                  "near-crossing volume was not wholly rejected");

            draw.transform[14]=30;
            draw.transform[12]=1000;
            check(!recvx::project_shadow(draw,view,320,240).drawable,
                  "original screen-bound rejection missing");
            std::cout<<"PASS model "<<model<<": "<<front<<" front, "
                     <<back<<" back; projection, order and rejection\n";
        }
        // njCnkCoP3:1684 selects the mask pass from the pre-offset fSx/fSy.
        // A triangle whose screen delta is below one float ulp at 2048
        // collapses to zero normal if the viewport offset is added first.
        {
            recvx::ShadowGeometry tiny;
            tiny.vertices={{0,0,0},{1.f/262144.f,0,0},{0,1.f/262144.f,0}};
            tiny.triangles={{0,1,2}};
            recvx::SceneShadow draw;
            draw.geometry=std::make_shared<const recvx::ShadowGeometry>(tiny);
            draw.transform={1,0,0,0,0,1,0,0,0,0,1,0,0,0,10,1};
            const auto projected=recvx::project_shadow(draw,view,320,320);
            check(projected.drawable,"sub-ulp triangle wholly rejected");
            check(projected.faces.size()==1,"sub-ulp triangle lost");
            // Screen delta 2^-13 rounds into the offset: post-offset
            // coordinates are identical, so only the pre-offset normal
            // can keep the face's pass assignment.
            check(projected.faces[0].vertices[1].x==2048.f,
                  "offset no longer absorbs the sub-ulp delta");
            check(projected.faces[0].original_normal
                      ==-(1.f/67108864.f),
                  "normal not computed from pre-offset fSx/fSy");
            std::cout<<"PASS sub-ulp: pre-offset normal retained\n";
        }
        std::cout<<"Projection component only; no pixel acceptance\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}

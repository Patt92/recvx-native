#include "shadow_geometry.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

static void check(bool ok,const char* message){
    if(!ok)throw std::runtime_error(message);
}
int main(int argc,char** argv){
    try{
        check(argc==2,"expected SYSTEM/0001.bin path");
        std::ifstream file(argv[1],std::ios::binary);
        check(bool(file),"cannot open SYSTEM data");
        const std::vector<std::uint8_t> data(
            (std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
        const auto models=recvx::decode_shadow_geometry(data);
        check(models.size()==2,"expected two original shadow models");
        check(models[0].vertices.size()==18&&models[0].triangles.size()==32,
              "ground shadow geometry mismatch");
        check(models[1].vertices.size()==33&&models[1].triangles.size()==62,
              "extended shadow geometry mismatch");
        check(models[0].triangles.front()==std::array<unsigned,3>{0,3,2},
              "ground winding/order changed");
        check(models[1].triangles.front()==std::array<unsigned,3>{16,2,1},
              "extended winding/order changed");
        for(const auto& model:models){
            check(model.radius<0,"original negative model radius lost");
            std::map<std::pair<unsigned,unsigned>,unsigned> edges;
            for(const auto& t:model.triangles)
                for(unsigned i=0;i<3;++i)++edges[{t[i],t[(i+1)%3]}];
            for(const auto& edge:edges){
                const auto reverse=edges.find({edge.first.second,edge.first.first});
                check(edge.second==1&&reverse!=edges.end()&&reverse->second==1,
                      "modifier volume must have opposite paired edges");
            }
        }
        auto rejects=[](const std::vector<std::uint8_t>& bytes){
            try{recvx::decode_shadow_geometry(bytes);}
            catch(const std::runtime_error&){return true;}
            return false;
        };
        check(rejects({}),"empty asset accepted");
        auto truncated=data;
        truncated.resize(0xa1f);
        check(rejects(truncated),"truncated shadow block accepted");
        auto invalid=data;
        invalid[0x7f6]=255;invalid[0x7f7]=255;
        check(rejects(invalid),"invalid triangle index accepted");
        auto wrong_type=data;
        wrong_type[0x7f0]=64;
        check(rejects(wrong_type),"non-modifier polygon accepted");
        std::cout<<"PASS: original models, winding, closed volumes, bounds and rejection tests\n";
        std::cout<<"Component evidence only; F05 runtime/render integration remains open\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"FAIL: "<<error.what()<<"\n";
        return 1;
    }
}

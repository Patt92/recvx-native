#include "file_folder.h"
#include "binary_view.h"
#include "item_asset.h"
#include <stdexcept>
namespace recvx {
namespace original_file_folder {
constexpr bool TRUE=true;
struct NJS_CNK_OBJECT {float pos[3]{};int ang[3]{};NJS_CNK_OBJECT* child=nullptr;NJS_CNK_OBJECT* sibling=nullptr;};
struct NJS_POINT2 {float x=0,y=0;};
struct FV_WORK {int tag=0,filecsr=0,roll=0,j=0,z=0,ang00=0,filenum=0,page=0,pagewait01=0,mode_02=0;unsigned afsmode=0;short koma[16]{},koma2[16]{};};
struct S_WORK {unsigned statusflg=0;int mode=8,testmode=8,maincsr=1,number=0,wait=0,wn=2,subscreenmode=2;};
struct SITEM {struct {NJS_CNK_OBJECT* objP;} mdl;};
struct System {unsigned pad_ps=0,pad_on=0;unsigned* itm=nullptr;const unsigned* ev_flg=nullptr;};
struct Part {struct {float r=.7f,g=.7f,b=.7f;} col;};
static thread_local FV_WORK fvwork;
static thread_local S_WORK swork;
static thread_local System system;static thread_local System* sys=&system;
static thread_local SITEM sitem;
static thread_local Part parts_10b[16];
static thread_local unsigned sound=~0u;
static thread_local bool reader=false;
static void CallSystemSe(int,unsigned n){sound=n;}
static void NameChangeSet(S_WORK*){}
static void bhDispItemName(NJS_POINT2*,int,int,int,float){} // drawn by GameUi
static void FileFlagInit(){}
static void StatusToFileView(){reader=true;} // owner runs the sprite transition
static int bhCkFlg(const unsigned* flags,unsigned bit){return (flags[bit/32]&(0x80000000u>>(bit%32)))!=0;}
#include "file_folder.inc"
}
struct FileFolder::State {
    original_file_folder::FV_WORK work;
    original_file_folder::S_WORK status;
    std::array<original_file_folder::NJS_CNK_OBJECT,4> nodes{};
    bool reader=false,navigation=false;
    unsigned sound=~0u,clock=0;
    std::array<float,2> arrow{{.7f,.7f}};
    State(){nodes[0].child=&nodes[1];nodes[1].sibling=&nodes[2];nodes[2].sibling=&nodes[3];original_file_folder::FileSyu(&nodes[1]);}
    void enter(Inventory& inv,unsigned pad){using namespace original_file_folder;fvwork=work;swork=status;system={pad,pad,inv.items.data()};sitem.mdl.objP=nodes.data();for(unsigned i=0;i<2;++i)parts_10b[13+i].col.r=parts_10b[13+i].col.g=parts_10b[13+i].col.b=arrow[i];original_file_folder::sound=~0u;original_file_folder::reader=false;}
    void leave(){using namespace original_file_folder;work=fvwork;status=swork;for(unsigned i=0;i<2;++i)arrow[i]=parts_10b[13+i].col.r;sound=original_file_folder::sound;reader=original_file_folder::reader;navigation=status.mode==2;}
};
FileFolder::FileFolder():state_(std::make_shared<State>()){}
unsigned file_page_reduction(const std::array<std::uint32_t,32>& flags){original_file_folder::system.ev_flg=flags.data();return unsigned(original_file_folder::PlayPageCheck());}
void FileFolder::open(Inventory& inv,bool bonus,bool automatic){state_=std::make_shared<State>();inv.items[383]|=1;if(bonus)inv.items[383]|=0x800;auto& s=*state_;if(automatic)s.status.statusflg|=0x40000000;s.enter(inv,0);original_file_folder::SearchTag(1);s.leave();}
void FileFolder::step(Inventory& inv,unsigned pad){auto& s=*state_;s.enter(inv,pad);if(s.status.testmode==9)original_file_folder::SelectTag();else{original_file_folder::SelectFile();if(original_file_folder::fvwork.z==0)original_file_folder::SearchTag(1);}s.leave();++s.clock;}
void FileFolder::return_from_reader(){auto& s=*state_;s.work.afsmode=(s.work.afsmode&~0xB80u)|0x20;s.status.statusflg&=~0x40u;s.reader=false;s.status.testmode=9;}
unsigned FileFolder::take_sound(){auto n=state_->sound;state_->sound=~0u;return n;}
bool FileFolder::take_reader(){bool n=state_->reader;state_->reader=false;return n;}
bool FileFolder::take_navigation(){bool n=state_->navigation;state_->navigation=false;return n;}
bool FileFolder::moving()const{return state_->work.z!=0;}
bool FileFolder::registers()const{return state_->status.testmode==9;}
bool FileFolder::arrows()const{return !(state_->status.statusflg&0x20000000);}
float FileFolder::arrow_brightness(unsigned index)const{return state_->arrow.at(index);}
unsigned FileFolder::folder()const{return unsigned(state_->work.filecsr);}
unsigned FileFolder::file()const{return unsigned(state_->work.tag+state_->work.filecsr*8);}
unsigned FileFolder::clock()const{return state_->clock;}
Scene FileFolder::scene(const std::vector<std::uint8_t>& packet,const Inventory& inv)const{
    const auto& s=*state_;BinaryView b(packet);const auto base=b.relative(4,b.u16(8),0);const auto count=b.u16(10);const auto nodes=b.relative(base,b.u32(16),count*52);
    if(count!=4)throw std::runtime_error("file folder hierarchy does not contain three folders");
    ModelGeometryOptions options;options.pose.resize(4);options.evalflags.resize(4,8);
    const unsigned groups[3]={0,2,1};std::array<NJS_POINT3,3> positions{};
    for(unsigned n=0;n<3;++n){const auto p=nodes+(n+1)*52;NJS_POINT3 pos{b.f32(p+8),b.f32(p+12),b.f32(p+16)};NJS_MATRIX m;njUnitMatrix(&m);njRotateX(&m,s.nodes[n+1].ang[0]);njRotateY(&m,s.nodes[n+1].ang[1]);njRotateZ(&m,s.nodes[n+1].ang[2]);njCalcPoint(&m,&pos,&positions[n]);
        options.pose[n+1]={positions[n],{0,groups[n]==folder()?s.work.ang00:0,0}};
        options.evalflags[n+1]=(s.work.afsmode&0x20)&&groups[n]!=folder()?8:0;
    }
    auto result=decode_item_asset(packet,options);
    // MakeTag's exact four vertices, ownership and selected-register colors.
    for(unsigned n=0;n<3;++n){if(options.evalflags[n+1]&8)continue;NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,positions[n].x,positions[n].y,positions[n].z);if(groups[n]==folder())njRotateY(&m,s.work.ang00);
        for(unsigned tag=0;tag<8;++tag){const unsigned index=groups[n]*8+tag;if(!(inv.items[383]&(1u<<index)))continue;const float y=1.2f-.35f*tag,z=.04f-.02f*tag;std::array<NJS_POINT3,4> p{{{0,y,z},{0,y+.2f,z},{1.2f,y+.2f,z},{1.2f,y,z}}};for(auto& v:p)njCalcPoint(&m,&v,&v);const unsigned c=index==file()?192:128;
            for(auto ids:{std::array<unsigned,3>{0,1,2},std::array<unsigned,3>{0,2,3}}){SceneTriangle t;t.color=0xff000000u|(c<<16)|(c<<8)|c;t.object=std::uint16_t(n+1);for(unsigned i=0;i<3;++i)t.vertices[i].position=p[ids[i]];result.triangles.push_back(t);}
        }
    }
    return result;
}
}

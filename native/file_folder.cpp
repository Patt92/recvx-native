#include "file_folder.h"
#include "binary_view.h"
#include "item_asset.h"
#include <stdexcept>
namespace recvx {
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Die Dateiordner-Routinen aus
// fileview.c / itemview.c (SearchTag, SelectFile, SelectTag, FileSyu,
// PlayPageCheck mit der Fortschrittsreihenfolge des Handbuchs) kamen aus
// file_folder.inc, erzeugt von tools/generate_file_folder.py. Generator und
// Include sind Quarantäne (archived/FORBIDDEN_DONTUSE/) und dürfen weder
// gelesen noch ausgeführt werden. Mit den Routinen entfällt auch die
// Host-Brücke, die nur für sie existierte (die thread_local-Spiegel von
// FV_WORK/S_WORK/SITEM, CallSystemSe, NameChangeSet, StatusToFileView,
// bhCkFlg, FileFlagInit und der enter/leave-Austausch).
//
// Zu leisten: Ordnerauswahl, Drehung, Registercursor, der Übergang in den
// Leser und die Seitenreduktion des Handbuchs als eigenen nativen Mechanismus
// bestimmen — nicht über einen Generator auf die Originalquellen.
//
// Solange das offen ist:
// - step() bewegt nichts: kein Cursor, keine Ordnerdrehung (ang00 bleibt 0),
//   kein Moduswechsel, kein SE und keine Leseranforderung. Nur der reine
//   Host-Zähler clock() läuft weiter, damit die aufrufende UI ihren eigenen
//   Takt behält.
// - file_page_reduction liefert 0, weil PlayPageCheck fehlt. Das ist kein
//   geratener Seitenstand, sondern "keine Seite bestimmbar".
// - Die Datenstrukturen FV_WORK/S_WORK und die Knotenwinkel bleiben als
//   Zustand erhalten, damit scene() die drei Ordner weiter aus den
//   Originaldaten aufbaut — allerdings in Ruhelage, weil FileSyu die
//   Startwinkel nicht mehr setzt.
//
// Sichtbare Folge: Der Dateiordner lässt sich nicht bedienen. Die drei Ordner
// stehen unbewegt in ihrer Ausgangslage, Register lassen sich nicht wählen,
// kein Dokument öffnet sich, und das Handbuch meldet keine Seiten.
namespace original_file_folder {
// Feldnamengleiche Teilmengen der Originalwerke; von scene() und den
// Abfragen unten gelesen.
struct NJS_CNK_OBJECT {float pos[3]{};int ang[3]{};};
struct FV_WORK {int tag=0,filecsr=0,roll=0,j=0,z=0,ang00=0,filenum=0,page=0,pagewait01=0,mode_02=0;unsigned afsmode=0;short koma[16]{},koma2[16]{};};
struct S_WORK {unsigned statusflg=0;int mode=8,testmode=8,maincsr=1,number=0,wait=0,wn=2,subscreenmode=2;};
}
struct FileFolder::State {
    original_file_folder::FV_WORK work;
    original_file_folder::S_WORK status;
    std::array<original_file_folder::NJS_CNK_OBJECT,4> nodes{};
    bool reader=false,navigation=false;
    unsigned sound=~0u,clock=0;
    std::array<float,2> arrow{{.7f,.7f}};
};
FileFolder::FileFolder():state_(std::make_shared<State>()){}
bool FileFolder::automatic()const{return (state_->status.statusflg&0x40000000u)!=0;}
unsigned file_page_reduction(const std::array<std::uint32_t,32>& flags){(void)flags;return 0;}
void FileFolder::open(Inventory& inv,bool bonus,bool automatic){
    state_=std::make_shared<State>();inv.items[383]|=1;if(bonus)inv.items[383]|=0x800;
    if(automatic)state_->status.statusflg|=0x40000000;
}
void FileFolder::step(Inventory& inv,unsigned pad){(void)inv;(void)pad;++state_->clock;}
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

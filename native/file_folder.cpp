#include "file_folder.h"
#include "binary_view.h"
#include "item_asset.h"
#include <cstdint>
#include <stdexcept>
namespace recvx {
// Dateiordner und Seitenreduktion, eigenständig nativ (D03).
// Semantikreferenz: fileview.c (SearchTag, SelectFile, SelectTag,
// PlayPageCheck) und itemview.c (FileSyu, MakeTag). Kein Funktionskörper
// übernommen, kein Originalkern. Die Werke unten tragen die Originalfeldnamen,
// damit die Zustandsübergänge nachvollziehbar bleiben.
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
unsigned file_page_reduction(const std::array<std::uint32_t,32>& flags){
    // PlayPageCheck: der Fortschritt im Szenario kürzt das Handbuch von hinten.
    // Die Reihenfolge ist die des Originals, der erste gesetzte Marker gewinnt.
    auto set=[&](unsigned bit){return (flags.at(bit/32)&(0x80000000u>>(bit%32)))!=0;};
    if(set(0x199))return 0;
    if(set(0x2b))return 1;
    if(set(0x129))return 2;
    if(set(0xe0))return 4;
    if(set(0x96))return 6;
    return 7;
}
void FileFolder::open(Inventory& inv,bool bonus,bool automatic){
    // MakeTag: Datei 0 (Handbuch) ist immer vorhanden, ssd_flg 0x1000 aus Raum
    // 551 zeigt zusätzlich Datei 11. FileSyu dreht die drei Ordner in ihre
    // Startlage, und der gewählte Ordner steht nach vorn (SelectFile z 1).
    state_=std::make_shared<State>();inv.items[383]|=1;if(bonus)inv.items[383]|=0x800;
    if(automatic)state_->status.statusflg|=0x40000000;
    auto& s=*state_;
    for(unsigned n=1;n<4;++n){s.nodes[n].ang[0]=32768;s.nodes[n].ang[1]=32768;}
    for(unsigned n=1;n<4;++n)s.nodes[n].ang[1]=std::int16_t(s.work.filecsr*21845-32768);
    s.work.afsmode|=0x20;
    s.status.testmode=8;
    search_tag(inv,1);
}
bool FileFolder::search_tag(const Inventory& inv,int direction){
    // SearchTag: das nächste belegte Register des aktuellen Ordners suchen.
    auto& s=*state_;const int start=s.work.tag;
    for(;;){
        if(inv.items[383]&(1u<<unsigned(s.work.tag+s.work.filecsr*8)))return true;
        s.work.tag=(s.work.tag+direction)&0x7;
        if(s.work.tag==start)return false;
    }
}
void FileFolder::step(Inventory& inv,unsigned pad){
    auto& s=*state_;++s.clock;
    const bool decide=(pad&0x800)||(s.status.statusflg&0x40000000u);
    const bool cancel=(pad&0x1000)!=0;
    s.arrow={{.7f,.7f}};
    if(s.status.testmode==8){
        // SelectFile: links/rechts drehen den Ordnerstapel, Bestätigen fährt den
        // gewählten Ordner heraus und wechselt in die Registerauswahl,
        // Abbrechen geht zurück in die Statusleiste.
        if(s.work.z==0&&s.work.roll==0){
            if(!decide&&!cancel){
                if(pad&0x4){s.arrow[1]=1.f;s.work.tag=0;if(++s.work.filecsr>2)s.work.filecsr=0;s.work.roll=1;for(auto& k:s.work.koma)k=2730;s.work.z=1;s.sound=6;}
                else if(pad&0x8){s.arrow[0]=1.f;s.work.tag=0;if(--s.work.filecsr<0)s.work.filecsr=2;s.work.roll=1;for(auto& k:s.work.koma)k=-2730;s.work.z=1;s.sound=6;}
            }else if(decide){
                if(search_tag(inv,1)){s.work.z=2;s.sound=3;for(unsigned i=0;i<6;++i)s.work.koma2[i]=1820;s.status.statusflg|=0x20000000;}
                else s.status.statusflg&=~0x20000000u;
            }else{s.navigation=true;s.status.statusflg|=0x20000;s.sound=0;}
        }
        switch(s.work.z){
        case 1:
            // Die zwölf Teilschritte der Stapeldrehung; danach stehen alle drei
            // Ordner auf dem Winkel des gewählten Faches.
            if(s.work.j<16&&s.work.koma[s.work.j]!=0){for(unsigned n=1;n<4;++n)s.nodes[n].ang[1]+=s.work.koma[s.work.j];++s.work.j;break;}
            for(unsigned n=1;n<4;++n)s.nodes[n].ang[1]=std::int16_t(s.work.filecsr*21845-32768);
            s.work.roll=0;s.work.z=0;s.work.j=0;for(auto& k:s.work.koma)k=0;break;
        case 2:
            // Der gewählte Ordner schwenkt heraus, die beiden anderen weichen.
            if(s.work.j<16&&s.work.koma2[s.work.j]!=0){rotate_selected(s.work.koma2[s.work.j]);++s.work.j;break;}
            s.work.j=0;for(unsigned i=0;i<6;++i)s.work.koma2[i]=1820;s.work.z=3;s.work.afsmode|=0x20;break;
        case 3:
            // Das Register dreht sich zum Leser hin; danach steht der Cursor auf
            // dem ersten belegten Register.
            if(s.work.j<16&&s.work.koma2[s.work.j]!=0){s.work.ang00-=s.work.koma2[s.work.j];++s.work.j;break;}
            s.work.j=0;for(auto& k:s.work.koma2)k=0;s.work.z=0;s.status.testmode=9;search_tag(inv,1);break;
        default:break;
        }
        return;
    }
    // SelectTag: hoch/runter wählt ein belegtes Register, Bestätigen öffnet den
    // Leser, Abbrechen dreht den Ordner zurück in den Stapel.
    if(s.work.z==0&&!(s.work.afsmode&0x40)){
        if(!decide&&!cancel){
            const int direction=(pad&0x1)?-1:(pad&0x2)?1:0;
            if(direction){
                const int start=s.work.tag;
                for(;;){
                    s.work.tag=(s.work.tag+direction)&0x7;
                    if(s.work.tag==start)break;
                    if(inv.items[383]&(1u<<unsigned(s.work.tag+s.work.filecsr*8))){s.sound=3;break;}
                }
            }
        }else if(decide){
            if(inv.items[383]&(1u<<unsigned(s.work.tag+s.work.filecsr*8))){
                s.sound=3;s.reader=true;s.work.afsmode|=0x60;s.status.statusflg|=0x40;s.work.page=0;s.work.pagewait01=0;
            }
        }else{s.sound=0;for(unsigned i=0;i<6;++i)s.work.koma2[i]=1820;s.work.z=1;}
        return;
    }
    if(s.work.z==1){
        if(s.work.j<16&&s.work.koma2[s.work.j]!=0){s.work.ang00+=s.work.koma2[s.work.j];++s.work.j;return;}
        s.work.j=0;for(unsigned i=0;i<6;++i)s.work.koma2[i]=1820;s.work.z=2;s.work.afsmode&=~0x20u;return;
    }
    if(s.work.z==2){
        if(s.work.j<16&&s.work.koma2[s.work.j]!=0){rotate_selected(-s.work.koma2[s.work.j]);++s.work.j;return;}
        s.work.j=0;for(auto& k:s.work.koma2)k=0;s.status.testmode=8;s.work.z=0;return;
    }
}
void FileFolder::rotate_selected(int step){
    // SelectFile z 2 / SelectTag z 2: je Fach dreht ein anderes Paar der drei
    // Ordnerknoten gegeneinander.
    auto& s=*state_;
    switch(s.work.filecsr){
    case 0:s.nodes[2].ang[1]+=step;s.nodes[3].ang[1]-=step;break;
    case 1:s.nodes[1].ang[1]+=step;s.nodes[2].ang[1]-=step;break;
    case 2:s.nodes[1].ang[1]-=step;s.nodes[3].ang[1]+=step;break;
    default:break;
    }
}
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

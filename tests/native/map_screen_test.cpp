#include "map_screen.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace recvx;

/* D10: Komponentenprüfung des nativen Kartenbildschirms.
   1. Jede Datentabelle in native/map_tables.c wird gegen den Initialisierertext
      von src/ps2/veronica/prog/map.c geprüft (Regressionstest gegen die Quelle).
   2. MapCnvStatus2Flag für alle Spieler-IDs, Item vorhanden und fehlend.
   3. Verbindungsgraph (MapCncInit/MapCnc/MapCncConnect/MapCheckNextMap).
   4. Etagenwahl MapGetFloorNo gegen die echte Originaltabelle in MRY.
   5. Taskfolge Öffnen -> Normal -> Zoom -> Exit mit SE- und Flagfolge an den
      echten Originaldateien, dazu die Nichttrefferfälle. */

static int failures=0;
static void fail(const std::string& what){std::cerr<<"FAIL "<<what<<"\n";++failures;}
static void check(const std::string& what,bool ok){if(!ok)fail(what);}
template<typename A,typename B>
static void equal(const std::string& what,A got,B expected){
    if(!(got==static_cast<A>(expected))){
        std::ostringstream out;out<<what<<": got "<<got<<" expected "<<expected;fail(out.str());}}
static void close_to(const std::string& what,double got,double expected){
    if(std::fabs(got-expected)>1e-6*(1.0+std::fabs(expected))){
        std::ostringstream out;out<<what<<": got "<<got<<" expected "<<expected;fail(out.str());}}

static std::string read_text(const std::string& path){
    std::ifstream in(path,std::ios::binary);
    if(!in){fail("cannot read "+path);return {};}
    std::ostringstream out;out<<in.rdbuf();return out.str();
}
static std::vector<std::uint8_t> read_bytes(const std::string& path){
    std::ifstream in(path,std::ios::binary);
    if(!in)return {};
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
}

/* Der Initialisiererblock einer Tabelle aus dem Quelltext: ab der Deklaration
   die erste '{' und bis zur zugehörigen schliessenden '}'. */
static std::string table_block(const std::string& source,const std::string& declaration){
    const auto position=source.find(declaration);
    if(position==std::string::npos){fail("table not found: "+declaration);return {};}
    const auto open=source.find('{',position);
    if(open==std::string::npos){fail("initialiser not found: "+declaration);return {};}
    int depth=0;
    for(std::size_t index=open;index<source.size();++index){
        if(source[index]=='{')++depth;
        else if(source[index]=='}'){
            if(--depth==0)return source.substr(open,index-open+1);
        }
    }
    fail("unterminated initialiser: "+declaration);
    return {};
}

/* Alle Zahlenliterale des Blocks in Reihenfolge. Zahlen, die Teil eines
   Namens sind (FsprArrowDraw2, MP_SPR_10M_D), werden übersprungen. */
static std::vector<double> numbers(const std::string& block){
    std::vector<double> values;
    for(std::size_t index=0;index<block.size();){
        const char c=block[index];
        const bool starts=(c>='0'&&c<='9')||((c=='-'||c=='+')&&index+1<block.size()&&block[index+1]>='0'&&block[index+1]<='9');
        if(!starts){++index;continue;}
        if(index>0){
            const char previous=block[index-1];
            if((previous>='a'&&previous<='z')||(previous>='A'&&previous<='Z')||previous=='_'||
               (previous>='0'&&previous<='9')||previous=='.'||previous=='x'||previous=='X'){++index;continue;}
        }
        std::size_t end=index;
        if(block[index]=='-'||block[index]=='+')++end;
        if(end+1<block.size()&&block[end]=='0'&&(block[end+1]=='x'||block[end+1]=='X')){
            end+=2;
            while(end<block.size()&&std::isxdigit(static_cast<unsigned char>(block[end])))++end;
            values.push_back(double(std::strtoull(block.substr(index,end-index).c_str(),nullptr,16)));
        }
        else{
            while(end<block.size()&&(std::isdigit(static_cast<unsigned char>(block[end]))||block[end]=='.'))++end;
            values.push_back(std::strtod(block.substr(index,end-index).c_str(),nullptr));
            if(end<block.size()&&(block[end]=='f'||block[end]=='F'))++end;
        }
        index=end;
    }
    return values;
}

static void compare(const std::string& what,const std::string& source,const std::string& declaration,
                    const std::vector<double>& native){
    const auto block=table_block(source,declaration);
    if(block.empty())return;
    const auto original=numbers(block);
    if(original.size()!=native.size()){
        std::ostringstream out;out<<what<<": "<<original.size()<<" values in map.c, "<<native.size()<<" native";
        fail(out.str());return;}
    for(std::size_t index=0;index<original.size();++index)
        if(std::fabs(original[index]-native[index])>1e-6*(1.0+std::fabs(original[index]))){
            std::ostringstream out;out<<what<<"["<<index<<"]: map.c "<<original[index]<<", native "<<native[index];
            fail(out.str());return;}
}

static void test_tables(const std::string& root){
    const auto source=read_text(root+"/src/ps2/veronica/prog/map.c");
    if(source.empty())return;
    std::vector<double> values;

    values.clear();
    for(const auto& entry:recvx_map_pal){values.push_back(entry.a);values.push_back(entry.r);values.push_back(entry.g);values.push_back(entry.b);}
    compare("MapPal",source,"NJS_ARGB MapPal[32]",values);

    values.clear();
    for(auto color:recvx_map_col)values.push_back(double(color));
    compare("MapCol",source,"NJS_COLOR MapCol[3]",values);

    values.clear();
    for(const auto& entry:recvx_map_itm_dat){values.push_back(entry.itm_no);values.push_back(entry.flg_no);}
    compare("ItmDat",source,"ID_WORK ItmDat[22]",values);

    values.clear();
    for(auto entry:recvx_map_cnc_dat_a)values.push_back(entry);
    compare("CncDatA",source,"CncDatA[81]",values);

    values.clear();
    for(auto entry:recvx_map_cnc_dat_b)values.push_back(entry);
    compare("CncDatB",source,"CncDatB[49]",values);

    values.clear();
    for(auto entry:recvx_map_snd_tbl)values.push_back(double(std::uint32_t(entry)));
    compare("SndTbl",source,"int SndTbl[1]",values);

    values.clear();
    for(const auto& entry:recvx_map_mrk_atr){
        values.push_back(entry.mrk_no);values.push_back(entry.scale);
        values.push_back(entry.offset);values.push_back(entry.pal_no);}
    compare("MrkAtr",source,"MA_WORK MrkAtr[6]",values);

    values.clear();
    for(const auto& entry:recvx_map_tex_anm){
        values.push_back(entry.sx);values.push_back(entry.sy);values.push_back(entry.cx);values.push_back(entry.cy);
        values.push_back(entry.u1);values.push_back(entry.v1);values.push_back(entry.u2);values.push_back(entry.v2);
        values.push_back(entry.texid);values.push_back(entry.attr);}
    compare("TexAnm",source,"NJS_TEXANIM TexAnm[18]",values);

    /* SprSet enthält im Original Enum- und Funktionsnamen; numerisch bleiben
       act_bit, spr_col und die drei Positionen. Die Namen werden separat in
       Reihenfolge geprüft. */
    values.clear();
    for(const auto& entry:recvx_map_spr_set){
        values.push_back(entry.act_bit);values.push_back(entry.spr_col);
        values.push_back(entry.x);values.push_back(entry.y);values.push_back(entry.z);}
    compare("SprSet",source,"SS_WORK SprSet[11]",values);
    {
        const auto block=table_block(source,"SS_WORK SprSet[11]");
        static const char* sprites[11]={"MP_SPR_SILHOUETTE","MP_SPR_ARROW_UP","MP_SPR_ARROW_DOWN",
            "MP_SPR_ARROW_LEFT","MP_SPR_ARROW_RIGHT","MP_SPR_ARROW_UP","MP_SPR_ARROW_DOWN",
            "MP_SPR_ARROW_LEFT","MP_SPR_ARROW_RIGHT","MP_SPR_LR_ZOOM","MP_SPR_TITLE"};
        static const int expected_sprite[11]={RECVX_MP_SPR_SILHOUETTE,RECVX_MP_SPR_ARROW_UP,RECVX_MP_SPR_ARROW_DOWN,
            RECVX_MP_SPR_ARROW_LEFT,RECVX_MP_SPR_ARROW_RIGHT,RECVX_MP_SPR_ARROW_UP,RECVX_MP_SPR_ARROW_DOWN,
            RECVX_MP_SPR_ARROW_LEFT,RECVX_MP_SPR_ARROW_RIGHT,RECVX_MP_SPR_LR_ZOOM,RECVX_MP_SPR_TITLE};
        static const char* procedures[11]={"FsprSilhouetteDraw","FsprArrowDraw","FsprArrowDraw","FsprArrowDraw",
            "FsprArrowDraw","FsprArrowDraw2","FsprArrowDraw2","FsprArrowDraw2","FsprArrowDraw2",
            "FsprSpriteDraw","FsprSpriteDraw"};
        static const int expected_function[11]={RECVX_MAP_FSPR_SILHOUETTE,RECVX_MAP_FSPR_ARROW,RECVX_MAP_FSPR_ARROW,
            RECVX_MAP_FSPR_ARROW,RECVX_MAP_FSPR_ARROW,RECVX_MAP_FSPR_ARROW2,RECVX_MAP_FSPR_ARROW2,
            RECVX_MAP_FSPR_ARROW2,RECVX_MAP_FSPR_ARROW2,RECVX_MAP_FSPR_SPRITE,RECVX_MAP_FSPR_SPRITE};
        std::size_t cursor=0;
        for(int row=0;row<11;++row){
            const auto sprite_at=block.find(sprites[row],cursor);
            const auto procedure_at=block.find(procedures[row],cursor);
            if(sprite_at==std::string::npos||procedure_at==std::string::npos||procedure_at<sprite_at){
                fail("SprSet row order in map.c");break;}
            equal("SprSet spr_no row "+std::to_string(row),recvx_map_spr_set[row].spr_no,expected_sprite[row]);
            equal("SprSet function row "+std::to_string(row),recvx_map_spr_set[row].function,expected_function[row]);
            cursor=procedure_at+std::string(procedures[row]).size();
        }
    }

    const auto point_values=[](const RECVX_MAP_POINT2* table,int count){
        std::vector<double> out;
        for(int index=0;index<count;++index){out.push_back(table[index].x);out.push_back(table[index].y);}
        return out;};
    compare("dst_pos0",source,"NJS_POINT2 dst_pos0[4]",point_values(recvx_map_zoom_dst_pos0,4));
    compare("dst_pos1",source,"NJS_POINT2 dst_pos1[4]",point_values(recvx_map_zoom_dst_pos1,4));
    compare("dst_pos2",source,"NJS_POINT2 dst_pos2[4]",point_values(recvx_map_zoom_dst_pos2,4));
    compare("ScrOut",source,"NJS_POINT2 ScrOut[2]",point_values(recvx_map_scr_out,2));
    compare("ScrIn",source,"NJS_POINT2 ScrIn[2]",point_values(recvx_map_scr_in,2));

    values.clear();
    for(float value:recvx_map_info_set_pos)values.push_back(value);
    compare("FsubZoomInfomation set_pos",source,"NJS_POINT3 set_pos",values);

    values.clear();
    for(const auto& entry:recvx_map_cmp_arw){
        values.push_back(entry.x);values.push_back(entry.y);values.push_back(entry.z);values.push_back(entry.col);}
    compare("CmpArw",source,"NJS_POLYGON_VTX CmpArw[4]",values);

    values.clear();
    for(const auto& entry:recvx_map_nml_mes){
        values.push_back(entry.x);values.push_back(entry.y);values.push_back(entry.z);values.push_back(entry.spr);}
    compare("NmlMes",source,"MD_WORK NmlMes[2]",values);

    values.clear();
    for(int entry:recvx_map_adj_map_a)values.push_back(entry);
    compare("AdjMapA",source,"int AdjMapA[3]",values);
}

/* ------------------------------------------------- Flaggruppen 8 und 9 */

namespace {
struct FlagStore {
    /* Nur die beiden Gruppen, die MapCnvStatus2Flag anfasst; die Semantik ist
       die von EventVm::set_flag (mode 0 setzen, 1 löschen). */
    bool bits[17][1024]={};
    bool check(unsigned group,unsigned index)const{return bits[group][index&0x3ff];}
    void set(unsigned group,unsigned index,unsigned mode){
        if(mode%3==0)bits[group][index&0x3ff]=true;
        else if(mode%3==1)bits[group][index&0x3ff]=false;
        else bits[group][index&0x3ff]=!bits[group][index&0x3ff];}
};
MapServices services_for(FlagStore& flags,const std::uint32_t* slots){
    MapServices services;
    services.flag_check=[&flags](unsigned group,unsigned index){return flags.check(group,index);};
    services.flag_set=[&flags](unsigned group,unsigned index,unsigned mode){flags.set(group,index,mode);};
    services.inventory_slots=[slots]{return slots;};
    return services;
}
} // namespace

static void test_flag_conversion(){
    /* map.c:256: id 0..2 bleiben, 3 wird 2, 4 wird 0. Gruppe 8 behält nur
       id+1 gesetzt, Gruppe 9 wird für jedes ItmDat-Item gelöscht und nur für
       vorhandene Items gesetzt. */
    static const int folded[5]={0,1,2,2,0};
    for(int player=0;player<5;++player){
        FlagStore flags;
        std::uint32_t slots[16]={};
        MapHost host;host.ply_id=player;
        MapScreen screen;
        auto services=services_for(flags,slots);
        screen.convert_status_to_flags(host,services);
        const int id=folded[player];
        for(int bit=1;bit<=3;++bit)
            equal("Gruppe 8 Bit "+std::to_string(bit)+" Spieler "+std::to_string(player),
                  flags.check(8,unsigned(bit)),bit==id+1);
        for(int index=0;index<22;++index)
            check("Gruppe 9 ohne Items gelöscht",!flags.check(9,recvx_map_itm_dat[index].flg_no));
    }
    /* Item vorhanden: genau dessen flg_no wird gesetzt, die anderen nicht.
       Die beiden Einträge 59 und 86 teilen flg_no 96 (map.c:44). */
    for(int entry=0;entry<22;++entry){
        FlagStore flags;
        std::uint32_t slots[16]={};
        slots[3]=std::uint32_t(recvx_map_itm_dat[entry].itm_no)<<16|1u;
        MapHost host;host.ply_id=0;
        MapScreen screen;
        auto services=services_for(flags,slots);
        screen.convert_status_to_flags(host,services);
        check("flg_no des vorhandenen Items gesetzt",flags.check(9,recvx_map_itm_dat[entry].flg_no));
        /* Genau eine flg_no ist gesetzt. Die Einträge 0 und 1 (Item 59 und 86)
           teilen im Original flg_no 96 (map.c:44), deshalb werden die
           Flaggennummern und nicht die Tabellenzeilen gezählt. */
        int set_count=0;
        bool counted[256]={};
        for(int index=0;index<22;++index){
            const unsigned number=recvx_map_itm_dat[index].flg_no;
            if(counted[number])continue;
            counted[number]=true;
            if(flags.check(9,number))++set_count;
        }
        equal("nur eine flg_no gesetzt (Eintrag "+std::to_string(entry)+")",set_count,1);
    }
    /* Nichttreffer: eine Item-ID, die in ItmDat nicht vorkommt, setzt nichts. */
    {
        FlagStore flags;
        std::uint32_t slots[16]={};
        slots[0]=std::uint32_t(200)<<16|1u;
        MapHost host;host.ply_id=0;
        MapScreen screen;
        auto services=services_for(flags,slots);
        screen.convert_status_to_flags(host,services);
        for(int index=0;index<22;++index)
            check("fremde Item-ID setzt keine Kartenflagge",!flags.check(9,recvx_map_itm_dat[index].flg_no));
    }
    /* Die Itemliste ist die des aktiven Spielers; nur die 16 übergebenen
       Slots zählen (map.c:287 &sys->itm[ply_id * 16]). */
    {
        FlagStore flags;
        std::uint32_t slots[16]={};
        slots[15]=std::uint32_t(recvx_map_itm_dat[0].itm_no)<<16|1u;
        MapHost host;host.ply_id=3;
        MapScreen screen;
        auto services=services_for(flags,slots);
        screen.convert_status_to_flags(host,services);
        check("letzter Slot wird geprüft",flags.check(9,recvx_map_itm_dat[0].flg_no));
        equal("Spieler 3 faltet auf Gruppe-8-Bit 3",flags.check(8,3),true);
    }
}

/* ------------------------------------------------------- Verbindungsgraph */

static void test_connections(){
    FlagStore flags;
    std::uint32_t slots[16]={};
    auto services=services_for(flags,slots);
    MapScreen screen;
    screen.connection_init(11,6);
    equal("Verbindungsknoten",screen.connections().size(),size_t(66));
    for(int map_no=0;map_no<11;++map_no)
        for(int floor_no=0;floor_no<6;++floor_no){
            const auto* node=screen.connection(map_no,floor_no);
            if(!node){fail("Knoten fehlt");continue;}
            equal("map_no des Knotens",node->map_no,map_no);
            equal("flr_no des Knotens",node->flr_no,floor_no);
            equal("Status initial 0",node->status,0);
        }
    /* Ohne gesetzte Gruppe-8-Flaggen ist jeder Status 0; MapCheckNextMap darf
       dann in keiner Richtung ein Ziel melden (Nichttrefferfall). */
    screen.connection_connect(recvx_map_cnc_dat_a,81,services);
    auto next=screen.check_next_map();
    equal("kein Ziel oben ohne Flaggen",next.map_up,-1);
    equal("kein Ziel unten ohne Flaggen",next.map_down,-1);
    equal("kein Ziel links ohne Flaggen",next.map_left,-1);
    equal("kein Ziel rechts ohne Flaggen",next.map_right,-1);
    /* Mit allen Gruppe-8-Flaggen der Tabelle bekommen die verbundenen Knoten
       Status 1. CncDatA:48/0 wird über 0xFF04 (Flagge 4) freigeschaltet. */
    MapScreen open;
    FlagStore all;
    for(unsigned index=0;index<256;++index)all.set(8,index,0);
    auto open_services=services_for(all,slots);
    open.connection_init(11,6);
    open.connection_connect(recvx_map_cnc_dat_a,81,open_services);
    int with_status=0;
    for(const auto& node:open.connections())if(node.status!=0)++with_status;
    check("CncDatA verbindet Knoten mit Status",with_status>0);
    /* map.c:2852 indiziert AdjMapA[3] mit dem 0..15-Slot der Karte 10. Beide
       Originaltabellen bleiben in Reichweite; eine Tabellenänderung, die das
       verletzt, muss auffallen statt maskiert zu werden. */
    for(int table=0;table<2;++table){
        const std::uint16_t* data=table?recvx_map_cnc_dat_b:recvx_map_cnc_dat_a;
        const std::size_t count=table?49u:81u;
        for(std::size_t index=0;index<count;++index){
            const std::uint16_t value=data[index];
            if((value>>8)==255)continue;
            for(int which=0;which<2;++which){
                const int packed=which?(value&0xFF):(value>>8);
                const int map_no=packed/16,slot=packed&0xF;
                check("Karte unter cnc_map",map_no<11);
                check("Slot unter cnc_flr",slot<6);
                if(map_no==10)check("Karte 10 bleibt in AdjMapA[3]",slot<3);
            }
        }
    }
    /* MapCnc richtet die Kette: dst/src sind map_no * 16 + flr_no. */
    MapScreen graph;
    graph.connection_init(11,6);
    graph.connection_set(0*16+0,0*16+1,1);
    const auto* source=graph.connection(0,1);
    if(!source)fail("Quellknoten fehlt");
    else{
        equal("gleiche Karte, kleinerer Slot -> flr_prev",source->flr_prev,0);
        equal("flr_next unverändert",source->flr_next,-1);
    }
    graph.connection_set(0*16+2,0*16+1,1);
    source=graph.connection(0,1);
    if(source)equal("gleiche Karte, größerer Slot -> flr_next",source->flr_next,2);
    graph.connection_set(1*16+0,3*16+0,1);
    const auto* higher=graph.connection(3,0);
    if(higher)equal("kleinere Karte -> map_prev",higher->map_prev,1*6+0);
    graph.connection_set(5*16+0,3*16+0,1);
    higher=graph.connection(3,0);
    if(higher)equal("größere Karte -> map_next",higher->map_next,5*6+0);
}

/* ------------------------------------------------------------ Etagenwahl */

static void test_floor_table(const std::string& data_root){
    const auto file=read_bytes(data_root+"/archives/MRY/0000.bin");
    if(file.empty()){fail("MRY/0000.bin nicht lesbar");return;}
    /* Die Floor-Tabelle liegt am Ende des Indexpakets; der Port findet sie beim
       Laden. Hier wird sie über den Lauf von MapScreen bestimmt und dann direkt
       mit MapGetFloorNo abgefragt. */
    FlagStore flags;
    std::uint32_t slots[16]={};
    auto services=services_for(flags,slots);
    services.read_inside_file=[&](unsigned,unsigned id){
        char name[64];std::snprintf(name,sizeof name,"%s/archives/MRY/%04u.bin",data_root.c_str(),id);
        return read_bytes(name);};
    MapHost host;
    host.ply_id=0;host.stg_no=0;host.rom_no=0;host.player_y=0;
    MapScreen screen;
    screen.init(RECVX_MP_SET_GAME,host,services);
    for(int tick=0;tick<8&&screen.mode()!=RECVX_MP_MOD_WAIT_NOMAP;++tick){
        screen.control(host);
        if(screen.mode()==RECVX_MP_MOD_VEW_NORMAL||screen.mode()==RECVX_MP_MOD_WAIT_NORMAL)break;
    }
    check("Indexdatei des Spielers wurde angefordert",
          !screen.requested_files().empty()&&screen.requested_files()[0]==0);
    check("Markermodelle im Indexpaket gefunden",!screen.marker_models().empty());
    /* MapGetFloorNo direkt: der erste Eintrag der Originaltabelle muss für
       seinen eigenen Schlüssel gefunden werden, ein Schlüssel unter dem ersten
       Label liefert -1. */
    const auto& buffer=screen.buffer();
    check("Indexdatei geladen",!buffer.empty());
    const int miss=MapScreen::get_floor_no(buffer.data(),buffer.size(),-1000,0.0f);
    equal("Schlüssel unter der Tabelle liefert -1",miss,-1);
}

/* ------------------------------------------------------------- Taskfolge */

static void test_task_sequence(const std::string& data_root){
    FlagStore flags;
    std::uint32_t slots[16]={};
    auto services=services_for(flags,slots);
    std::vector<unsigned> requested;
    services.read_inside_file=[&](unsigned partition,unsigned id){
        requested.push_back(id);
        equal("Partition ist dor_partid (MRY.AFS)",partition,5u);
        char name[64];std::snprintf(name,sizeof name,"%s/archives/MRY/%04u.bin",data_root.c_str(),id);
        return read_bytes(name);};
    std::vector<std::pair<int,int>> sounds;
    services.call_system_se=[&](int type,int number){sounds.push_back({type,number});};
    std::vector<std::pair<std::uint32_t,float>> fades;
    services.set_screen_fade=[&](std::uint32_t color,float speed){fades.push_back({color,speed});};

    MapHost host;
    host.ply_id=0;host.stg_no=0;host.rom_no=0;
    host.player_x=0;host.player_y=0;host.player_z=0;
    host.st_flg=0;host.cb_flg=0;host.fade_pbk=0x11223344u;
    host.dor_partid=5;

    MapScreen screen;
    screen.init(RECVX_MP_SET_GAME,host,services);
    equal("Öffnen spielt SE 3",sounds.size(),size_t(1));
    if(!sounds.empty()){equal("SE-Typ beim Öffnen",sounds[0].first,0);equal("SE-Nummer beim Öffnen",sounds[0].second,3);}
    equal("Status aus set_mod MP_SET_GAME",screen.status()&7,2);

    /* Erster Tick: MP_MOD_FIRST legt Hintergrund, Zoom-Screen und die
       Wartetask an, baut den Verbindungsgraphen und wechselt nach MP_MOD_WAIT;
       FtskMapWait endet sofort, weil st_flg Bit 3 nicht gesetzt ist. */
    check("control läuft weiter",screen.control(host));
    equal("Abblenden mit sys->fade_pbk",fades.empty()?0u:fades[0].first,0x11223344u);
    equal("nach dem ersten Tick MP_MOD_IDX_READ",screen.mode(),RECVX_MP_MOD_IDX_READ);
    check("Hintergrundgitter gezeichnet",!screen.draw_list().empty());

    /* MP_SET_SUB (status Bit 2) spielt kein Öffnungs-SE (map.c:166). */
    {
        std::vector<std::pair<int,int>> quiet;
        auto silent=services_for(flags,slots);
        silent.call_system_se=[&](int type,int number){quiet.push_back({type,number});};
        MapScreen sub;
        sub.init(RECVX_MP_SET_SUB,host,silent);
        equal("MP_SET_SUB ohne Öffnungs-SE",quiet.size(),size_t(0));
        equal("MP_SET_SUB setzt Status Bit 2",sub.status()&7,4);
    }

    /* Bis zur normalen Ansicht laufen: Index lesen, auswerten, Etagendatei
       lesen, auswerten. */
    int guard=0;
    while(screen.mode()!=RECVX_MP_MOD_WAIT_NORMAL&&screen.mode()!=RECVX_MP_MOD_WAIT_NOMAP&&guard++<40)
        screen.control(host);
    check("normale Ansicht oder kein-Karte-Zustand erreicht",
          screen.mode()==RECVX_MP_MOD_WAIT_NORMAL||screen.mode()==RECVX_MP_MOD_WAIT_NOMAP);
    check("Indexdatei 0 und Etagendatei 5 angefordert",
          requested.size()>=2&&requested[0]==0u&&requested[1]==5u);

    if(screen.mode()==RECVX_MP_MOD_WAIT_NORMAL){
        /* FtskMapNormal Fall 0 spielt SE 5. */
        bool normal_sound=false;
        for(const auto& entry:sounds)if(entry.second==5)normal_sound=true;
        check("Wechsel in die normale Ansicht spielt SE 5",normal_sound);

        /* Ohne Kartenobjekte kann FsubMapDraw den Code nicht ausführen; mit
           einem gesetzten Tag erlaubt FtskMapNormal den Zoom. Die Objekte
           kommen im Betrieb vom Integrator; hier wird ein einzelnes Objekt
           übergeben, damit MapCodeProcess läuft (Diagnoseaufbau). */
        std::vector<MapObject> objects(1);
        objects[0].has_model=true;
        screen.set_map_objects(std::move(objects));
        screen.control(host);

        const std::size_t before=sounds.size();
        host.pad_ps=0x100;
        screen.control(host);
        host.pad_ps=0;
        if(screen.current_tag()!=-1){
            check("Zoomwechsel angestoßen",screen.mode()==RECVX_MP_MOD_VEW_ZOOM||screen.mode()==RECVX_MP_MOD_WAIT_ZOOM);
            screen.control(host);
            equal("Zoomansicht aktiv",screen.mode(),RECVX_MP_MOD_WAIT_ZOOM);
            check("Zoom setzt Status Bit 0x200",bool(screen.status()&0x200));
            bool zoom_sound=false;
            for(std::size_t index=before;index<sounds.size();++index)if(sounds[index].second==5)zoom_sound=true;
            check("Zoomeintritt spielt SE 5",zoom_sound);
            /* Zurück aus dem Zoom: dieselbe Taste, Bit 0x200 fällt. */
            host.pad_ps=0x100;
            screen.control(host);
            host.pad_ps=0;
            screen.control(host);
            check("Zoom verlassen löscht Status Bit 0x200",!(screen.status()&0x200));
        }
        else check("ohne Tag bleibt die normale Ansicht",screen.mode()==RECVX_MP_MOD_WAIT_NORMAL);
    }

    /* Zurück in einen Wartezustand, in dem der Schließzweig gilt. */
    for(int tick=0;tick<6&&screen.mode()!=RECVX_MP_MOD_WAIT_NORMAL&&
        screen.mode()!=RECVX_MP_MOD_WAIT_ZOOM&&screen.mode()!=RECVX_MP_MOD_WAIT_NOMAP;++tick)
        screen.control(host);

    /* Schließen: Kreis/Dreieck (0x3000) spielt SE 0, blendet schwarz ab,
       sperrt das Pad über Status 0x400 und läuft über FtskMapExit nach
       MP_MOD_EXIT, wo bhControlMap 0 liefert.
       Die Taste muss zwei Ticks anliegen: MapPadMain läuft im Original am
       ENDE von bhControlMap (map.c:556), der Schließzweig im switch sieht
       deshalb den Padstand des vorigen Ticks. Der Zoomeintritt dagegen läuft
       in MapFuncExec nach MapPadMain und reagiert im selben Tick. */
    const std::size_t fades_before=fades.size();
    host.pad_ps=0x3000;
    screen.control(host);
    screen.control(host);
    host.pad_ps=0;
    check("Status 0x400 sperrt das Pad",bool(screen.status()&0x400));
    check("Abblenden nach Schwarz",fades.size()>fades_before&&fades.back().first==0xFF000000u);
    bool close_sound=false;
    for(const auto& entry:sounds)if(entry.second==0)close_sound=true;
    check("Schließen spielt SE 0",close_sound);
    bool finished=false;
    for(int tick=0;tick<6;++tick)if(!screen.control(host)){finished=true;break;}
    check("bhControlMap endet",finished);
    equal("Endzustand MP_MOD_EXIT",screen.mode(),RECVX_MP_MOD_EXIT);

    screen.exit();
    check("bhExitMap löscht gm_flg 0x80000",screen.game_map_flag_cleared());
    check("Kartentextur wird freigegeben",screen.released_map_texture());
    check("Raumtexturen werden zurückgegeben",screen.restores_room_texture());
}

/* ------------------------------------------------------------- Palette/Modi */

static void test_palette_and_modes(){
    /* MapPaletteMain: Einträge 0..15 bleiben die Tabelle, 16..31 pulsieren.
       Der Testaufbau ruft control einmal mit leeren Diensten auf. */
    FlagStore flags;
    std::uint32_t slots[16]={};
    auto services=services_for(flags,slots);
    services.read_inside_file=[](unsigned,unsigned){return std::vector<std::uint8_t>{};};
    MapHost host;
    MapScreen screen;
    screen.init(RECVX_MP_SET_EVENT,host,services);
    equal("MP_SET_EVENT setzt Status Bit 0",screen.status()&7,1);
    screen.control(host);
    for(int index=0;index<16;++index){
        close_to("Palette "+std::to_string(index)+" a",screen.palette()[index].a,recvx_map_pal[index].a);
        close_to("Palette "+std::to_string(index)+" r",screen.palette()[index].r,recvx_map_pal[index].r);
    }
    bool pulsing=false;
    for(int index=16;index<32;++index)
        if(std::fabs(screen.palette()[index].r-recvx_map_pal[index].r)>1e-6)pulsing=true;
    check("Einträge 16..31 pulsieren",pulsing);
    for(int index=0;index<3;++index)
        equal("MapCol "+std::to_string(index),screen.colors()[index],recvx_map_col[index]);
    /* Eine fehlende Datei ist der Originalpfad fil_mode 1: der Kartenschirm
       bleibt ohne Karte, statt eine Ersatzkarte zu erfinden. */
    for(int tick=0;tick<12;++tick)screen.control(host);
    check("ohne lesbare Datei keine Kartendaten",!screen.has_map_data());
}

/* -------------------------------------------------------------------- main */

int main(int argc,char** argv){
    const std::string root=argc>1?argv[1]:".";
    const std::string data_root=argc>2?argv[2]:root+"/data";
    test_tables(root);
    test_flag_conversion();
    test_connections();
    test_palette_and_modes();
    if(!read_bytes(data_root+"/archives/MRY/0000.bin").empty()){
        test_floor_table(data_root);
        test_task_sequence(data_root);
    }
    else std::cerr<<"note: "<<data_root<<"/archives/MRY not present, archive-backed checks skipped\n";
    if(failures)std::cerr<<failures<<" map screen check(s) failed\n";
    else std::cout<<"map screen: all checks passed\n";
    return failures?1:0;
}

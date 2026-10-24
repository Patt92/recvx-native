#include "computer.h"
#include "back_color.h"
#include "asset_io.h"
#include "audio.h"
#include "ninja_matrix.h"
#include "ui.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
namespace recvx {
bool ComputerTerminal::begin(EventVm& events){
    if(active_||!(events.flags[10][0]&0x400000))return false;
    if(!frame_)frame_=std::make_unique<RECVX_COM_FRAME>();
    // bhSysCall: st_flg |= 0x4000000, sp_flg = 0x30 (re-read from the work after the switch).
    events.flags[5][0]|=0x4000000u;events.flags[6][0]=0x30;
    const RECVX_COM_SYSTEM before{events.flags[5][0],events.flags[10][0],events.flags[6][0],
                                  events.flags[11][0],events.flags[12][0]};
    recvx_com_begin(events.com_number,&before);
    *frame_={};frame_->active=1;active_=true;pending_=0;character_gaps_=0;message_gaps_=0;
    set_back_color(0); // bhMainSequence mn_mode 7 case 2 (system.c:2120-2124), the terminal's texture read
    return true;
}
void ComputerTerminal::tick(unsigned held,unsigned pressed,EventVm& events,AudioMixer* audio){
    if(!active_)return;
    // syRtcGetDate: the terminal shows the machine clock; syRtcInit failing
    // leaves com_flg 0x100 clear and the original falls back to 21:35:12.
    const auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};const std::tm* broken=std::localtime(&now);const bool clock=broken!=nullptr;
    if(clock)local=*broken;
    const RECVX_COM_INPUT input{held,pressed|pending_,clock?1:0,local.tm_hour,local.tm_min,local.tm_sec};
    pending_=0;
    const bool running=recvx_com_tick(&input,frame_.get());
    if(running)capture(); // keep the last pass of the task readable after it ends
    // CallSystemSe(0, 0x8000026x): MIDI slot 2, room-pack bank 2 lists 0x6a..0x6f.
    for(unsigned i=0;i<frame_->sound_count;++i)if(audio)audio->play_system_midi(frame_->sounds[i]);
    // Script command 240: bhStFlg(sys->ev_flg, flag).
    for(unsigned i=0;i<frame_->flag_count;++i)events.set_flag(1,frame_->flags[i],0);
    if(!running){
        // bhExitSpEvtComputer (sp_evt.c:762-777) changes the live sys words in
        // place. The event task keeps running while the terminal is open
        // (system.c:659/2121) and may change these words meanwhile (RM_1090
        // event 5: bhCineSet 1 clears cb_flg/st_flg 0x4), so the open-time copy
        // must not be written back.
        events.flags[11][0]|=0x4u;events.flags[6][0]=~0u;events.flags[11][0]|=0x8000u;
        events.flags[5][0]&=~0x4000000u;events.flags[10][0]&=~0x400000u;
        events.flags[12][0]|=0x20000u;events.flags[12][0]&=~0x80u;
        if(frame_->stand_player){
            // sp_evt.c:781 bhStandPlayerMotion (player.c:1114) beim Spielerbesitzer.
            if(stand_player)stand_player();else ++stand_gaps_;
        }
        active_=false;
    }
}
void ComputerTerminal::capture(){
    text_.clear();
    for(const auto& row:frame_->text){
        std::string line;
        for(const auto cell:row)line+=char((cell&0xffu)?(cell&0xffu)+32u:' ');
        while(!line.empty()&&line.back()==' ')line.pop_back();
        text_.push_back(line);
    }
    for(unsigned i=0;i<2;++i){
        const char* word=frame_->word[i];
        words_[i].assign(word,std::find(word,word+sizeof frame_->word[i],'\0'));
    }
}
namespace {
unsigned channel(unsigned color,unsigned shift){return (color>>shift)&255;}
void blend(std::uint32_t& dst,unsigned r,unsigned g,unsigned b,unsigned a){
    if(!a)return;
    const unsigned dr=channel(dst,16),dg=channel(dst,8),db=channel(dst,0);
    dst=0xff000000u|((r*a+dr*(255-a))/255)<<16|((g*a+dg*(255-a))/255)<<8|((b*a+db*(255-a))/255);
}
void fill(SoftwareRenderer& image,const RECVX_COM_DRAW& d){
    const int x0=std::max(0,int(d.x1)),y0=std::max(0,int(d.y1)),x1=std::min(image.width(),int(d.x2)),y1=std::min(image.height(),int(d.y2));
    auto& canvas=image.canvas();
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)blend(canvas[y*image.width()+x],channel(d.color,16),channel(d.color,8),channel(d.color,0),channel(d.color,24));
}
// njDrawQuadTexture: point-sampled (njTextureFilterMode(0)), colour modulation, trans 1 = alpha blend.
void quad(SoftwareRenderer& image,const SceneTexture& texture,const RECVX_COM_DRAW& d){
    const int x0=int(d.x1),y0=int(d.y1),x1=int(d.x2),y1=int(d.y2);if(x1<=x0||y1<=y0||texture.pixels.empty())return;
    auto& canvas=image.canvas();
    for(int y=std::max(0,y0);y<std::min(image.height(),y1);++y)for(int x=std::max(0,x0);x<std::min(image.width(),x1);++x){
        const float fu=d.u1+(d.u2-d.u1)*(float(x-x0)+.5f)/float(x1-x0),fv=d.v1+(d.v2-d.v1)*(float(y-y0)+.5f)/float(y1-y0);
        const unsigned tx=std::min(texture.width-1,unsigned(std::max(0.f,fu*float(texture.width)))),ty=std::min(texture.height-1,unsigned(std::max(0.f,fv*float(texture.height))));
        const auto texel=texture.pixels[ty*texture.width+tx];
        const unsigned r=channel(texel,16)*channel(d.color,16)/255,g=channel(texel,8)*channel(d.color,8)/255,b=channel(texel,0)*channel(d.color,0)/255;
        auto& out=canvas[y*image.width()+x];
        if(d.trans)blend(out,r,g,b,channel(texel,24)*channel(d.color,24)/255); /* host alpha 0xFF = opaque */ else out=0xff000000u|r<<16|g<<8|b;
    }
}
}
// bhDrawSpEvtComCharacter: njSetScreen(600, 608, 416, 528, 336) and
// njUserClipping(2, ...) restrict the character to the 160x160 window, whose
// centre is exactly the screen centre of that projection. The view matrix is
// the base camera matrix (cmat) followed by the script's translation,
// rotation and the fixed 0.05 scale.
void ComputerTerminal::draw_character(SoftwareRenderer& image)const{
    const auto& c=frame_->character;
    if(!c.active)return;
    const int w=int(c.clip_x2-c.clip_x1),h=int(c.clip_y2-c.clip_y1);
    if(w<=0||h<=0||!character_scene)return;
    auto scene=character_scene(unsigned(c.chr_no),unsigned(c.mtn_no),unsigned(c.frm_no));
    if(!scene){++character_gaps_;return;}
    NJS_MATRIX view;njUnitMatrix(&view);njScale(&view,1,-1,-1);
    njTranslate(&view,c.px,c.py,c.pz);
    njRotateXYZ(&view,c.ax,c.ay,c.az);
    njScale(&view,c.scale,c.scale,c.scale);
    SoftwareRenderer window(w,h);window.set_supersampling(image.supersampling());window.set_render_threads(image.render_threads());
    window.set_texture_filtering(image.texture_filtering());
    scene->background=0;
    // njClipZ(-1, fNaViwClipFar): the host projection needs a positive near
    // plane, and the 0.05-scaled model never reaches either plane.
    window.draw_view(*scene,view,c.dist,c.dist,false,{},0.001f,0);
    const auto& depth=window.depth();
    for(int y=0;y<h;++y){
        const int oy=y+int(c.clip_y1);
        if(oy<0||oy>=image.height())continue;
        for(int x=0;x<w;++x){
            const int ox=x+int(c.clip_x1);
            if(ox<0||ox>=image.width())continue;
            if(depth[std::size_t(y)*w+x]==0.0f)continue; // nothing drawn here
            image.canvas()[std::size_t(oy)*image.width()+ox]=window.pixels()[std::size_t(y)*w+x];
        }
    }
}
void ComputerTerminal::draw(SoftwareRenderer& image,const GameUi& ui,const EventVm& events)const{
    if(!active_||!frame_)return;
    if(textures_.empty()){
        // bhSetMemPvpTexture: SYSTEM entry 2 holds background, window/font/keyboard and icon pages.
        textures_=decode_texture_bundle(read_asset_file(data_/"archives/SYSTEM/0002.bin",8*1024*1024));
        if(textures_.size()<3)throw std::runtime_error("computer terminal textures missing");
    }
    std::vector<RECVX_COM_DRAW> draws(frame_->draw,frame_->draw+frame_->draw_count);
    // njDrawQuadTexture depth is -1/z: a larger z lies nearer. Polygons (-0.8 fade, -0.85 lines) cover the quads.
    std::stable_sort(draws.begin(),draws.end(),[](const auto& a,const auto& b){return a.kind!=b.kind?a.kind<b.kind:a.z<b.z;});
    for(const auto& d:draws){
        if(d.kind==1)break; // the solid polygons come after the character
        if(d.texture>=0&&unsigned(d.texture)<textures_.size())quad(image,textures_[d.texture],d);
    }
    draw_character(image);
    for(const auto& d:draws)if(d.kind==1)fill(image,d);
    // bhDispMessage(68, 434 - lines * 30, ..., room message): drawn after the quad pass.
    if(frame_->message_visible){
        try{ui.show_message(image,events.room_message(unsigned(frame_->message_number)),68,434-frame_->message_lines*30);}
        catch(const std::runtime_error&){++message_gaps_;}
    }
}
}

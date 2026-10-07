#include "computer.h"
#include "asset_io.h"
#include "audio.h"
#include "ui.h"
#include <algorithm>
#include <cstdio>
namespace recvx {
bool ComputerTerminal::begin(EventVm& events){
    if(active_||!(events.flags[10][0]&0x400000))return false;
    if(!frame_)frame_=std::make_unique<RECVX_COM_FRAME>();
    const RECVX_COM_SYSTEM before{events.flags[5][0],events.flags[10][0],events.flags[6][0]};
    recvx_com_begin(events.com_number,&before);
    // bhSysCall: st_flg |= 0x4000000, sp_flg = 0x30 (re-read from the work after the switch).
    events.flags[5][0]|=0x4000000u;events.flags[6][0]=0x30;
    *frame_={};frame_->active=1;active_=true;pending_=0;return true;
}
void ComputerTerminal::tick(unsigned held,unsigned pressed,EventVm& events,AudioMixer* audio){
    if(!active_)return;
    const RECVX_COM_INPUT input{held,pressed|pending_,0};pending_=0;
    const bool running=recvx_com_tick(&input,frame_.get());
    // CallSystemSe(0, 0x8000026x): MIDI slot 2, room-pack bank 2 lists 0x6a..0x6f.
    for(unsigned i=0;i<frame_->sound_count;++i)if(audio)audio->play_system_midi(frame_->sounds[i]);
    // Script command 0xf0: bhStFlg(sys->ev_flg, flag).
    for(unsigned i=0;i<frame_->flag_count;++i)events.set_flag(1,frame_->flags[i],0);
    if(!running){
        // bhExitSpEvtComputer: st_flg &= ~0x4000000, cb_flg &= ~0x400000, sp_flg = -1.
        events.flags[5][0]&=~0x4000000u;events.flags[10][0]&=~0x400000u;events.flags[6][0]=frame_->system.sp_flg;
        active_=false;
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
        if(d.trans)blend(out,r,g,b,std::min(255u,channel(texel,24)*2)*channel(d.color,24)/255); /* PS2 texel alpha 0x80 = opaque */ else out=0xff000000u|r<<16|g<<8|b;
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
    for(const auto& d:draws){if(d.kind==1)fill(image,d);else if(d.texture>=0&&unsigned(d.texture)<textures_.size())quad(image,textures_[d.texture],d);}
    // bhDispMessage(68, 434 - lines * 30, ..., room message): drawn after the quad pass.
    if(frame_->message_visible)ui.show_message(image,events.room_message(unsigned(frame_->message_number)),68,434-frame_->message_lines*30);
}
}

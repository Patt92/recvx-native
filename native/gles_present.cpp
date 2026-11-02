// Host-Praesentationsstufe fuer Android (metalfx_present.h): OpenGL ES 3.0 auf dem EGL-Fenster von SDL.
// Plattformanpassung, keine Spiellogik: das 640x480-Bild der Runtime wird unveraendert hochgeladen; die Stufe
// skaliert und bearbeitet nur die Anzeige. Gegenstueck zu metalfx_present.mm / fsr_present_d3d12.cpp.
//
// Kette je Bild (synchron auf dem Hauptthread, eigener GLES-3.0-Kontext auf dem SDL-Fenster):
//   Upload 640x480  ->  [FSR 1: EASU auf die Einpassung, danach RCAS]  ->  [CRT full: Hellpass + Gauss]
//   ->  present (Schaerfen, CRT, 4:3- bzw. 16:9-Einpassung) in den Fensterpuffer  ->  SDL_GL_SwapWindow.
// Synchron statt auf eigenem Thread: SDL sichert und restauriert beim Pausieren der App den auf dem Hauptthread
// aktuellen Kontext (SDL_androidevents.c); ein zweiter Thread wuerde diesen Lebenszyklus umgehen.
//
// FSR 1 ist AMD FidelityFX Super Resolution 1 (MIT, FidelityFX SDK 2.1.0
// Kits/FidelityFX/upscalers/fsr3/include/gpu/fsr1/ffx_fsr1.h): ffxFsrEasuFloat und FsrRcasF als GLSL-ES-Port in
// derselben Rechenfolge, das Textur-Gather von EASU als texelFetch der zwoelf Taps mit Randklemmung. Die
// Praesentation (Schaerfen, Strahlprofil, Maske, Bloom) ist derselbe Kern wie present_cs der FSR-Stufe.
// Frame-Generation gibt es auf Android nicht; Bildraten ueber 30 wiederholen das Bild (represent).
#include "metalfx_present.h"
#include <SDL.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace recvx {
namespace {
constexpr const char* vertex_shader=R"GLSL(#version 300 es
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.0-1.0,0.0,1.0);}
)GLSL";
// Zeilen eines Zielpuffers (FBO) zaehlen wie das Spielbild von oben: Zeile 0 ist v = 0.
constexpr const char* easu_shader=R"GLSL(#version 300 es
precision highp float;precision highp int;
uniform highp sampler2D src;uniform vec4 con0;uniform ivec2 src_max;
out vec4 o;
vec3 tap(ivec2 q){return texelFetch(src,clamp(q,ivec2(0),src_max),0).rgb;}
float approx_rcp(float v){return uintBitsToFloat(0x7ef07ebbu-floatBitsToUint(v));}
float approx_rsqrt(float v){return uintBitsToFloat(0x5f347d74u-(floatBitsToUint(v)>>1u));}
void easu_tap(inout vec3 aC,inout float aW,vec2 off,vec2 dir,vec2 len,float lob,float clp,vec3 c){
    vec2 v;v.x=(off.x*dir.x)+(off.y*dir.y);v.y=(off.x*(-dir.y))+(off.y*dir.x);v*=len;
    float d2=v.x*v.x+v.y*v.y;d2=min(d2,clp);
    float wB=(2.0/5.0)*d2+(-1.0);float wA=lob*d2+(-1.0);wB*=wB;wA*=wA;wB=(25.0/16.0)*wB+(-(25.0/16.0-1.0));
    float w=wB*wA;aC+=c*w;aW+=w;}
void easu_set(inout vec2 dir,inout float len,vec2 pp,bool biS,bool biT,bool biU,bool biV,float lA,float lB,float lC,float lD,float lE){
    float w=0.0;
    if(biS)w=(1.0-pp.x)*(1.0-pp.y);
    if(biT)w=pp.x*(1.0-pp.y);
    if(biU)w=(1.0-pp.x)*pp.y;
    if(biV)w=pp.x*pp.y;
    float dc=lD-lC;float cb=lC-lB;float lenX=max(abs(dc),abs(cb));lenX=approx_rcp(lenX);
    float dirX=lD-lB;dir.x+=dirX*w;lenX=clamp(abs(dirX)*lenX,0.0,1.0);lenX*=lenX;len+=lenX*w;
    float ec=lE-lC;float ca=lC-lA;float lenY=max(abs(ec),abs(ca));lenY=approx_rcp(lenY);
    float dirY=lE-lA;dir.y+=dirY*w;lenY=clamp(abs(dirY)*lenY,0.0,1.0);lenY*=lenY;len+=lenY*w;}
float luma(vec3 c){return c.b*0.5+(c.r*0.5+c.g);}
void main(){
    vec2 ip=floor(gl_FragCoord.xy);
    vec2 pp=ip*con0.xy+con0.zw;vec2 fp=floor(pp);pp-=fp;ivec2 f=ivec2(fp);
    vec3 b=tap(f+ivec2(0,-1)),c=tap(f+ivec2(1,-1));
    vec3 e=tap(f+ivec2(-1,0)),ff=tap(f),g=tap(f+ivec2(1,0)),h=tap(f+ivec2(2,0));
    vec3 i=tap(f+ivec2(-1,1)),j=tap(f+ivec2(0,1)),k=tap(f+ivec2(1,1)),l=tap(f+ivec2(2,1));
    vec3 n=tap(f+ivec2(0,2)),oo=tap(f+ivec2(1,2));
    float bL=luma(b),cL=luma(c),iL=luma(i),jL=luma(j),fL=luma(ff),eL=luma(e),kL=luma(k),lL=luma(l),hL=luma(h),gL=luma(g),oL=luma(oo),nL=luma(n);
    vec2 dir=vec2(0.0);float len=0.0;
    easu_set(dir,len,pp,true,false,false,false,bL,eL,fL,gL,jL);
    easu_set(dir,len,pp,false,true,false,false,cL,fL,gL,hL,kL);
    easu_set(dir,len,pp,false,false,true,false,fL,iL,jL,kL,nL);
    easu_set(dir,len,pp,false,false,false,true,gL,jL,kL,lL,oL);
    vec2 dir2=dir*dir;float dirR=dir2.x+dir2.y;bool zro=dirR<(1.0/32768.0);
    dirR=approx_rsqrt(dirR);dirR=zro?1.0:dirR;dir.x=zro?1.0:dir.x;dir*=vec2(dirR);
    len=len*0.5;len*=len;
    float stretch=(dir.x*dir.x+dir.y*dir.y)*approx_rcp(max(abs(dir.x),abs(dir.y)));
    vec2 len2=vec2(1.0+(stretch-1.0)*len,1.0+(-0.5)*len);
    float lob=0.5+((1.0/4.0-0.04)-0.5)*len;float clp=approx_rcp(lob);
    vec3 min4=min(min(min(ff,g),j),k),max4=max(max(max(ff,g),j),k);
    vec3 aC=vec3(0.0);float aW=0.0;
    easu_tap(aC,aW,vec2(0.0,-1.0)-pp,dir,len2,lob,clp,b);
    easu_tap(aC,aW,vec2(1.0,-1.0)-pp,dir,len2,lob,clp,c);
    easu_tap(aC,aW,vec2(-1.0,1.0)-pp,dir,len2,lob,clp,i);
    easu_tap(aC,aW,vec2(0.0,1.0)-pp,dir,len2,lob,clp,j);
    easu_tap(aC,aW,vec2(0.0,0.0)-pp,dir,len2,lob,clp,ff);
    easu_tap(aC,aW,vec2(-1.0,0.0)-pp,dir,len2,lob,clp,e);
    easu_tap(aC,aW,vec2(1.0,1.0)-pp,dir,len2,lob,clp,k);
    easu_tap(aC,aW,vec2(2.0,1.0)-pp,dir,len2,lob,clp,l);
    easu_tap(aC,aW,vec2(2.0,0.0)-pp,dir,len2,lob,clp,h);
    easu_tap(aC,aW,vec2(1.0,0.0)-pp,dir,len2,lob,clp,g);
    easu_tap(aC,aW,vec2(1.0,2.0)-pp,dir,len2,lob,clp,oo);
    easu_tap(aC,aW,vec2(0.0,2.0)-pp,dir,len2,lob,clp,n);
    o=vec4(min(max4,max(min4,aC*vec3(1.0/aW))),1.0);
}
)GLSL";
constexpr const char* rcas_shader=R"GLSL(#version 300 es
precision highp float;precision highp int;
uniform highp sampler2D src;uniform float sharp;uniform ivec2 src_max;
out vec4 o;
vec3 tap(ivec2 q){return texelFetch(src,clamp(q,ivec2(0),src_max),0).rgb;}
float approx_rcp_medium(float v){float b=uintBitsToFloat(0x7ef19fffu-floatBitsToUint(v));return b*(-b*v+2.0);}
float luma(vec3 c){return c.b*0.5+(c.r*0.5+c.g);}
void main(){
    ivec2 sp=ivec2(floor(gl_FragCoord.xy));
    vec3 b=tap(sp+ivec2(0,-1)),d=tap(sp+ivec2(-1,0)),e=tap(sp),f=tap(sp+ivec2(1,0)),h=tap(sp+ivec2(0,1));
    float bL=luma(b),dL=luma(d),eL=luma(e),fL=luma(f),hL=luma(h);
    vec3 mn4=min(min(min(b,d),f),h),mx4=max(max(max(b,d),f),h);
    vec2 peakC=vec2(1.0,-1.0*4.0);
    float lowerLimiterMultiplier=clamp(eL/min(min(min(bL,dL),fL),hL),0.0,1.0);
    vec3 hitMin=mn4*(1.0/(4.0*mx4))*lowerLimiterMultiplier;
    vec3 hitMax=(peakC.x-mx4)*(1.0/(4.0*mn4+peakC.y));
    vec3 lobeRGB=max(-hitMin,hitMax);
    float lobe=max(-(0.25-(1.0/16.0)),min(max(max(lobeRGB.r,lobeRGB.g),lobeRGB.b),0.0))*sharp;
    float rcpL=approx_rcp_medium(4.0*lobe+1.0);
    o=vec4((lobe*b+lobe*d+lobe*h+lobe*f+e)*rcpL,1.0);
}
)GLSL";
constexpr const char* bloom_down_shader=R"GLSL(#version 300 es
precision highp float;
uniform highp sampler2D src;uniform vec2 dst_size;
out vec4 o;
vec3 to_linear(vec3 c){return pow(max(c,0.0),vec3(2.2));}
void main(){
    vec2 t=1.0/dst_size;vec2 uv=floor(gl_FragCoord.xy)*t+0.5*t;vec2 hh=t*0.25;
    vec3 c=to_linear(texture(src,uv+vec2(-hh.x,-hh.y)).rgb)+to_linear(texture(src,uv+vec2(hh.x,-hh.y)).rgb)+to_linear(texture(src,uv+vec2(-hh.x,hh.y)).rgb)+to_linear(texture(src,uv+vec2(hh.x,hh.y)).rgb);
    c*=0.25;float l=dot(c,vec3(0.2126,0.7152,0.0722));
    o=vec4(c*smoothstep(0.5,0.9,l),1.0);
}
)GLSL";
constexpr const char* bloom_blur_shader=R"GLSL(#version 300 es
precision highp float;
uniform highp sampler2D src;uniform vec2 dst_size;uniform vec2 dir;
out vec4 o;
const float w[5]=float[5](0.2270270270,0.1945945946,0.1216216216,0.0540540541,0.0162162162);
void main(){
    vec2 t=1.0/dst_size;vec2 uv=floor(gl_FragCoord.xy)*t+0.5*t;
    vec3 c=texture(src,uv).rgb*w[0];
    for(int i=1;i<5;++i){vec2 off=dir*t*float(i);c+=texture(src,uv+off).rgb*w[i];c+=texture(src,uv-off).rgb*w[i];}
    o=vec4(c,1.0);
}
)GLSL";
// Fensterpuffer: gl_FragCoord.y zaehlt von unten; gid ist die Pixelzeile von oben wie in present_cs.
constexpr const char* present_shader=R"GLSL(#version 300 es
precision highp float;precision highp int;
uniform highp sampler2D src_lin;uniform highp sampler2D src_pt;uniform highp sampler2D bloom_tex;
uniform vec2 src_size,fit_origin,fit_size;uniform ivec2 dst_size;
uniform int mode,nearest,game_lines,phosphor;
uniform float sharpness,bloom,mask_strength,sigma_dark,sigma_bright;
out vec4 o;
vec3 to_linear(vec3 c){return pow(max(c,0.0),vec3(2.2));}
vec3 to_srgb(vec3 c){return pow(max(c,0.0),vec3(1.0/2.2));}
vec3 knee(vec3 c){return mix(c,0.75+0.25*(1.0-exp(-(c-0.75)/0.25)),greaterThan(c,vec3(0.75)));}
void main(){
    uvec2 gid=uvec2(uint(gl_FragCoord.x),uint(dst_size.y-1-int(gl_FragCoord.y)));
    vec2 p=vec2(gid)+0.5-fit_origin;
    if(p.x<0.0||p.y<0.0||p.x>=fit_size.x||p.y>=fit_size.y){o=vec4(0.0,0.0,0.0,1.0);return;}
    vec2 uv=p/fit_size;
    vec3 c=(nearest!=0?texture(src_pt,uv):texture(src_lin,uv)).rgb;
    if(sharpness>0.0){
        vec2 t=1.0/src_size;
        vec3 a=texture(src_lin,uv+vec2(0.0,-t.y)).rgb,b=texture(src_lin,uv+vec2(-t.x,0.0)).rgb,d=texture(src_lin,uv+vec2(t.x,0.0)).rgb,e=texture(src_lin,uv+vec2(0.0,t.y)).rgb;
        vec3 mn=min(min(min(a,b),min(d,e)),c),mx=max(max(max(a,b),max(d,e)),c);
        vec3 amp=sqrt(clamp(min(mn,1.0-mx)/max(mx,vec3(1e-4)),0.0,1.0));
        vec3 w=amp*(-1.0/(8.0-3.0*sharpness));
        c=clamp(((a+b+d+e)*w+c)/(4.0*w+1.0),0.0,1.0);
    }
    if(mode!=0){
        vec3 l=to_linear(c);
        float line=p.y/fit_size.y*float(game_lines);float f=fract(line)-0.5;
        float lum=dot(l,vec3(0.2126,0.7152,0.0722));float sigma=mix(sigma_dark,sigma_bright,clamp(lum,0.0,1.0));
        float beam=exp(-f*f/(2.0*sigma*sigma));float beam_gain=mix(1.0,1.0/min(1.0,sigma*2.5066283),0.5);
        vec3 mask=vec3(1.0);
        if(mode==1){uint k=gid.x%3u;mask=vec3(k==0u?1.0:1.0-mask_strength,k==1u?1.0:1.0-mask_strength,k==2u?1.0:1.0-mask_strength);}
        else{uint ph=uint(phosphor);uint k=(gid.x/ph)%3u;float stagger=((gid.x/(ph*3u))&1u)!=0u?0.5:0.0;float sl=fract(line+stagger);float slot=sl<0.08?1.0-mask_strength*0.5:1.0;
             mask=vec3(k==0u?1.0:1.0-mask_strength,k==1u?1.0:1.0-mask_strength,k==2u?1.0:1.0-mask_strength)*slot;}
        l=l*beam*beam_gain*mask;
        if(bloom>0.0)l+=texture(bloom_tex,uv).rgb*bloom;
        c=to_srgb(mode==2?knee(l):l);
    }
    o=vec4(c,1.0);
}
)GLSL";
GLuint compile(GLenum type,const char* source,const char* name){
    const GLuint s=glCreateShader(type);glShaderSource(s,1,&source,nullptr);glCompileShader(s);
    GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048]{};glGetShaderInfoLog(s,sizeof log,nullptr,log);glDeleteShader(s);throw std::runtime_error(std::string("GLES presentation shader ")+name+": "+log);}
    return s;}
GLuint program(const char* fragment,const char* name){
    const GLuint vs=compile(GL_VERTEX_SHADER,vertex_shader,"vertex"),fs=compile(GL_FRAGMENT_SHADER,fragment,name);
    const GLuint p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);glDeleteShader(vs);glDeleteShader(fs);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[2048]{};glGetProgramInfoLog(p,sizeof log,nullptr,log);glDeleteProgram(p);throw std::runtime_error(std::string("GLES presentation program ")+name+": "+log);}
    return p;}
struct Target {GLuint tex=0,fbo=0;int w=0,h=0;};
class GlesPresenter final:public MetalFxPresenter {
public:
    GlesPresenter(SDL_Window* window,const VideoSettings& initial):window_(window),settings_(initial){
        context_=SDL_GL_CreateContext(window_);if(!context_)throw std::runtime_error(std::string("GLES 3.0 context: ")+SDL_GetError());
        if(SDL_GL_MakeCurrent(window_,context_)!=0){SDL_GL_DeleteContext(context_);throw std::runtime_error(std::string("GLES make current: ")+SDL_GetError());}
        try{
            const char* version=reinterpret_cast<const char*>(glGetString(GL_VERSION));
            if(!version||std::strncmp(version,"OpenGL ES 3",11)!=0)throw std::runtime_error(std::string("OpenGL ES 3.0 required, driver reports ")+(version?version:"nothing"));
            renderer_name_=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
            easu_=program(easu_shader,"easu");rcas_=program(rcas_shader,"rcas");bloom_down_=program(bloom_down_shader,"bloom down");
            bloom_blur_=program(bloom_blur_shader,"bloom blur");present_=program(present_shader,"present");
            glGenVertexArrays(1,&vao_);
            glGenSamplers(1,&lin_);glSamplerParameteri(lin_,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glSamplerParameteri(lin_,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glSamplerParameteri(lin_,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glSamplerParameteri(lin_,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glGenSamplers(1,&pt_);glSamplerParameteri(pt_,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glSamplerParameteri(pt_,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glSamplerParameteri(pt_,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glSamplerParameteri(pt_,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            // Spielbild 0xAARRGGBB (Bytes B,G,R,A) als GL_RGBA: Swizzle tauscht Rot und Blau beim Lesen.
            glGenTextures(1,&src_);glBindTexture(GL_TEXTURE_2D,src_);glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,640,480);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_R,GL_BLUE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_B,GL_RED);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            // Bloom in Halbfloat, wenn der Treiber darauf zeichnen kann (EXT_color_buffer_half_float/float), sonst RGBA8.
            const char* ext=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
            half_float_target_=ext&&(std::strstr(ext,"GL_EXT_color_buffer_half_float")||std::strstr(ext,"GL_EXT_color_buffer_float"));
            if(GLenum e=glGetError();e!=GL_NO_ERROR)throw std::runtime_error("GLES presentation setup failed (GL error "+std::to_string(e)+")");
        }catch(...){release();throw;}
        apply_swap_interval();
    }
    ~GlesPresenter()override{release();}
    void configure(const VideoSettings& settings)override{settings_=settings;dirty_=true;apply_swap_interval();}
    bool upscaler_supported()const override{return true;}
    bool frame_generation_supported()const override{return false;}
    std::uint32_t upscalers()const override{return 1u<<unsigned(Upscaler::Fsr1);}
    bool synchronous()const override{return true;}
    const PresentStats& stats()const override{return stats_;}
    std::string describe()const override{
        std::ostringstream out;out<<"GLES "<<renderer_name_<<": ";
        if(source_upscaled_)out<<"FSR 1 "<<fit_w_<<"x"<<fit_h_;else out<<(settings_.upscaler==Upscaler::Off&&settings_.post_process==PostProcess::Off?"nearest":"bilinear");
        if(settings_.sharpness!=Sharpness::Off)out<<", sharpen "<<settings_.sharpness_amount();
        if(settings_.post_process==PostProcess::CrtLight)out<<", CRT light";else if(settings_.post_process==PostProcess::CrtFull)out<<", CRT full (phosphor "<<phosphor_<<"px, bloom)";
        if(!notes_.empty())out<<" ("<<notes_<<")";
        return out.str();}
    bool present(const PresentFrame& frame)override{
        if(broken_||frame.width!=640||frame.height!=480||!frame.pixels)return false;
        if(SDL_GL_MakeCurrent(window_,context_)!=0){++stats_.skipped;return true;} // Oberflaeche pausiert (App im Hintergrund)
        try{
            int dw=0,dh=0;SDL_GL_GetDrawableSize(window_,&dw,&dh);dw=std::max(dw,1);dh=std::max(dh,1);
            if(dirty_||dw!=drawable_w_||dh!=drawable_h_)rebuild(dw,dh);
            glBindTexture(GL_TEXTURE_2D,src_);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,frame.pixels);
            glBindVertexArray(vao_);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);
            source_=src_;source_w_=640;source_h_=480;source_upscaled_=false;
            if(easu_target_.tex&&rcas_target_.tex){
                draw_into(easu_target_,easu_);
                glUniform4f(glGetUniformLocation(easu_,"con0"),640.0f/float(fit_w_),480.0f/float(fit_h_),0.5f*640.0f/float(fit_w_)-0.5f,0.5f*480.0f/float(fit_h_)-0.5f);
                glUniform2i(glGetUniformLocation(easu_,"src_max"),639,479);
                bind_texture(0,src_,pt_);glUniform1i(glGetUniformLocation(easu_,"src"),0);glDrawArrays(GL_TRIANGLES,0,3);
                draw_into(rcas_target_,rcas_);
                glUniform1f(glGetUniformLocation(rcas_,"sharp"),std::exp2(-0.25f)); // FSR-1-Standard 0.25 Blendenstufen
                glUniform2i(glGetUniformLocation(rcas_,"src_max"),fit_w_-1,fit_h_-1);
                bind_texture(0,easu_target_.tex,pt_);glUniform1i(glGetUniformLocation(rcas_,"src"),0);glDrawArrays(GL_TRIANGLES,0,3);
                source_=rcas_target_.tex;source_w_=fit_w_;source_h_=fit_h_;source_upscaled_=true;}
            if(bloom_a_.tex&&bloom_b_.tex){
                draw_into(bloom_a_,bloom_down_);glUniform2f(glGetUniformLocation(bloom_down_,"dst_size"),float(bloom_a_.w),float(bloom_a_.h));
                bind_texture(0,source_,lin_);glUniform1i(glGetUniformLocation(bloom_down_,"src"),0);glDrawArrays(GL_TRIANGLES,0,3);
                blur(bloom_a_,bloom_b_,1.0f,0.0f);blur(bloom_b_,bloom_a_,0.0f,1.0f);}
            show();
            if(GLenum e=glGetError();e!=GL_NO_ERROR)throw std::runtime_error("GL error "+std::to_string(e));
            SDL_GL_SwapWindow(window_);++stats_.presents;return true;
        }catch(const std::exception& e){notes_=e.what();broken_=true;return false;}}
    void represent()override{
        if(broken_||!source_||SDL_GL_MakeCurrent(window_,context_)!=0)return;
        glBindVertexArray(vao_);show();SDL_GL_SwapWindow(window_);++stats_.presents;++stats_.repeated;}
private:
    void apply_swap_interval(){if(context_&&SDL_GL_MakeCurrent(window_,context_)==0)SDL_GL_SetSwapInterval(settings_.vsync()?1:0);}
    static void destroy(Target& t){if(t.fbo)glDeleteFramebuffers(1,&t.fbo);if(t.tex)glDeleteTextures(1,&t.tex);t={};}
    Target target(int w,int h,bool half){
        Target t;t.w=std::max(w,1);t.h=std::max(h,1);
        glGenTextures(1,&t.tex);glBindTexture(GL_TEXTURE_2D,t.tex);glTexStorage2D(GL_TEXTURE_2D,1,half?GL_RGBA16F:GL_RGBA8,t.w,t.h);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glGenFramebuffers(1,&t.fbo);glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t.tex,0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){destroy(t);glBindFramebuffer(GL_FRAMEBUFFER,0);throw std::runtime_error("GLES render target incomplete");}
        glBindFramebuffer(GL_FRAMEBUFFER,0);return t;}
    void draw_into(const Target& t,GLuint p){glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);glViewport(0,0,t.w,t.h);glUseProgram(p);}
    void bind_texture(unsigned unit,GLuint tex,GLuint sampler){glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,tex);glBindSampler(unit,sampler);}
    void blur(const Target& from,const Target& to,float dx,float dy){
        draw_into(to,bloom_blur_);glUniform2f(glGetUniformLocation(bloom_blur_,"dst_size"),float(to.w),float(to.h));glUniform2f(glGetUniformLocation(bloom_blur_,"dir"),dx,dy);
        bind_texture(0,from.tex,lin_);glUniform1i(glGetUniformLocation(bloom_blur_,"src"),0);glDrawArrays(GL_TRIANGLES,0,3);}
    // Einpassung (4:3 bzw. 16:9 mit dem Breitbild-Mod) und Ziele fuer Fenstergroesse und Einstellungen.
    void rebuild(int dw,int dh){
        dirty_=false;drawable_w_=dw;drawable_h_=dh;notes_.clear();
        const double aw=settings_.widescreen?640.0*4.0/3.0:640.0; // Host-Mod Breitbild (widescreen.h)
        const double scale=std::min(dw/aw,dh/480.0);fit_w_=std::max(1,int(std::floor(aw*scale)));fit_h_=std::max(1,int(std::floor(480.0*scale)));
        fit_x_=(dw-fit_w_)/2;fit_y_=(dh-fit_h_)/2;phosphor_=std::max(1,int(std::lround(fit_w_/640.0/2.0)));
        destroy(easu_target_);destroy(rcas_target_);destroy(bloom_a_);destroy(bloom_b_);
        if(settings_.upscaler==Upscaler::Fsr1){
            if(fit_w_<=640&&fit_h_<=480)notes_="FSR 1: window not larger than 640x480";
            else{easu_target_=target(fit_w_,fit_h_,false);rcas_target_=target(fit_w_,fit_h_,false);}}
        else if(settings_.upscaler!=Upscaler::Off)notes_="this upscaler is not available on Android";
        if(settings_.frame_generation!=FrameGeneration::Off)notes_+=std::string(notes_.empty()?"":", ")+"no frame generation on Android";
        if(settings_.post_process==PostProcess::CrtFull){bloom_a_=target(fit_w_/4,fit_h_/4,half_float_target_);bloom_b_=target(fit_w_/4,fit_h_/4,half_float_target_);}}
    void show(){
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,drawable_w_,drawable_h_);glUseProgram(present_);
        bind_texture(0,source_,lin_);bind_texture(1,source_,pt_);bind_texture(2,bloom_a_.tex?bloom_a_.tex:source_,lin_);
        glUniform1i(glGetUniformLocation(present_,"src_lin"),0);glUniform1i(glGetUniformLocation(present_,"src_pt"),1);glUniform1i(glGetUniformLocation(present_,"bloom_tex"),2);
        glUniform2f(glGetUniformLocation(present_,"src_size"),float(source_w_),float(source_h_));
        glUniform2f(glGetUniformLocation(present_,"fit_origin"),float(fit_x_),float(fit_y_));glUniform2f(glGetUniformLocation(present_,"fit_size"),float(fit_w_),float(fit_h_));
        glUniform2i(glGetUniformLocation(present_,"dst_size"),drawable_w_,drawable_h_);
        const int mode=settings_.post_process==PostProcess::CrtLight?1:settings_.post_process==PostProcess::CrtFull?2:0;
        glUniform1i(glGetUniformLocation(present_,"mode"),mode);
        glUniform1i(glGetUniformLocation(present_,"nearest"),!source_upscaled_&&settings_.upscaler==Upscaler::Off&&mode==0?1:0);
        glUniform1i(glGetUniformLocation(present_,"game_lines"),480);glUniform1i(glGetUniformLocation(present_,"phosphor"),phosphor_);
        glUniform1f(glGetUniformLocation(present_,"sharpness"),settings_.sharpness_amount());
        glUniform1f(glGetUniformLocation(present_,"bloom"),mode==2&&bloom_a_.tex?0.35f:0.0f);
        glUniform1f(glGetUniformLocation(present_,"mask_strength"),mode==2?0.35f:0.18f);
        glUniform1f(glGetUniformLocation(present_,"sigma_dark"),0.22f);glUniform1f(glGetUniformLocation(present_,"sigma_bright"),0.30f);
        glDrawArrays(GL_TRIANGLES,0,3);}
    void release(){
        if(!context_)return;
        if(SDL_GL_MakeCurrent(window_,context_)==0){
            destroy(easu_target_);destroy(rcas_target_);destroy(bloom_a_);destroy(bloom_b_);
            if(src_)glDeleteTextures(1,&src_);if(lin_)glDeleteSamplers(1,&lin_);if(pt_)glDeleteSamplers(1,&pt_);if(vao_)glDeleteVertexArrays(1,&vao_);
            for(GLuint p:{easu_,rcas_,bloom_down_,bloom_blur_,present_})if(p)glDeleteProgram(p);}
        SDL_GL_MakeCurrent(window_,nullptr);SDL_GL_DeleteContext(context_);context_=nullptr;}
    SDL_Window* window_;SDL_GLContext context_=nullptr;VideoSettings settings_;PresentStats stats_;std::string notes_,renderer_name_;
    GLuint easu_=0,rcas_=0,bloom_down_=0,bloom_blur_=0,present_=0,vao_=0,lin_=0,pt_=0,src_=0,source_=0;
    Target easu_target_,rcas_target_,bloom_a_,bloom_b_;
    int drawable_w_=0,drawable_h_=0,fit_x_=0,fit_y_=0,fit_w_=640,fit_h_=480,phosphor_=1,source_w_=640,source_h_=480;
    bool dirty_=true,broken_=false,half_float_target_=false,source_upscaled_=false;
};
}
std::unique_ptr<MetalFxPresenter> create_gles_presenter(void* window,const VideoSettings& initial,std::string& reason){
    try{return std::make_unique<GlesPresenter>(static_cast<SDL_Window*>(window),initial);}
    catch(const std::exception& e){reason=e.what();return nullptr;}
}
}

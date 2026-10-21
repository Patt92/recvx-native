#define SDL_MAIN_HANDLED
#include "window_presentation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void fit(int width,int height){
    std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> surface(
        SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_ARGB8888),SDL_FreeSurface);
    check(bool(surface),SDL_GetError());
    std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(SDL_CreateSoftwareRenderer(surface.get()),SDL_DestroyRenderer);
    check(bool(renderer),SDL_GetError());
    check(recvx::configure_window_presentation(renderer.get())==0,SDL_GetError());
    float sx=0,sy=0;SDL_RenderGetScale(renderer.get(),&sx,&sy);
    const float expected=std::min(float(width)/640.f,float(height)/480.f);
    check(std::abs(sx-expected)<0.002f&&std::abs(sy-expected)<0.002f,"image does not fit drawable at 4:3");
    check(SDL_SetRenderDrawColor(renderer.get(),0,0,0,255)==0,SDL_GetError());
    check(SDL_RenderClear(renderer.get())==0,SDL_GetError());
    check(SDL_SetRenderDrawColor(renderer.get(),255,255,255,255)==0,SDL_GetError());
    const SDL_Rect image{0,0,640,480};check(SDL_RenderFillRect(renderer.get(),&image)==0,SDL_GetError());
    SDL_RenderPresent(renderer.get());
    // The image fills the limiting axis, rather than leaving a 1x picture centred in a 2x window.
    const int x=width/2,y=height/2;
    const auto pixel=[&](int px,int py){return static_cast<const Uint32*>(surface->pixels)[py*(surface->pitch/4)+px]&0xffffffu;};
    check(pixel(x,y)==0xffffffu,"image centre missing");
    if(float(width)/height>640.f/480.f)check(pixel(x,1)==0xffffffu&&pixel(x,height-2)==0xffffffu,"vertical output remains small");
    else check(pixel(1,y)==0xffffffu&&pixel(width-2,y)==0xffffffu,"horizontal output remains small");
}
int main(){try{
    for(const auto size:{SDL_Point{1280,960},SDL_Point{1280,959},SDL_Point{1280,900},SDL_Point{1920,1080},SDL_Point{640,480},SDL_Point{500,375},SDL_Point{2560,1920}})fit(size.x,size.y);
    std::cout<<"window presentation fits drawable across integer-scale boundaries\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#pragma once
#include <SDL.h>
namespace recvx {
// Host presentation only: fit the fixed 640x480 game image into the drawable,
// preserving 4:3. Integer scaling would drop to 1x below 960 output pixels in
// height, leaving a 640x480 image inside an otherwise almost 1280x960 window.
// Host-Mod Breitbild (widescreen.h): die logische Flaeche 16:9 dehnt das anamorphe Bild beim Kopieren.
inline int configure_window_presentation(SDL_Renderer* renderer,bool widescreen=false){
    if(SDL_RenderSetIntegerScale(renderer,SDL_FALSE)<0)return -1;
    return widescreen?SDL_RenderSetLogicalSize(renderer,1280,720):SDL_RenderSetLogicalSize(renderer,640,480);
}
}

// Web-Plattformanpassung (Emscripten): SDL gehoert dem Browser-Hauptthread, die Runtime laeuft unveraendert
// auf einem eigenen pthread (web_main.cpp). Jeder SDL-Aufruf der Runtime wird beim Linken per
// -Wl,--wrap=SDL_x auf __wrap_SDL_x umgelenkt (CMakeLists.txt, RECVX_WEB_SDL_WRAPS) und hier synchron auf dem
// Hauptthread ausgefuehrt. Keine Spiellogik: gleiche Argumente, gleiche Rueckgabewerte, gleiche Reihenfolge.
//
// Grund (docs/WEB.md): Browser-Ereignisse, WebGL2-Kontext und WebAudio existieren nur auf dem
// Hauptthread; die blockierende Schleife der Runtime (SDL_WaitEventTimeout, Raster-Worker) darf den Hauptthread
// nicht belegen. Einzige Abweichung im Ablauf: SDL_WaitEventTimeout wartet auf dem Spielthread (Abfrage auf dem
// Hauptthread je 1 ms), damit der Hauptthread nie blockiert; Rueckgabe- und Ereignissemantik bleiben gleich.
#include <SDL.h>
#include <SDL_syswm.h>
#include <emscripten.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#include <pthread.h>
#include <time.h>
#include <functional>
#include <utility>
#include "web_sdl_proxy.h"

namespace {
emscripten::ProxyingQueue& queue(){static emscripten::ProxyingQueue q;return q;}
pthread_t main_thread_id;
bool main_thread_known=false;
template<class F> auto on_main(F&& f)->decltype(f()){
    if(!main_thread_known||emscripten_is_main_browser_thread())return f();
    using R=decltype(f());
    if constexpr(std::is_void_v<R>){queue().proxySync(main_thread_id,std::function<void()>(std::forward<F>(f)));}
    else{R result{};queue().proxySync(main_thread_id,std::function<void()>([&]{result=f();}));return result;}
}
}
namespace recvx {
void web_sdl_bind_main_thread(){main_thread_id=pthread_self();main_thread_known=true;}
void web_sdl_pump_main_thread(){queue().execute();}
}

extern "C" {
#define RECVX_REAL(ret,name,...) ret __real_##name(__VA_ARGS__);
RECVX_REAL(int,SDL_Init,Uint32)
RECVX_REAL(void,SDL_Quit,void)
RECVX_REAL(SDL_bool,SDL_SetHint,const char*,const char*)
RECVX_REAL(SDL_Window*,SDL_CreateWindow,const char*,int,int,int,int,Uint32)
RECVX_REAL(void,SDL_DestroyWindow,SDL_Window*)
RECVX_REAL(void,SDL_RaiseWindow,SDL_Window*)
RECVX_REAL(void,SDL_SetWindowTitle,SDL_Window*,const char*)
RECVX_REAL(int,SDL_GetWindowDisplayMode,SDL_Window*,SDL_DisplayMode*)
RECVX_REAL(SDL_bool,SDL_GetWindowWMInfo,SDL_Window*,SDL_SysWMinfo*)
RECVX_REAL(SDL_Renderer*,SDL_CreateRenderer,SDL_Window*,int,Uint32)
RECVX_REAL(void,SDL_DestroyRenderer,SDL_Renderer*)
RECVX_REAL(int,SDL_RenderSetIntegerScale,SDL_Renderer*,SDL_bool)
RECVX_REAL(int,SDL_RenderSetLogicalSize,SDL_Renderer*,int,int)
RECVX_REAL(int,SDL_RenderSetVSync,SDL_Renderer*,int)
RECVX_REAL(SDL_Texture*,SDL_CreateTexture,SDL_Renderer*,Uint32,int,int,int)
RECVX_REAL(void,SDL_DestroyTexture,SDL_Texture*)
RECVX_REAL(int,SDL_UpdateTexture,SDL_Texture*,const SDL_Rect*,const void*,int)
RECVX_REAL(int,SDL_RenderClear,SDL_Renderer*)
RECVX_REAL(int,SDL_RenderCopy,SDL_Renderer*,SDL_Texture*,const SDL_Rect*,const SDL_Rect*)
RECVX_REAL(void,SDL_RenderPresent,SDL_Renderer*)
RECVX_REAL(int,SDL_PollEvent,SDL_Event*)
RECVX_REAL(int,SDL_PushEvent,SDL_Event*)
RECVX_REAL(const Uint8*,SDL_GetKeyboardState,int*)
RECVX_REAL(SDL_Keymod,SDL_GetModState,void)
RECVX_REAL(SDL_AudioDeviceID,SDL_OpenAudioDevice,const char*,int,const SDL_AudioSpec*,SDL_AudioSpec*,int)
RECVX_REAL(void,SDL_CloseAudioDevice,SDL_AudioDeviceID)
RECVX_REAL(void,SDL_PauseAudioDevice,SDL_AudioDeviceID,int)
RECVX_REAL(int,SDL_QueueAudio,SDL_AudioDeviceID,const void*,Uint32)
RECVX_REAL(Uint32,SDL_GetQueuedAudioSize,SDL_AudioDeviceID)
RECVX_REAL(void,SDL_ClearQueuedAudio,SDL_AudioDeviceID)
RECVX_REAL(int,SDL_NumJoysticks,void)
RECVX_REAL(SDL_bool,SDL_IsGameController,int)
RECVX_REAL(SDL_JoystickID,SDL_JoystickGetDeviceInstanceID,int)
RECVX_REAL(const char*,SDL_JoystickNameForIndex,int)
RECVX_REAL(SDL_GameController*,SDL_GameControllerOpen,int)
RECVX_REAL(void,SDL_GameControllerClose,SDL_GameController*)
RECVX_REAL(SDL_GameController*,SDL_GameControllerFromInstanceID,SDL_JoystickID)
RECVX_REAL(Sint16,SDL_GameControllerGetAxis,SDL_GameController*,SDL_GameControllerAxis)
RECVX_REAL(Uint8,SDL_GameControllerGetButton,SDL_GameController*,SDL_GameControllerButton)
RECVX_REAL(const char*,SDL_GameControllerName,SDL_GameController*)
RECVX_REAL(int,SDL_GameControllerRumble,SDL_GameController*,Uint16,Uint16,Uint32)
RECVX_REAL(int,SDL_GameControllerAddMappingsFromRW,SDL_RWops*,int)
RECVX_REAL(int,SDL_WaitEventTimeout,SDL_Event*,int)
#undef RECVX_REAL

int __wrap_SDL_Init(Uint32 flags){return on_main([&]{return __real_SDL_Init(flags);});}
void __wrap_SDL_Quit(void){on_main([&]{__real_SDL_Quit();});}
SDL_bool __wrap_SDL_SetHint(const char* n,const char* v){return on_main([&]{return __real_SDL_SetHint(n,v);});}
SDL_Window* __wrap_SDL_CreateWindow(const char* t,int x,int y,int w,int h,Uint32 f){return on_main([&]{return __real_SDL_CreateWindow(t,x,y,w,h,f);});}
void __wrap_SDL_DestroyWindow(SDL_Window* w){on_main([&]{__real_SDL_DestroyWindow(w);});}
void __wrap_SDL_RaiseWindow(SDL_Window* w){on_main([&]{__real_SDL_RaiseWindow(w);});}
void __wrap_SDL_SetWindowTitle(SDL_Window* w,const char* t){on_main([&]{__real_SDL_SetWindowTitle(w,t);});}
int __wrap_SDL_GetWindowDisplayMode(SDL_Window* w,SDL_DisplayMode* m){return on_main([&]{return __real_SDL_GetWindowDisplayMode(w,m);});}
SDL_bool __wrap_SDL_GetWindowWMInfo(SDL_Window* w,SDL_SysWMinfo* i){return on_main([&]{return __real_SDL_GetWindowWMInfo(w,i);});}
SDL_Renderer* __wrap_SDL_CreateRenderer(SDL_Window* w,int i,Uint32 f){return on_main([&]{return __real_SDL_CreateRenderer(w,i,f);});}
void __wrap_SDL_DestroyRenderer(SDL_Renderer* r){on_main([&]{__real_SDL_DestroyRenderer(r);});}
int __wrap_SDL_RenderSetIntegerScale(SDL_Renderer* r,SDL_bool e){return on_main([&]{return __real_SDL_RenderSetIntegerScale(r,e);});}
int __wrap_SDL_RenderSetLogicalSize(SDL_Renderer* r,int w,int h){return on_main([&]{return __real_SDL_RenderSetLogicalSize(r,w,h);});}
int __wrap_SDL_RenderSetVSync(SDL_Renderer* r,int v){return on_main([&]{return __real_SDL_RenderSetVSync(r,v);});}
SDL_Texture* __wrap_SDL_CreateTexture(SDL_Renderer* r,Uint32 f,int a,int w,int h){return on_main([&]{return __real_SDL_CreateTexture(r,f,a,w,h);});}
void __wrap_SDL_DestroyTexture(SDL_Texture* t){on_main([&]{__real_SDL_DestroyTexture(t);});}
int __wrap_SDL_UpdateTexture(SDL_Texture* t,const SDL_Rect* r,const void* p,int pitch){return on_main([&]{return __real_SDL_UpdateTexture(t,r,p,pitch);});}
int __wrap_SDL_RenderClear(SDL_Renderer* r){return on_main([&]{return __real_SDL_RenderClear(r);});}
int __wrap_SDL_RenderCopy(SDL_Renderer* r,SDL_Texture* t,const SDL_Rect* s,const SDL_Rect* d){return on_main([&]{return __real_SDL_RenderCopy(r,t,s,d);});}
void __wrap_SDL_RenderPresent(SDL_Renderer* r){on_main([&]{__real_SDL_RenderPresent(r);});}
int __wrap_SDL_PollEvent(SDL_Event* e){return on_main([&]{return __real_SDL_PollEvent(e);});}
int __wrap_SDL_PushEvent(SDL_Event* e){return on_main([&]{return __real_SDL_PushEvent(e);});}
// Zeigt auf SDLs Tastenzustandsfeld im gemeinsamen Speicher; der Inhalt wird vom Hauptthread fortgeschrieben.
const Uint8* __wrap_SDL_GetKeyboardState(int* n){return on_main([&]{return __real_SDL_GetKeyboardState(n);});}
SDL_Keymod __wrap_SDL_GetModState(void){return on_main([&]{return __real_SDL_GetModState();});}
SDL_AudioDeviceID __wrap_SDL_OpenAudioDevice(const char* d,int c,const SDL_AudioSpec* w,SDL_AudioSpec* o,int a){return on_main([&]{return __real_SDL_OpenAudioDevice(d,c,w,o,a);});}
void __wrap_SDL_CloseAudioDevice(SDL_AudioDeviceID d){on_main([&]{__real_SDL_CloseAudioDevice(d);});}
void __wrap_SDL_PauseAudioDevice(SDL_AudioDeviceID d,int p){on_main([&]{__real_SDL_PauseAudioDevice(d,p);});}
int __wrap_SDL_QueueAudio(SDL_AudioDeviceID d,const void* p,Uint32 n){return on_main([&]{return __real_SDL_QueueAudio(d,p,n);});}
Uint32 __wrap_SDL_GetQueuedAudioSize(SDL_AudioDeviceID d){return on_main([&]{return __real_SDL_GetQueuedAudioSize(d);});}
void __wrap_SDL_ClearQueuedAudio(SDL_AudioDeviceID d){on_main([&]{__real_SDL_ClearQueuedAudio(d);});}
int __wrap_SDL_NumJoysticks(void){return on_main([&]{return __real_SDL_NumJoysticks();});}
SDL_bool __wrap_SDL_IsGameController(int i){return on_main([&]{return __real_SDL_IsGameController(i);});}
SDL_JoystickID __wrap_SDL_JoystickGetDeviceInstanceID(int i){return on_main([&]{return __real_SDL_JoystickGetDeviceInstanceID(i);});}
const char* __wrap_SDL_JoystickNameForIndex(int i){return on_main([&]{return __real_SDL_JoystickNameForIndex(i);});}
SDL_GameController* __wrap_SDL_GameControllerOpen(int i){return on_main([&]{return __real_SDL_GameControllerOpen(i);});}
void __wrap_SDL_GameControllerClose(SDL_GameController* c){on_main([&]{__real_SDL_GameControllerClose(c);});}
SDL_GameController* __wrap_SDL_GameControllerFromInstanceID(SDL_JoystickID j){return on_main([&]{return __real_SDL_GameControllerFromInstanceID(j);});}
Sint16 __wrap_SDL_GameControllerGetAxis(SDL_GameController* c,SDL_GameControllerAxis a){return on_main([&]{return __real_SDL_GameControllerGetAxis(c,a);});}
Uint8 __wrap_SDL_GameControllerGetButton(SDL_GameController* c,SDL_GameControllerButton b){return on_main([&]{return __real_SDL_GameControllerGetButton(c,b);});}
const char* __wrap_SDL_GameControllerName(SDL_GameController* c){return on_main([&]{return __real_SDL_GameControllerName(c);});}
int __wrap_SDL_GameControllerRumble(SDL_GameController* c,Uint16 l,Uint16 h,Uint32 d){return on_main([&]{return __real_SDL_GameControllerRumble(c,l,h,d);});}
// SDL_GameControllerAddMappingsFromFile ist ein Makro auf ...FromRW(SDL_RWFromFile(...)); das RWops oeffnet
// die Datei auf dem aufrufenden Thread (WasmFS ist threadsicher), nur das Einlesen in SDL laeuft auf dem Hauptthread.
int __wrap_SDL_GameControllerAddMappingsFromRW(SDL_RWops* rw,int f){return on_main([&]{return __real_SDL_GameControllerAddMappingsFromRW(rw,f);});}
// SDL_WaitEventTimeout: Warten auf dem Spielthread, Ereignisabfrage auf dem Hauptthread. Rueckgabe wie SDL:
// 1 mit Ereignis (event!=nullptr: entnommen, sonst nur vorhanden), 0 bei Ablauf der Frist.
// SDL ruft SDL_WaitEventTimeout auch intern (SDL_PollEvent = WaitEventTimeout(e,0), SDL_events.c:957); --wrap
// lenkt diese inneren Aufrufe ebenfalls hierher. Auf dem Hauptthread gilt deshalb unveraendert SDLs Funktion.
int __wrap_SDL_WaitEventTimeout(SDL_Event* event,int timeout){
    if(!main_thread_known||emscripten_is_main_browser_thread())return __real_SDL_WaitEventTimeout(event,timeout);
    const Uint64 frequency=SDL_GetPerformanceFrequency(),start=SDL_GetPerformanceCounter();
    for(;;){
        // event==nullptr: SDL_PollEvent(nullptr) meldet nur, ob ein Ereignis ansteht, ohne es zu entnehmen.
        const int got=on_main([&]{return __real_SDL_PollEvent(event);});
        if(got)return 1;
        if(timeout>=0&&double(SDL_GetPerformanceCounter()-start)*1000.0/double(frequency)>=double(timeout))return 0;
        const timespec pause{0,1000000};nanosleep(&pause,nullptr);
    }
}
}

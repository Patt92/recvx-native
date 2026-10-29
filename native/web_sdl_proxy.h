#pragma once
// Web (Emscripten): Bindung der SDL-Aufrufe der Runtime an den Browser-Hauptthread (web_sdl_proxy.cpp).
namespace recvx {
// Einmal auf dem Browser-Hauptthread aufrufen, bevor der Spielthread startet.
void web_sdl_bind_main_thread();
// Hauptthread: anstehende SDL-Auftraege des Spielthreads ausfuehren (normalerweise erledigt das die Laufzeit).
void web_sdl_pump_main_thread();
}

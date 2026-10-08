/*
 * 32-Bit-Zeigerslots (arm64-Abweichung vom Original, minimal und dokumentiert).
 *
 * Original (PS2, ILP32): Zeiger sind 4 Byte. Die Gegnerdateien legen Zeiger in
 * rohe Arbeitspuffer (BH_PWORK.exp0/exp1, lkwkp-Ziel) in 4-Byte-Slots ab und
 * lesen sie wieder als `*(T**)(exp0+N)` bzw. `(T*)EXP0_I(N)`; Slots liegen
 * dicht (0x0, 0x4, 0x8 ...) und werden daneben als int/float beschrieben.
 * Auf macOS arm64 sind Zeiger 8 Byte und der Adressraum unterhalb 4 GiB ist
 * nicht abbildbar (getestet: mmap bei 0x10000000 scheitert, -pagezero_size
 * fuehrt zum Abbruch des Prozesses). Ein 8-Byte-Schreibzugriff wuerde den
 * Nachbarslot zerstoeren, ein int-Cast die oberen Adressbits verlieren.
 *
 * Daher speichert der Slot ein 32-Bit-Handle; die Zuordnung Handle <-> Zeiger
 * liegt in einer Tabelle (0 = NULL). Sonst keine Aenderung der Logik.
 * Erzeugt wird die Verwendung nur von tools/generate_enemy_sources.py
 * (Marker im Quelltext: recvx_p32_*). Die Handle-Tabelle waechst nur mit der
 * Anzahl unterschiedlicher je gespeicherter Adressen.
 */
#ifndef RECVX_PTR32_H
#define RECVX_PTR32_H
#include <stdint.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#if UINTPTR_MAX == 0xffffffffu
/* ILP32-Build der Original-Engine (wasm32): Zeiger passen wie auf der PS2 in
 * den Slot, das Handle ist die Adresse selbst. */
static inline uint32_t recvx_p32_intern(const void* p) { return (uint32_t)(uintptr_t)p; }
static inline void* recvx_p32_resolve(uint32_t handle) { return (void*)(uintptr_t)handle; }
static inline void recvx_p32_reset(void) {}
#else
uint32_t recvx_p32_intern(const void* p);   /* Zeiger -> Handle */
void* recvx_p32_resolve(uint32_t handle);   /* Handle -> Zeiger */
void recvx_p32_reset(void);                 /* nur fuer Tests */
#endif

/* Slot lesen/schreiben (unausgerichteter 4-Byte-Zugriff wie im Original). */
static inline void* recvx_p32_get(const void* slot)
{
    uint32_t h;
    memcpy(&h, slot, 4);
    return recvx_p32_resolve(h);
}
static inline void recvx_p32_set(void* slot, const void* p)
{
    const uint32_t h = recvx_p32_intern(p);
    memcpy(slot, &h, 4);
}
/* Zuweisungsausdruck: speichert und liefert den Zeiger (fuer `if ((*(T**)s = f()) != NULL)`). */
static inline void* recvx_p32_put(void* slot, const void* p)
{
    recvx_p32_set(slot, p);
    return (void*)p;
}
#ifdef __cplusplus
}
#endif
#endif

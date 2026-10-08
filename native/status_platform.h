/* Prototypes for the generated status/inventory sources (tools/generate_status_sources.py).
 * Metrowerks compiled sub1.c, itemview.c, fileview.c and message.c with
 * implicit declarations; the host compiler needs the original prototypes,
 * which are taken unchanged from the original headers listed here. Only
 * CoreZbuffOffI is the host-core boundary (see the generator, rewrite 3). */
#ifndef RECVX_STATUS_PLATFORM_H
#define RECVX_STATUS_PLATFORM_H
#include "types.h"
#include "flag.h"
#include "itemview.h"
#include "fileview.h"
#include "player.h"
#include "sdfunc.h"
#include "message.h"
#include "sub1.h"
/* ps2_loadtim2.h:28-30 (that header needs SDK-only TIM2 types) */
void SyncPath();
void D2_SyncTag();
void loadImage(void* tags);
void Ps2PxlconvCheck(void* timadr); /* ps2_loadtim2.h:27 */
void CoreZbuffOffI(void);
#endif

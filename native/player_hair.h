#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Inputs of one original frame for obwp[2] (Claire's ponytail, bhObjClpn).
typedef struct {
    float head[16];            // plp->mlwP->owP[5].mtx (world)
    int player_angles[6][3];   // plp->mlwP->objP[0..5].ang (local pose)
    int ax,ay,az;              // plp->ax/ay/az
    float root[3];             // ponytail owP[0] world position (object root)
    int reset;                 // plp->flg2 0x2: room entry/teleport clears the history
} RECVX_HAIR_INPUT;
// Runs bhObjClpn once and returns the ponytail objP[0..3].ang[0..1] (X/Y rotation per segment).
// Returns 0 on the initialising call (case 0), when no angles were produced yet.
int recvx_hair_tick(const RECVX_HAIR_INPUT* input,int angles[4][2]);
#ifdef __cplusplus
}
#endif

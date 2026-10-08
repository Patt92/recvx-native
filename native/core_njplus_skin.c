/* Original skin conversion of njplus.c (npCnkFlatOff .. npRetSkinConvert).
 * The definitions are extracted verbatim; see tools/generate_core_njplus_skin.py.
 * NP_WORK np and lcmat stay owned by the collision prefix module / ps2_dummy.c,
 * matching the original single-instance storage. */
#include "njplus.h"
#include "ps2_NaMatrix.h"
#include "ps2_dummy.h"
#include "core_njplus_collision.h"

extern NP_WORK np;

#include "core_njplus_skin.inc"

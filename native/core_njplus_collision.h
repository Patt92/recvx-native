#ifndef RECVX_CORE_NJPLUS_COLLISION_H
#define RECVX_CORE_NJPLUS_COLLISION_H

/*
 * Namespace boundary for the native port of njplus.c.  Original enemy sources
 * are compiled with this mapping, so they never bind directly to a ps2_* or
 * unprefixed host replacement.
 */
#define npPlusInit core_np_plus_init
#define npCollisionCheckCC core_np_collision_check_cc
#define npCollisionCheckCCEx core_np_collision_check_cc_ex
#define npCollisionCheckSC core_np_collision_check_sc
#define npDistanceP2C core_np_distance_p2c
#define npSetMemory core_np_set_memory
#define npSetMemoryL core_np_set_memory_l
#define npPushMdlstr core_np_push_mdlstr
#define npPopMdlstr core_np_pop_mdlstr
#define npPushMdlstr2 core_np_push_mdlstr2
#define npPopMdlstr2 core_np_pop_mdlstr2
#define npGetMatColor core_np_get_mat_color
#define npSetAllMatColor core_np_set_all_mat_color
#define npInitCalcSkin core_np_init_calc_skin
#define npCalcSkin core_np_calc_skin
#define npInitCalcSkinFM core_np_init_calc_skin_fm
#define npCalcSkinFM core_np_calc_skin_fm
#define npCutSkin core_np_cut_skin
#define npTransform core_np_transform
#define npCalcMorphing core_np_calc_morphing

#define njTranslateEx core_nj_translate_ex
#define njRotateEx core_nj_rotate_ex
#define njScaleEx core_nj_scale_ex
#define njMemCopy4 core_nj_mem_copy4

#endif

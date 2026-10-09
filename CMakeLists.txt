cmake_minimum_required(VERSION 3.16)
project(recvx_native VERSION 0.5.0 LANGUAGES C CXX)
include(CTest)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(RECVX_TABLES "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(OUTPUT "${RECVX_TABLES}/chunk_blend.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_chunk_blend.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_chunk_blend.py src/ps2/veronica/prog/ps2_NinjaCnk.c src/ps2/veronica/prog/ps2_NaSystem.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/door_structs.inc" "${RECVX_TABLES}/door_procedures.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_door_core.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_door_core.py tools/generate_native_tables.py include/ps2/veronica/prog/door.h src/ps2/veronica/prog/door.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/ui_tables.inc" "${RECVX_TABLES}/combinations.inc" "${RECVX_TABLES}/status_pulse.inc" "${RECVX_TABLES}/adx_tables.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_native_tables.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_native_tables.py include/ps2/veronica/prog/sub1.h src/ps2/veronica/prog/sub1.c src/ps2/veronica/prog/message.c src/ps2/veronica/prog/adxwrap.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/player_hair.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_player_hair.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_player_hair.py tools/generate_native_tables.py src/ps2/veronica/prog/objitm.c src/ps2/veronica/prog/player.c include/ps2/veronica/prog/objitm.h)
add_custom_command(OUTPUT "${RECVX_TABLES}/effect_particles.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_effect_particles.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_effect_particles.py tools/generate_native_tables.py include/ps2/veronica/prog/effsub3.h src/ps2/veronica/prog/effsub3.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/en04_damage.inc" "${RECVX_TABLES}/en01_damage.inc" "${RECVX_TABLES}/en01_damage_types.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_en01_damage.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS src/ps2/veronica/prog/en04.c tools/generate_en01_damage.py tools/generate_native_tables.py include/ps2/veronica/prog/types.h include/ps2/veronica/prog/en01.h include/ps2/veronica/prog/zonzon.h include/ps2/veronica/prog/zonzon1.h include/ps2/veronica/prog/macros.h src/ps2/veronica/prog/en01.c src/ps2/veronica/prog/zonzon.c src/ps2/veronica/prog/zonzon1.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/en07_bat.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_en07_bat.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_en07_bat.py tools/generate_native_tables.py src/ps2/veronica/prog/en07.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/core_njplus_collision.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_core_njplus.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}/core_njplus_collision.inc"
  DEPENDS tools/generate_core_njplus.py src/ps2/veronica/prog/njplus.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/core_njplus_skin.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_core_njplus_skin.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}/core_njplus_skin.inc"
  DEPENDS tools/generate_core_njplus_skin.py src/ps2/veronica/prog/njplus.c)
add_library(recvx_core_njplus STATIC native/core_njplus_collision.c native/core_njplus_memory.c native/core_njplus_modelstate.c native/core_njplus_skin.c "${RECVX_TABLES}/core_njplus_collision.inc" "${RECVX_TABLES}/core_njplus_skin.inc")
target_include_directories(recvx_core_njplus PRIVATE native build-tools/ninja include/ps2/veronica/prog "${RECVX_TABLES}" src/ps2/veronica/prog)
target_compile_definitions(recvx_core_njplus PRIVATE RECVX_NATIVE=1)
target_compile_options(recvx_core_njplus PRIVATE -w -include "${RECVX_TABLES}/types/types.h" -Wno-return-mismatch)
add_library(recvx_core_ninja STATIC native/core_ninja_ex.c)
target_include_directories(recvx_core_ninja PRIVATE native build-tools/ninja include/ps2/veronica/prog)
target_link_libraries(recvx_core_ninja PRIVATE recvx_math)
add_library(recvx_core_model STATIC native/core_model.c)
target_include_directories(recvx_core_model PRIVATE native build-tools/ninja include/ps2/veronica/prog "${RECVX_TABLES}" src/ps2/veronica/prog)
target_compile_options(recvx_core_model PRIVATE -include "${RECVX_TABLES}/types/types.h")
target_link_libraries(recvx_core_model PRIVATE recvx_math)
add_library(recvx_core_effect STATIC native/core_effect.c)
target_include_directories(recvx_core_effect PRIVATE native build-tools/ninja include/ps2/veronica/prog "${RECVX_TABLES}" src/ps2/veronica/prog)
target_compile_options(recvx_core_effect PRIVATE -include "${RECVX_TABLES}/types/types.h")
target_link_libraries(recvx_core_effect PRIVATE recvx_core_njplus)
add_library(recvx_core_object STATIC native/core_object.c)
target_include_directories(recvx_core_object PRIVATE native build-tools/ninja include/ps2/veronica/prog "${RECVX_TABLES}" src/ps2/veronica/prog)
target_compile_options(recvx_core_object PRIVATE -include "${RECVX_TABLES}/types/types.h")
target_link_libraries(recvx_core_object PRIVATE recvx_core_njplus)
add_library(recvx_core_enemy_arena STATIC native/core_enemy_arena.c)
target_include_directories(recvx_core_enemy_arena PRIVATE native build-tools/ninja include/ps2/veronica/prog "${RECVX_TABLES}" src/ps2/veronica/prog)
target_compile_options(recvx_core_enemy_arena PRIVATE -include "${RECVX_TABLES}/types/types.h")
add_library(recvx_core_enemy_interaction STATIC native/core_enemy_interaction.c)
target_include_directories(recvx_core_enemy_interaction PRIVATE native build-tools/ninja include/ps2/veronica/prog "${RECVX_TABLES}" src/ps2/veronica/prog)
target_compile_options(recvx_core_enemy_interaction PRIVATE -include "${RECVX_TABLES}/types/types.h")
target_link_libraries(recvx_core_enemy_interaction PRIVATE recvx_core_njplus recvx_core_effect recvx_math)
# Originale Gegnerschicht im Host-ABI als Link-/Abhaengigkeits-Audit. Der
# produktive Laufzeitpfad bleibt die gemeinsame native Actor-/MTN-Schicht;
# recvx_original_core32 darunter ist ein expliziter Importaudit.
file(GLOB RECVX_EN_SRC RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/src/ps2/veronica/prog" "${CMAKE_CURRENT_SOURCE_DIR}/src/ps2/veronica/prog/en[0-9]*.c")
# flag.c und message.c liefert recvx_original_status aus denselben Originalquellen;
# sie werden hier nicht erneut uebersetzt (sonst doppelte Definitionen).
set(RECVX_ORIG_MODULES eneset.c zonzon.c zonzon1.c hitchk.c hitchkl.c pwksub.c Motion.c weapon.c subpl.c rutchk.c effsub2.c effsub3.c effsub4.c light.c player.c ps2_NaColi.c MdlPut.c
  # Abhaengigkeiten im Abschluss von bhControlEnemy.
  cut.c face.c pl_evt.c playpch.c playpch2.c room.c ps2_NaFog.c ps2_NaMem.c binfunc.c camera.c dread.c ps2_event.c ps2_NinjaPtcl.c)
set(RECVX_ORIG_ENGINE_SRC ${RECVX_EN_SRC} ${RECVX_ORIG_MODULES})
set(RECVX_ORIG_DIR "${RECVX_TABLES}/enemy")
set(RECVX_ORIG_OUT "")
foreach(f ${RECVX_ORIG_ENGINE_SRC})
  list(APPEND RECVX_ORIG_OUT "${RECVX_ORIG_DIR}/${f}")
endforeach()
add_custom_command(OUTPUT "${RECVX_TABLES}/types/types.h"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_types.py" --out "${RECVX_TABLES}/types/types.h"
  DEPENDS tools/generate_types.py include/ps2/veronica/prog/types.h build-tools/ninja/ninjadef.h build-tools/ninja/ninjastr.h build-tools/ninja/ninjacnk.h build-tools/ninja/sg_xpt.h build-tools/ninja/ninjaapi.h)
add_custom_command(OUTPUT ${RECVX_ORIG_OUT}
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_enemy_sources.py" --out "${RECVX_ORIG_DIR}" ${RECVX_ORIG_ENGINE_SRC}
  DEPENDS tools/generate_enemy_sources.py "${RECVX_TABLES}/types/types.h")
add_library(recvx_original_engine STATIC EXCLUDE_FROM_ALL ${RECVX_ORIG_OUT} native/main_globals.c native/recvx_ptr32.c)
target_include_directories(recvx_original_engine PRIVATE build-tools/ninja native include/ps2/veronica/prog "${RECVX_TABLES}" src/ps2/veronica/prog)
target_compile_definitions(recvx_original_engine PRIVATE RECVX_NATIVE=1
  npPlusInit=core_np_plus_init npCollisionCheckCC=core_np_collision_check_cc
  npCollisionCheckCCEx=core_np_collision_check_cc_ex npCollisionCheckSC=core_np_collision_check_sc
  npDistanceP2C=core_np_distance_p2c npSetMemory=core_np_set_memory npSetMemoryL=core_np_set_memory_l
  npPushMdlstr=core_np_push_mdlstr npPopMdlstr=core_np_pop_mdlstr
  npPushMdlstr2=core_np_push_mdlstr2 npPopMdlstr2=core_np_pop_mdlstr2
  npGetMatColor=core_np_get_mat_color npSetAllMatColor=core_np_set_all_mat_color
  npInitCalcSkin=core_np_init_calc_skin npCalcSkin=core_np_calc_skin
  npInitCalcSkinFM=core_np_init_calc_skin_fm npCalcSkinFM=core_np_calc_skin_fm
  npCutSkin=core_np_cut_skin
  npTransform=core_np_transform npCalcMorphing=core_np_calc_morphing
  bhGetGidx=core_bh_get_gidx bhSetExtraEffectWork=core_bh_set_extra_effect_work
  bhSetEffect=core_bh_set_effect bhSetEffectTb=core_bh_set_effect_tb
  bhSetEffectEvt=core_bh_set_effect_evt bhDeleteYakkyou=core_bh_delete_yakkyou
  bhSetShadow=core_bh_set_shadow bhLinkBlood=core_bh_link_blood
  bhSetObject=core_bh_set_object bhSetItem=core_bh_set_item bhInitObjItm=core_bh_init_obj_itm
  njTranslateEx=core_nj_translate_ex
  njRotate=core_nj_rotate njRotateEx=core_nj_rotate_ex njScaleEx=core_nj_scale_ex njMemCopy4=core_nj_mem_copy4
  njGetSystemAttr=core_nj_get_system_attr njSetSystemAttr=core_nj_set_system_attr
  bhPutModel=core_bh_put_model bhCalcModel=core_bh_calc_model bhCalcTree=core_bh_calc_tree npCnkFlatOff=core_np_cnk_flat_off npClrTranslate=core_np_clr_translate npCopyMemory=core_np_copy_memory npGetWHDSizeSub=core_np_get_whd_size_sub npGetWHDSize=core_np_get_whd_size npSkinConvPreparation=core_np_skin_conv_preparation npSkinConvSub=core_np_skin_conv_sub npSkinConvMain=core_np_skin_conv_main npSkinConvert=core_np_skin_convert npRetSkinConvMain=core_np_ret_skin_conv_main npRetSkinConvert=core_np_ret_skin_convert
)
target_compile_options(recvx_original_engine PRIVATE -w -include "${RECVX_TABLES}/types/types.h" -Wno-return-mismatch -ffunction-sections)
target_link_libraries(recvx_original_engine INTERFACE recvx_core_njplus recvx_core_ninja recvx_core_model recvx_core_effect recvx_core_object recvx_core_enemy_arena recvx_core_enemy_interaction recvx_original_status)
# Original-Engine als ILP32-Modul (wasm32 -> wasm2c -> C), siehe tools/build_core32.py.
# Die Originalquellen laufen mit PS2-Strukturlayout; der Host bindet die Importe.
set(RECVX_CORE32_DIR "${RECVX_TABLES}/core32")
add_custom_command(OUTPUT "${RECVX_CORE32_DIR}/core32.c" "${RECVX_CORE32_DIR}/core32.h" "${RECVX_CORE32_DIR}/core32_gaps.c"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_core32.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}" "${RECVX_CORE32_DIR}"
  DEPENDS tools/build_core32.py tools/generate_types.py ${RECVX_ORIG_OUT} "${RECVX_TABLES}/core_njplus_collision.inc" "${RECVX_TABLES}/core_njplus_skin.inc"
    native/core32_bridge.c native/core32_libc.c native/core32_host.c native/main_globals.c native/recvx_ptr32.h
    native/core_njplus_collision.c native/core_njplus_memory.c native/core_njplus_modelstate.c native/core_njplus_skin.c
    native/core_model.c native/core_effect.c native/core_object.c native/core_ninja_ex.c native/ninja_matrix.c
    src/ps2/veronica/prog/ps2_NaMath.c
  VERBATIM)
add_library(recvx_original_core32 STATIC "${RECVX_CORE32_DIR}/core32.c" "${RECVX_CORE32_DIR}/core32_gaps.c" native/core32_host.c
  native/third_party/wasm2c/wasm-rt-impl.c native/third_party/wasm2c/wasm-rt-mem-impl.c)
target_include_directories(recvx_original_core32 PRIVATE "${RECVX_CORE32_DIR}" native/third_party/wasm2c PUBLIC native)
set_source_files_properties("${RECVX_CORE32_DIR}/core32.c" PROPERTIES COMPILE_OPTIONS "-O1;-w")
target_compile_options(recvx_original_core32 PRIVATE -w)
set(RECVX_STATUS_DIR "${RECVX_TABLES}/status")
set(RECVX_STATUS_OUT "${RECVX_STATUS_DIR}/sub1.c" "${RECVX_STATUS_DIR}/itemview.c" "${RECVX_STATUS_DIR}/fileview.c" "${RECVX_STATUS_DIR}/message.c" "${RECVX_STATUS_DIR}/flag.c" "${RECVX_STATUS_DIR}/ninja2d.c")
add_custom_command(OUTPUT ${RECVX_STATUS_OUT}
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_status_sources.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_status_sources.py tools/generate_native_tables.py tools/generate_types.py
    src/ps2/veronica/prog/sub1.c src/ps2/veronica/prog/itemview.c src/ps2/veronica/prog/fileview.c
    src/ps2/veronica/prog/message.c src/ps2/veronica/prog/flag.c
    src/ps2/veronica/prog/ps2_dummy.c src/ps2/veronica/prog/ps2_NaSystem.c
    include/ps2/veronica/prog/types.h build-tools/ninja/ninjadef.h build-tools/ninja/ninjastr.h
  VERBATIM)
add_library(recvx_original_status STATIC EXCLUDE_FROM_ALL ${RECVX_STATUS_OUT} native/core_gs.cpp native/core_gs_glue.c)
target_include_directories(recvx_original_status PRIVATE native "${RECVX_STATUS_DIR}" build-tools/ninja include/ps2/veronica/prog src/ps2/veronica/prog)
target_compile_options(recvx_original_status PRIVATE -w -Wno-return-mismatch)
target_compile_features(recvx_original_status PRIVATE cxx_std_17)
add_custom_command(OUTPUT "${RECVX_TABLES}/subplayer.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_subplayer.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_subplayer.py tools/generate_native_tables.py src/ps2/veronica/prog/subpl.c src/ps2/veronica/prog/eneset.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/player_stairs.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_player_stairs.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tools"
  DEPENDS tools/generate_player_stairs.py tools/generate_native_tables.py src/ps2/veronica/prog/player.c include/ps2/veronica/prog/player.h)
add_custom_command(OUTPUT "${RECVX_TABLES}/view_clip.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_view_clip.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_view_clip.py src/ps2/veronica/prog/event.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/weapon_flash.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_weapon_flash.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_weapon_flash.py tools/generate_native_tables.py src/ps2/veronica/prog/effsub1.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/targeting.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_targeting.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_targeting.py tools/generate_native_tables.py src/ps2/veronica/prog/pwksub.c src/ps2/veronica/prog/playpch.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/combat_tables.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_combat_tables.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_combat_tables.py tools/generate_native_tables.py src/ps2/veronica/prog/player.c src/ps2/veronica/prog/weapon.c src/ps2/veronica/prog/en01.c src/ps2/veronica/prog/en04.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/file_folder.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_file_folder.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_file_folder.py tools/generate_native_tables.py src/ps2/veronica/prog/itemview.c src/ps2/veronica/prog/fileview.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/inventory_engine.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_inventory_engine.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_inventory_engine.py tools/generate_native_tables.py src/ps2/veronica/prog/itemview.c src/ps2/veronica/prog/sub1.c src/ps2/veronica/prog/fileview.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/effect_sprites.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_effect_sprites.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_effect_sprites.py src/ps2/veronica/prog/effsub1.c src/ps2/veronica/prog/effsub5.c src/ps2/veronica/prog/player.c src/ps2/veronica/prog/light.c src/ps2/veronica/prog/effect.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/pickup_engine.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_pickup_engine.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_pickup_engine.py tools/generate_native_tables.py src/ps2/veronica/prog/sub1.c src/ps2/veronica/prog/weapon.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/item_use.inc" "${RECVX_TABLES}/item_use00.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_item_use.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_item_use.py tools/generate_native_tables.py src/ps2/veronica/prog/sub1.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/object_itmbox.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_object_itmbox.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_object_itmbox.py tools/generate_native_tables.py src/ps2/veronica/prog/objitm.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/event_inventory.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_event_inventory.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_event_inventory.py tools/generate_native_tables.py src/ps2/veronica/prog/event.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/computer.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_computer.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_computer.py tools/generate_native_tables.py src/ps2/veronica/prog/sp_evt.c include/ps2/veronica/prog/sp_evt.h include/ps2/veronica/prog/types.h)
add_custom_command(OUTPUT "${RECVX_TABLES}/box_engine.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_box_engine.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_box_engine.py tools/generate_native_tables.py src/ps2/veronica/prog/sub1.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/enemy_spawn.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_enemy_spawn.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_enemy_spawn.py tools/generate_native_tables.py src/ps2/veronica/prog/en04.c src/ps2/veronica/prog/zonzon.c)
add_custom_command(OUTPUT "${RECVX_TABLES}/gameover.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_gameover.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_gameover.py src/ps2/veronica/prog/gameover.c include/ps2/veronica/prog/gameover.h src/ps2/veronica/prog/screen.c src/ps2/veronica/prog/player.c tools/generate_native_tables.py)
add_custom_command(OUTPUT "${RECVX_TABLES}/effect_collision.inc"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_effect_collision.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
  DEPENDS tools/generate_effect_collision.py tools/generate_native_tables.py src/ps2/veronica/prog/hitchk.c)
add_library(recvx_game native/file_folder.cpp "${RECVX_TABLES}/file_folder.inc" native/effect_collision.cpp "${RECVX_TABLES}/effect_collision.inc" native/gameover.cpp "${RECVX_TABLES}/gameover.inc" native/effect_particles.cpp "${RECVX_TABLES}/effect_particles.inc" native/computer.cpp native/computer_core.c "${RECVX_TABLES}/computer.inc" native/subplayer.c "${RECVX_TABLES}/subplayer.inc" native/en01_damage.c "${RECVX_TABLES}/en01_damage.inc" "${RECVX_TABLES}/en01_damage_types.inc" native/enemy_spawn.cpp "${RECVX_TABLES}/enemy_spawn.inc" native/en07_bat.c "${RECVX_TABLES}/en07_bat.inc" native/effect_sprites.cpp "${RECVX_TABLES}/effect_sprites.inc" native/inspection.cpp "${RECVX_TABLES}/inventory_engine.inc" native/combat.cpp native/weapon_flash.cpp "${RECVX_TABLES}/weapon_flash.inc" native/targeting.cpp "${RECVX_TABLES}/targeting.inc" native/weapon_asset.cpp "${RECVX_TABLES}/combat_tables.inc" native/box_menu.cpp native/inventory.cpp "${RECVX_TABLES}/box_engine.inc" native/messages.cpp native/ui.cpp native/events.cpp native/audio.cpp native/movie.cpp native/world.cpp native/collision.cpp native/door_core.c native/doors.cpp src/ps2/veronica/prog/item.c "${RECVX_TABLES}/door_structs.inc" "${RECVX_TABLES}/door_procedures.inc" "${RECVX_TABLES}/combinations.inc" "${RECVX_TABLES}/ui_tables.inc" native/status_pulse.cpp native/snapshot.cpp native/view_clip.cpp "${RECVX_TABLES}/view_clip.inc" native/player_stairs.c "${RECVX_TABLES}/player_stairs.inc" native/player_hair.c "${RECVX_TABLES}/player_hair.inc" "${RECVX_TABLES}/status_pulse.inc" "${RECVX_TABLES}/adx_tables.inc")
target_sources(recvx_game PRIVATE native/core_random.cpp native/object_itmbox.cpp native/world_itmbox.cpp "${RECVX_TABLES}/object_itmbox.inc")
target_include_directories(recvx_game PRIVATE "${RECVX_TABLES}" build-tools/ninja)
target_sources(recvx_game PRIVATE "${RECVX_TABLES}/pickup_engine.inc" "${RECVX_TABLES}/event_inventory.inc" "${RECVX_TABLES}/item_use.inc" "${RECVX_TABLES}/item_use00.inc")
target_compile_definitions(recvx_game PRIVATE RECVX_NATIVE=1)
target_link_libraries(recvx_game PUBLIC recvx_scene)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
add_library(recvx_data native/expand.cpp native/archive.cpp native/room.cpp native/asset_io.cpp)
target_include_directories(recvx_data PUBLIC native)
target_compile_features(recvx_data PUBLIC cxx_std_17)
add_library(recvx_math src/ps2/veronica/prog/ps2_NaMath.c native/ninja_matrix.c)
target_compile_definitions(recvx_math PRIVATE RECVX_NATIVE=1)
target_include_directories(recvx_math PUBLIC native)
target_compile_features(recvx_math PRIVATE c_std_99)
if(NOT MSVC)
  target_link_libraries(recvx_math PUBLIC m)
  target_compile_options(recvx_math PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
endif()
add_library(recvx_disc src/ps2/veronica/prog/ps2_dvd_image.c)
target_include_directories(recvx_disc PRIVATE include/ps2/veronica/prog)
target_include_directories(recvx_data PUBLIC include/ps2/veronica/prog)
target_link_libraries(recvx_data PUBLIC recvx_disc)
add_library(recvx_scene native/scene.cpp native/renderer.cpp native/actor.cpp native/face.cpp native/item_asset.cpp native/chunk_blend.cpp "${RECVX_TABLES}/chunk_blend.inc")
target_include_directories(recvx_scene PRIVATE "${RECVX_TABLES}")
find_package(Threads REQUIRED)
target_link_libraries(recvx_scene PUBLIC recvx_data recvx_math Threads::Threads)
add_executable(recvx-render-benchmark EXCLUDE_FROM_ALL tools/benchmark_renderer.cpp)
target_link_libraries(recvx-render-benchmark PRIVATE recvx_scene)
if(MSVC)
  target_compile_options(recvx_scene PRIVATE /W4)
else()
  target_compile_options(recvx_scene PRIVATE -Wall -Wextra -Wpedantic)
endif()
option(RECVX_BUILD_RUNTIME "Build the native SDL window and room renderer" ON)
option(RECVX_FETCH_SDL2 "Download and build the pinned SDL2 dependency" OFF)
if(RECVX_BUILD_RUNTIME)
  if(RECVX_FETCH_SDL2)
    include(FetchContent)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL2
      URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
      URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165)
    FetchContent_MakeAvailable(SDL2)
  else()
    find_package(SDL2 2.0.18 CONFIG REQUIRED)
  endif()
  add_executable(recvx-runtime native/runtime.cpp)
  add_library(recvx_input native/input.cpp native/frontend.cpp native/debug_menu.cpp native/third_person.cpp native/video_settings.cpp)
  target_link_libraries(recvx_input PUBLIC recvx_game SDL2::SDL2)
  if(BUILD_TESTING)
    add_executable(recvx-debug-tests tests/native/debug_menu_test.cpp)
    target_link_libraries(recvx-debug-tests PRIVATE recvx_input)
    add_test(NAME debug_menu_hotkey_registry COMMAND recvx-debug-tests)
    add_executable(recvx-third-person-tests tests/native/third_person_test.cpp)
    target_link_libraries(recvx-third-person-tests PRIVATE recvx_input)
    add_test(NAME third_person_camera_mod COMMAND recvx-third-person-tests)
    add_executable(recvx-frontend-tests tests/native/frontend_test.cpp)
    target_link_libraries(recvx-frontend-tests PRIVATE recvx_input)
    add_test(NAME main_menu_settings_video_skip COMMAND recvx-frontend-tests)
    add_executable(recvx-event-lifecycle-tests tests/native/event_lifecycle_test.cpp)
    target_link_libraries(recvx-event-lifecycle-tests PRIVATE recvx_input)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0050.RDT")
      add_test(NAME event_lifecycle_control_release COMMAND recvx-event-lifecycle-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
    endif()
    add_executable(recvx-input-tests tests/native/input_test.cpp)
    target_link_libraries(recvx-input-tests PRIVATE recvx_input)
    add_test(NAME keyboard_controller_config COMMAND recvx-input-tests)
  endif()
  # recvx-runtime contains no ILP32/wasm2c enemy core.  That audit is built
  # only by its standalone historical test target below.
  target_link_libraries(recvx-runtime PRIVATE recvx_input)
  if(WIN32)
    add_custom_command(TARGET recvx-runtime POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:recvx-runtime>)
    if(BUILD_TESTING)
      add_custom_command(TARGET recvx-input-tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:recvx-input-tests>)
    endif()
    install(FILES $<TARGET_FILE:SDL2::SDL2> DESTINATION bin)
  elseif(APPLE)
    set_target_properties(recvx-runtime PROPERTIES INSTALL_RPATH "@loader_path/../lib")
  else()
    set_target_properties(recvx-runtime PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
  endif()
  install(TARGETS recvx-runtime RUNTIME DESTINATION bin)
endif()
add_executable(recvx-native native/main.cpp)
target_link_libraries(recvx-native PRIVATE recvx_data)
add_executable(recvx-assets native/export_assets.cpp)
target_link_libraries(recvx-assets PRIVATE recvx_scene)
add_executable(recvx-audio native/audio_main.cpp)
target_link_libraries(recvx-audio PRIVATE recvx_game)
add_executable(recvx-world-assets native/world_assets.cpp)
target_link_libraries(recvx-world-assets PRIVATE recvx_game)
if(MSVC)
  target_compile_options(recvx_data PRIVATE /W4)
  target_compile_options(recvx-native PRIVATE /W4)
else()
  target_compile_options(recvx_data PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(recvx-native PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(BUILD_TESTING)
  add_executable(recvx-chunk-blend-tests tests/native/chunk_blend_test.cpp)
  target_link_libraries(recvx-chunk-blend-tests PRIVATE recvx_scene)
  add_test(NAME original_chunk_blend COMMAND recvx-chunk-blend-tests)
  add_executable(recvx-inventory-engine-tests tests/native/inventory_engine_test.cpp)
  target_include_directories(recvx-inventory-engine-tests PRIVATE "${RECVX_TABLES}")
  target_link_libraries(recvx-inventory-engine-tests PRIVATE recvx_game)
  add_test(NAME original_inventory_table_coverage COMMAND recvx-inventory-engine-tests)
  add_executable(recvx-hd-texture-tests tests/native/hd_texture_test.cpp)
  target_link_libraries(recvx-hd-texture-tests PRIVATE recvx_data)
  add_test(NAME hd_texture_content_key COMMAND recvx-hd-texture-tests)
  add_executable(recvx-status-pulse-tests tests/native/status_pulse_test.cpp)
  target_link_libraries(recvx-status-pulse-tests PRIVATE recvx_game)
  add_test(NAME status_condition_pulse COMMAND recvx-status-pulse-tests)
  add_executable(recvx-snapshot-tests tests/native/snapshot_test.cpp)
  target_link_libraries(recvx-snapshot-tests PRIVATE recvx_game)
  add_test(NAME snapshot_savefile_roundtrip COMMAND recvx-snapshot-tests)
  add_executable(recvx-view-clip-tests tests/native/view_clip_test.cpp)
  target_link_libraries(recvx-view-clip-tests PRIVATE recvx_game)
  add_test(NAME original_view_clip COMMAND recvx-view-clip-tests)
  add_executable(recvx-inventory-tests tests/native/inventory_menu_test.cpp)
  target_link_libraries(recvx-inventory-tests PRIVATE recvx_game)
  add_test(NAME inventory_combine_inspect COMMAND recvx-inventory-tests)
  add_executable(recvx-file-reader-tests tests/native/file_reader_test.cpp)
  target_link_libraries(recvx-file-reader-tests PRIVATE recvx_game)
  add_test(NAME original_file_reader COMMAND recvx-file-reader-tests)
  add_executable(recvx-door-tests tests/native/door_test.cpp)
  target_link_libraries(recvx-door-tests PRIVATE recvx_game)
  add_test(NAME door_transition COMMAND recvx-door-tests)
  add_executable(recvx-collision-tests tests/native/collision_test.cpp)
  target_link_libraries(recvx-collision-tests PRIVATE recvx_game)
  add_test(NAME wall_collision COMMAND recvx-collision-tests)
  add_executable(recvx-audio-tests tests/native/audio_movie_test.cpp)
  target_link_libraries(recvx-audio-tests PRIVATE recvx_game)
  add_test(NAME audio_movie COMMAND recvx-audio-tests)
  add_executable(recvx-bgm-room-tests tests/native/bgm_room_test.cpp)
  target_link_libraries(recvx-bgm-room-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0050.RDT")
    add_test(NAME bgm_room_transition COMMAND recvx-bgm-room-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-computer-tests tests/native/computer_test.cpp)
  target_link_libraries(recvx-computer-tests PRIVATE recvx_game)
  add_test(NAME computer_terminal COMMAND recvx-computer-tests)
  add_executable(recvx-ammunition-tests tests/native/ammunition_test.cpp)
  target_link_libraries(recvx-ammunition-tests PRIVATE recvx_game)
  add_test(NAME ammunition_pickup_zero COMMAND recvx-ammunition-tests)
  add_executable(recvx-combat-tests tests/native/combat_test.cpp)
  target_link_libraries(recvx-combat-tests PRIVATE recvx_game)
  add_test(NAME combat_weapons_ammunition COMMAND recvx-combat-tests)
  add_executable(recvx-effect-debris-tests tests/native/effect_debris_test.cpp)
  target_link_libraries(recvx-effect-debris-tests PRIVATE recvx_game)
  add_test(NAME effect_collision_debris COMMAND recvx-effect-debris-tests)
  add_executable(recvx-effect-pool-tests tests/native/effect_pool_test.cpp)
  target_link_libraries(recvx-effect-pool-tests PRIVATE recvx_game)
  add_test(NAME effect_pool_lifetime COMMAND recvx-effect-pool-tests)
  add_executable(recvx-gameover-tests tests/native/gameover_test.cpp)
  target_link_libraries(recvx-gameover-tests PRIVATE recvx_game)
  add_test(NAME gameover_original COMMAND recvx-gameover-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-enemy-damage-tests tests/native/enemy_damage_test.cpp)
  target_link_libraries(recvx-enemy-damage-tests PRIVATE recvx_game)
  add_test(NAME enemy_damage_original_en01 COMMAND recvx-enemy-damage-tests)
  add_executable(recvx-gameplay-bindings-tests tests/native/gameplay_bindings_test.cpp)
  target_link_libraries(recvx-gameplay-bindings-tests PRIVATE recvx_game)
if(TARGET recvx_input)
 target_link_libraries(recvx-gameplay-bindings-tests PRIVATE recvx_input)
 target_compile_definitions(recvx-gameplay-bindings-tests PRIVATE RECVX_TEST_INPUT)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0050.RDT")
  add_test(NAME native-gameplay-bindings COMMAND recvx-gameplay-bindings-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
endif()
  add_executable(recvx-original-engine-tests tests/native/original_engine_test.cpp)
  # Historical ILP32 audit only; it is intentionally not linked into recvx_game.
  target_link_libraries(recvx-original-engine-tests PRIVATE recvx_game recvx_original_core32)
  target_compile_features(recvx-original-engine-tests PRIVATE cxx_std_17)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0020.RDT")
    add_test(NAME original_enemy_layer COMMAND recvx-original-engine-tests "${CMAKE_CURRENT_SOURCE_DIR}/data" RM_0020 300)
  endif()
  add_executable(recvx-script-motion-tests tests/native/script_motion_test.cpp)
  target_link_libraries(recvx-script-motion-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0050.RDT")
    add_test(NAME script_motion COMMAND recvx-script-motion-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-progression-tests tests/native/progression_test.cpp)
  target_link_libraries(recvx-progression-tests PRIVATE recvx_game)
  add_test(NAME engine_progression_resources COMMAND recvx-progression-tests)
  add_executable(recvx-game-tests tests/native/game_test.cpp)
  target_link_libraries(recvx-game-tests PRIVATE recvx_game)
  add_test(NAME inventory_events_assets COMMAND recvx-game-tests)
  add_executable(recvx-data-tests tests/native/expand_test.cpp)
  target_link_libraries(recvx-data-tests PRIVATE recvx_data)
  add_test(NAME expand COMMAND recvx-data-tests)
  add_executable(recvx-math-tests tests/native/math_test.cpp)
  target_link_libraries(recvx-math-tests PRIVATE recvx_math)
  target_compile_features(recvx-math-tests PRIVATE cxx_std_17)
  add_test(NAME math COMMAND recvx-math-tests)
  add_executable(recvx-archive-tests tests/native/archive_test.cpp)
  target_link_libraries(recvx-archive-tests PRIVATE recvx_data)
  add_test(NAME archive_room COMMAND recvx-archive-tests)
  add_executable(recvx-matrix-tests tests/native/matrix_test.cpp)
  target_link_libraries(recvx-matrix-tests PRIVATE recvx_math)
  target_compile_features(recvx-matrix-tests PRIVATE cxx_std_17)
  add_test(NAME matrix COMMAND recvx-matrix-tests)
  add_executable(recvx-texture-filter-tests tests/native/texture_filter_test.cpp)
  target_link_libraries(recvx-texture-filter-tests PRIVATE recvx_scene)
  add_test(NAME texture_filtering COMMAND recvx-texture-filter-tests)
  add_executable(recvx-camera-return-tests tests/native/camera_return_test.cpp)
  target_link_libraries(recvx-camera-return-tests PRIVATE recvx_game)
  add_test(NAME active_camera_return COMMAND recvx-camera-return-tests)
  add_executable(recvx-scene-tests tests/native/scene_test.cpp)
  target_link_libraries(recvx-scene-tests PRIVATE recvx_scene)
  add_test(NAME scene_renderer COMMAND recvx-scene-tests)
  add_executable(recvx-face-tests tests/native/face_test.cpp)
  target_link_libraries(recvx-face-tests PRIVATE recvx_game)
  add_test(NAME original_face_geometry COMMAND recvx-face-tests)
  add_executable(recvx-actor-tests tests/native/actor_test.cpp)
  target_link_libraries(recvx-actor-tests PRIVATE recvx_scene)
  add_test(NAME player_motion_skin COMMAND recvx-actor-tests)
endif()
install(TARGETS recvx-native RUNTIME DESTINATION bin)

if(BUILD_TESTING)
  add_executable(recvx-core-njplus-tests tests/native/core_njplus_test.cpp)
  target_include_directories(recvx-core-njplus-tests PRIVATE build-tools/ninja include/ps2/veronica/prog)
  target_compile_options(recvx-core-njplus-tests PRIVATE -include "${RECVX_TABLES}/types/types.h")
  target_link_libraries(recvx-core-njplus-tests PRIVATE recvx_core_njplus recvx_original_engine recvx_math)
  add_test(NAME core_njplus_collision COMMAND recvx-core-njplus-tests)
  add_executable(recvx-core-model-tests tests/native/core_model_test.cpp)
  target_include_directories(recvx-core-model-tests PRIVATE native build-tools/ninja include/ps2/veronica/prog)
  target_compile_options(recvx-core-model-tests PRIVATE -include "${RECVX_TABLES}/types/types.h")
  target_link_libraries(recvx-core-model-tests PRIVATE recvx_core_model recvx_math)
  add_test(NAME core_model_matrices COMMAND recvx-core-model-tests)
  add_executable(recvx-core-effect-tests tests/native/core_effect_test.cpp)
  target_include_directories(recvx-core-effect-tests PRIVATE native build-tools/ninja include/ps2/veronica/prog)
  target_compile_options(recvx-core-effect-tests PRIVATE -include "${RECVX_TABLES}/types/types.h")
  target_link_libraries(recvx-core-effect-tests PRIVATE recvx_core_effect recvx_original_engine recvx_core_njplus)
  add_test(NAME core_effect_pool COMMAND recvx-core-effect-tests)
  add_executable(recvx-core-object-tests tests/native/core_object_test.cpp)
  target_include_directories(recvx-core-object-tests PRIVATE native build-tools/ninja include/ps2/veronica/prog)
  target_compile_options(recvx-core-object-tests PRIVATE -include "${RECVX_TABLES}/types/types.h")
  target_link_libraries(recvx-core-object-tests PRIVATE recvx_core_object recvx_original_engine recvx_core_njplus)
  add_test(NAME core_object_setup COMMAND recvx-core-object-tests)
  add_executable(recvx-core-enemy-layout-tests tests/native/core_enemy_layout_test.cpp)
  target_include_directories(recvx-core-enemy-layout-tests PRIVATE native build-tools/ninja include/ps2/veronica/prog)
  target_compile_options(recvx-core-enemy-layout-tests PRIVATE -include "${RECVX_TABLES}/types/types.h")
  target_link_libraries(recvx-core-enemy-layout-tests PRIVATE recvx_core_object recvx_original_engine recvx_core_njplus)
  add_test(NAME core_enemy_layout COMMAND recvx-core-enemy-layout-tests "${RECVX_TABLES}")
  add_executable(recvx-core-enemy-arena-tests tests/native/core_enemy_arena_test.cpp)
  target_include_directories(recvx-core-enemy-arena-tests PRIVATE native build-tools/ninja include/ps2/veronica/prog)
  target_compile_options(recvx-core-enemy-arena-tests PRIVATE -include "${RECVX_TABLES}/types/types.h")
  target_link_libraries(recvx-core-enemy-arena-tests PRIVATE recvx_core_enemy_arena recvx_original_engine recvx_core_njplus)
  if(APPLE)
    target_link_options(recvx-core-enemy-arena-tests PRIVATE -Wl,-dead_strip)
  else()
    target_link_options(recvx-core-enemy-arena-tests PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME core_enemy_arena COMMAND recvx-core-enemy-arena-tests)
  add_executable(recvx-core-enemy-interaction-tests tests/native/core_enemy_interaction_test.cpp)
  target_include_directories(recvx-core-enemy-interaction-tests PRIVATE native build-tools/ninja include/ps2/veronica/prog)
  target_compile_options(recvx-core-enemy-interaction-tests PRIVATE -include "${RECVX_TABLES}/types/types.h")
  target_link_libraries(recvx-core-enemy-interaction-tests PRIVATE recvx_core_enemy_interaction recvx_original_engine recvx_core_njplus recvx_core_effect recvx_math)
  if(APPLE)
    target_link_options(recvx-core-enemy-interaction-tests PRIVATE -Wl,-dead_strip)
  else()
    target_link_options(recvx-core-enemy-interaction-tests PRIVATE -Wl,--gc-sections)
  endif()
  add_test(NAME core_enemy_interaction COMMAND recvx-core-enemy-interaction-tests)
  add_executable(recvx-en07-bat-tests tests/native/en07_bat_test.cpp)
  target_link_libraries(recvx-en07-bat-tests PRIVATE recvx_game)
  add_test(NAME en07_bat_tests COMMAND recvx-en07-bat-tests data)
  add_executable(recvx-continuity-tests tests/native/continuity_test.cpp)
  target_link_libraries(recvx-continuity-tests PRIVATE recvx_game)
  add_test(NAME core_continuity COMMAND recvx-continuity-tests)
endif()

# Original crate/player push state and Motion.c sampling.
add_custom_command(OUTPUT "${RECVX_TABLES}/player_push.inc"
 COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_player_push.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
 DEPENDS tools/generate_player_push.py tools/generate_native_tables.py src/ps2/veronica/prog/player.c src/ps2/veronica/prog/objitm.c src/ps2/veronica/prog/hitchk.c src/ps2/veronica/prog/Motion.c src/ps2/veronica/prog/pwksub.c src/ps2/veronica/prog/ps2_NaColi.c include/ps2/veronica/prog/player.h)
target_sources(recvx_game PRIVATE native/player_push.cpp native/player_push_core.c "${RECVX_TABLES}/player_push.inc")

if(BUILD_TESTING)
 add_executable(recvx-push-tests tests/native/push_test.cpp)
 target_link_libraries(recvx-push-tests PRIVATE recvx_game)
 if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/SYSTEM/0010.bin")
  add_test(NAME player_push COMMAND recvx-push-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
 endif()
endif()

add_custom_command(OUTPUT "${RECVX_TABLES}/player_combat.inc" "${RECVX_TABLES}/player_combat_types.inc"
 COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_player_combat.py" "${CMAKE_CURRENT_SOURCE_DIR}" "${RECVX_TABLES}"
 DEPENDS tools/generate_player_combat.py tools/generate_native_tables.py src/ps2/veronica/prog/player.c src/ps2/veronica/prog/playpch.c src/ps2/veronica/prog/playpch2.c src/ps2/veronica/prog/pwksub.c src/ps2/veronica/prog/weapon.c src/ps2/veronica/prog/Motion.c include/ps2/veronica/prog/player.h include/ps2/veronica/prog/playpch.h include/ps2/veronica/prog/types.h)
target_sources(recvx_game PRIVATE native/player_combat_core.c "${RECVX_TABLES}/player_combat.inc" "${RECVX_TABLES}/player_combat_types.inc")
# Preserve original C field aliasing and separate float multiply/add evaluation.
if(NOT MSVC)
 set_source_files_properties(native/player_push_core.c PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing;-ffp-contract=off")
endif()

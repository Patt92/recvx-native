cmake_minimum_required(VERSION 3.16)
project(recvx_native VERSION 0.5.0 LANGUAGES C CXX)
include(CTest)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_no_original_core.py"
  "${CMAKE_CURRENT_SOURCE_DIR}" RESULT_VARIABLE RECVX_POLICY_RESULT)
if(NOT RECVX_POLICY_RESULT EQUAL 0)
  message(FATAL_ERROR "Native-port policy blocked this configuration. No core exceptions; see docs/NATIVE_REIMPLEMENTATION_ROADMAP.md")
endif()
add_custom_target(recvx-native-policy ALL
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_no_original_core.py"
    "${CMAKE_CURRENT_SOURCE_DIR}" --build-dir "${CMAKE_CURRENT_BINARY_DIR}"
  VERBATIM)
if(BUILD_TESTING)
  add_test(NAME application_icon_containers
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/app_icons_test.py")
  add_test(NAME development_app_packaging
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/build_native_test.py")
  add_test(NAME native_port_policy_contracts
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/port_policy_test.py")
  add_test(NAME build_coverage
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_build_coverage_test.py")
  add_test(NAME iso9660_build_extraction
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/iso9660_test.py")
  add_test(NAME iso_data_pipeline
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/prepare_native_data_test.py")
  add_test(NAME native_port_status_contracts
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/port_check_test.py")
  # Negativpruefungen des Windows-PE-Audits; brauchen den Zig-Werkzeugsatz.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/toolchains/zig-cxx")
    add_test(NAME windows_link_audit
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/windows_audit_test.py")
  endif()
endif()
set(RECVX_TABLES "${CMAKE_CURRENT_BINARY_DIR}/generated")
# tools/generate_chunk_blend.py ist aus dem Build entfernt (Policy 2026-10-09, R02):
# ein Generator darf nicht bei jedem Build die PS2-Quellen lesen, auch nicht
# fuer reine Daten. Chunk-Blendmodi liegen als native/chunk_blend_data.inc im Baum (byte-identisch geprueft).
# tools/generate_door_core.py ist Quarantaene (Policy 2026-10-09): Tuerdemo-
# Prozeduren und Parameterstrukturen fehlen, siehe native/door_core.c.
# generate_native_tables.py ist aus dem Build entfernt (Policy 2026-10-09, R02).
# Es war ein Mischgenerator: neben den ADX-Datenwerten schrieb es
# combinations.inc, status_pulse.inc und ui_tables.inc mit kopierten
# Originalfunktionen. Diese vorübergehend zu erzeugen und danach zu löschen war
# kein Schutzkonzept. Die ADX-Werte liegen jetzt als natives Datenmodul
# native/adx_tables.c/.h im Baum (byte-identisch geprüft), ebenso die reinen
# UI-Tabellen in native/ui_tables_data.inc. Beim Build wird nichts erzeugt.
# tools/generate_player_hair.py ist Quarantäne (Policy 2026-10-09): die
# Haarphysik von obwp[2] fehlt bis zur nativen Neubestimmung, siehe
# native/player_hair.c.
# tools/generate_effect_particles.py ist Quarantaene (Policy 2026-10-09): die
# Partikelemitter und die Blutlache fehlen bis zur nativen Neubestimmung,
# siehe native/effect_particles.cpp.
# tools/generate_en01_damage.py ist Quarantaene (Policy 2026-10-09): Gegner-KI,
# Trefferreaktion, Tod, Greifen und das Sprengfass fehlen bis zur nativen
# Neubestimmung, siehe native/enemies/en01_damage.c. Damit entfaellt auch
# en01_damage_types.inc, weil es aus demselben Generator stammt.
# Retired engine/console component targets are quarantined, never build inputs.
# Only independently ported native mechanisms belong in the production graph.
# tools/generate_subplayer.py ist Quarantäne (Policy 2026-10-09): die
# Begleitfiguren-Steuerung fehlt bis zur nativen Neubestimmung, siehe
# native/subplayer.c.
# tools/generate_player_stairs.py ist Quarantäne (Policy 2026-10-09): die
# Treppen-/Leiter-/Stufenaktionen fehlen bis zur nativen Neubestimmung, siehe
# native/player_stairs.c.
# tools/generate_view_clip.py ist Quarantäne (Policy 2026-10-09): die Clip-
# Bereiche sind jetzt direkt aus event.c in native/view_clip.cpp übernommen.
# tools/generate_targeting.py ist Quarantäne (Policy 2026-10-09): die
# Zielaufschaltung und die Armwinkel fehlen bis zur nativen Neubestimmung,
# siehe native/targeting.cpp.
# tools/generate_combat_tables.py ist aus dem Build entfernt (Policy 2026-10-09, R02):
# ein Generator darf nicht bei jedem Build die PS2-Quellen lesen, auch nicht
# fuer reine Daten. Waffen-/Kampftabellen liegen als native/combat_tables_data.inc im Baum (byte-identisch geprueft).
# tools/generate_file_folder.py ist Quarantaene (Policy 2026-10-09): Ordner-
# auswahl, Register und die Handbuchseiten fehlen bis zur nativen
# Neubestimmung, siehe native/file_folder.cpp.
# tools/generate_inventory_engine.py ist Quarantäne (Policy 2026-10-09): der
# Untersuchungsautomat, die Sichtbarkeitsmasken, die Namensauswahl und die
# Dateitabellen fehlen bis zur nativen Neubestimmung, siehe
# native/inspection.cpp.
# tools/generate_effect_sprites.py ist Quarantaene (Policy 2026-10-09): alle
# bhEff*-Prozeduren, die ef_info-Tabelle, WpnEffTab und lgttab fehlen bis zur
# nativen Neubestimmung, siehe native/effect_sprites.cpp.
# tools/generate_pickup_engine.py ist Quarantäne (Policy 2026-10-09).
# generate_item_use.py liegt seit 2026-10-09 in archived/FORBIDDEN_DONTUSE/tools
# (work/forbidden_cleanup_manifest.json). GetItem/ItemSort/ItemSearch/EraseItem,
# bhSearchBullet/bhCountBullet und ItemUse/Use_00 sind eigenständig nativ
# bestimmt, siehe native/inventory.cpp.
# tools/generate_object_itmbox.py ist Quarantäne (Policy 2026-10-09):
# bhObjItmBox fehlt bis zur nativen Neubestimmung, siehe
# native/object_itmbox.cpp.
# tools/generate_event_inventory.py ist Quarantaene (Policy 2026-10-09): die
# Inventarmutationen der Eventbefehle fehlen bis zur nativen Neubestimmung,
# siehe native/events.cpp.
# tools/generate_computer.py ist Quarantaene (Policy 2026-10-09): der
# Terminal-Skriptinterpreter fehlt bis zur nativen Neubestimmung, siehe
# native/computer_core.c.
# tools/generate_box_engine.py ist Quarantäne (Policy 2026-10-09):
# ItemBoxChange fehlt bis zur nativen Neubestimmung, siehe
# native/inventory.cpp.
# tools/generate_enemy_spawn.py ist Quarantaene (Policy 2026-10-09): die
# Spawn-/Fallenaktivierung fehlt bis zur nativen Neubestimmung, siehe
# native/enemies/enemy_spawn.cpp.
# tools/generate_gameover.py ist Quarantaene (Policy 2026-10-09): Todesszene,
# Retry-Abfrage und Bildblende fehlen bis zur nativen Neubestimmung, siehe
# native/gameover.cpp.
# tools/generate_effect_collision.py ist Quarantaene (Policy 2026-10-09):
# ATR-Boden/Wandkontakt, Reflexion und Linienabfrage sind eigenstaendig nativ;
# der abwaerts getestete Zylinderdeckel bleibt eine belegte Semantikluecke.
add_library(recvx_game native/file_folder.cpp native/effect_collision.cpp native/hit_model.cpp native/gameover.cpp native/effect_particles.cpp native/computer.cpp native/computer_core.c native/computer_tables.c native/subplayer.c native/enemies/en01_damage.c native/enemies/enemy_spawn.cpp native/effect_sprites.cpp native/effect_sprite_procedures.cpp native/effect_rap_camera.cpp native/effect_rap_blood.cpp native/effect_rap_claw.cpp native/effect_eff30x.cpp native/effect_eff130.cpp native/effect_eff199.cpp native/effect_eff013.cpp native/effect_sub3x.cpp native/weapon_fly.c native/inspection.cpp native/combat.cpp native/weapon_flash.cpp native/targeting.cpp native/weapon_asset.cpp native/box_menu.cpp native/inventory.cpp native/messages.cpp native/ui.cpp native/events.cpp native/audio.cpp native/audio_iop.cpp native/movie.cpp native/world.cpp native/back_color.cpp native/collision.cpp native/door_core.c native/doors.cpp native/item_tables.c native/status_pulse.cpp native/snapshot.cpp native/view_clip.cpp native/map_screen.cpp native/map_tables.c native/map_objects.cpp native/player_stairs.c native/player_action.c native/player_event.c native/player_hair.c native/player_head.c native/adx_tables.c)
target_sources(recvx_game PRIVATE native/core_random.cpp native/object_itmbox.cpp native/world_itmbox.cpp native/world_water.cpp native/geometry_motion.cpp)
# D10 Teil B: Kartenbild (Ps2AddPrim2D/3D-Seite von map.c) und sys->fade_* (screen.c).
target_sources(recvx_game PRIVATE native/map_render.cpp native/screen_fade.cpp)
# Gemeinsame Benutzerpfade (user_paths), Schreibmaschinen-Slots (save_slots) und
# die Slotauswahl (typewriter). bup_00.c ControlTypewriter / ps2_SaveScreen.
target_sources(recvx_game PRIVATE native/user_paths.cpp native/save_slots.cpp native/typewriter.cpp)
# C02: gemeinsamer Gegnerwork am WorldEntity-Besitzer, native Dispatchkette,
# bhSetMotion/bhCalcModel des Besitzers und alle nativ vorhandenen Gegner-
# routinen. en54/en55 fehlen bis zum Port von en17.c (Kettenwerk-Besitzer).
target_sources(recvx_game PRIVATE native/world_enemies.cpp native/motion_set.cpp
  native/enemies/enemy_work.c native/enemies/enemy_dispatch.c
  native/enemies/en00.c native/enemies/en01_core.c native/enemies/en01_sub.c native/enemies/en01_tables.c native/enemies/en02.c native/enemies/en02_tables.c native/enemies/en04.c native/enemies/en04_tables.c native/enemies/en07.c native/enemies/en08.c native/enemies/en09.c native/enemies/en09_tables.c native/enemies/en09_arm_tables.c native/enemies/en10.c
  native/enemies/en11.c native/enemies/en12.c native/enemies/en13.c native/enemies/en02_trans_tables.c native/enemies/en14.c native/enemies/en23.c native/enemies/en03_spider.c native/enemies/en05.c native/enemies/en05_tables.c
  native/enemies/en16.c native/enemies/en18.c native/enemies/en20.c native/enemies/en21.c native/enemies/en24.c native/enemies/en25.c
  native/enemies/en53.c native/enemies/en56.c native/enemies/en71.c
  native/enemies/en06.c native/enemies/en22.c native/enemies/en26.c native/enemies/en27.c native/enemies/en30.c
  native/enemies/en19.c native/enemies/en19_tables.c native/enemies/en29.c
  native/enemies/en17.c native/enemies/en54.c native/enemies/en55.c
  native/enemies/en03_lib.c native/enemies/en03_collision.c native/enemies/en03.c native/enemies/en15.c)
if(NOT MSVC)
  # Preserve separate float multiply/add steps also in production-only builds.
  set_property(SOURCE native/effect_collision.cpp native/hit_model.cpp native/geometry_motion.cpp native/motion_set.cpp native/enemies/en00.c native/enemies/en01_core.c native/enemies/en01_tables.c native/enemies/en02.c native/enemies/en02_tables.c native/enemies/en04.c native/enemies/en04_tables.c native/enemies/en07.c native/enemies/en08.c native/enemies/en09.c native/enemies/en09_tables.c native/enemies/en09_arm_tables.c native/enemies/en10.c native/enemies/en11.c native/enemies/en12.c native/enemies/en13.c native/enemies/en14.c native/enemies/en23.c native/enemies/en03_spider.c native/enemies/en05.c native/enemies/en05_tables.c native/enemies/en16.c native/enemies/en18.c native/enemies/en20.c native/enemies/en21.c native/enemies/en24.c native/enemies/en25.c native/enemies/en53.c native/enemies/en56.c native/enemies/en71.c native/enemies/en06.c native/enemies/en22.c native/enemies/en26.c native/enemies/en27.c native/enemies/en30.c native/enemies/en19.c native/enemies/en19_tables.c native/enemies/en29.c native/enemies/en17.c native/enemies/en54.c native/enemies/en55.c native/enemies/en03_lib.c native/enemies/en03_collision.c native/enemies/en03.c native/enemies/en15.c APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off)
endif()
target_include_directories(recvx_game PRIVATE "${RECVX_TABLES}" build-tools/ninja)
target_compile_definitions(recvx_game PRIVATE RECVX_NATIVE=1)
target_link_libraries(recvx_game PUBLIC recvx_scene)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
add_library(recvx_data native/expand.cpp native/archive.cpp native/room.cpp native/asset_io.cpp)
# native/enemies: Gegnerroutinen (enNN*, enemy_*) liegen in eigenem
# Unterordner. Der Pfad wird mit "native" zusammen weitergegeben, damit die
# vorhandenen kurzen #include-Namen der Gegnerheader gueltig bleiben.
target_include_directories(recvx_data PUBLIC native native/enemies)
target_compile_features(recvx_data PUBLIC cxx_std_17)
add_library(recvx_math native/ninja_math.c native/ninja_matrix.c)
target_compile_definitions(recvx_math PRIVATE RECVX_NATIVE=1)
target_include_directories(recvx_math PUBLIC native)
target_compile_features(recvx_math PRIVATE c_std_99)
if(NOT MSVC)
  target_link_libraries(recvx_math PUBLIC m)
  target_compile_options(recvx_math PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
endif()
# Raumliste als eigene native Datenquelle (Policy 2026-10-09: keine Datei aus
# src/ps2/veronica/prog/ wird kompiliert). Reine Spieldaten, siehe room_table.c.
add_library(recvx_disc native/room_table.c)
target_include_directories(recvx_disc PUBLIC native)
target_include_directories(recvx_data PUBLIC include/ps2/veronica/prog)
target_link_libraries(recvx_data PUBLIC recvx_disc)
add_library(recvx_scene native/scene.cpp native/renderer.cpp native/actor.cpp native/morph.cpp native/face.cpp native/item_asset.cpp native/chunk_blend.cpp)
target_include_directories(recvx_scene PRIVATE "${RECVX_TABLES}")
find_package(Threads REQUIRED)
target_link_libraries(recvx_scene PUBLIC recvx_data recvx_math Threads::Threads)
# Host-GPU-Rasterer: Plattformstandard plus optionales OpenGL auf allen Desktopplattformen.
option(RECVX_ENABLE_OPENGL "Build the optional OpenGL 4.1 raster backend (Linux GPU default)" ON)
target_sources(recvx_scene PRIVATE native/gpu_raster_none.cpp)
if(RECVX_ENABLE_OPENGL)
  target_sources(recvx_scene PRIVATE native/gpu_raster_opengl.cpp)
  target_compile_definitions(recvx_scene PRIVATE RECVX_HAVE_OPENGL=1)
endif()
if(APPLE)
  # Als CXX-Quelle: clang waehlt Objective-C++ an der Endung; enable_language(OBJCXX) bricht SDLs .m-Build.
  set_source_files_properties(native/gpu_raster_metal.mm PROPERTIES LANGUAGE CXX COMPILE_OPTIONS "-fobjc-arc")
  target_sources(recvx_scene PRIVATE native/gpu_raster_metal.mm)
  target_link_libraries(recvx_scene PUBLIC "-framework Metal" "-framework Foundation")
elseif(WIN32)
  # Rasterizer Ordered Views (D3D11.3) als Gegenstueck zum programmierbaren Mischen; d3d11.dll und
  # d3dcompiler_47.dll werden zur Laufzeit geladen, ohne ROV bleibt der Softwarepfad.
  target_sources(recvx_scene PRIVATE native/gpu_raster_d3d11.cpp)
endif()
add_executable(recvx-render-benchmark EXCLUDE_FROM_ALL tools/benchmark_renderer.cpp)
target_link_libraries(recvx-render-benchmark PRIVATE recvx_scene)
# Host-GPU-Rasterer gegen Bandstufe (Pixelvergleich, Zeit je Bild); Rueckgabe 77 ohne GPU-Backend.
add_executable(recvx-gpu-compare EXCLUDE_FROM_ALL tools/gpu_compare.cpp)
target_link_libraries(recvx-gpu-compare PRIVATE recvx_scene)
if(MSVC)
  target_compile_options(recvx_scene PRIVATE /W4)
else()
  target_compile_options(recvx_scene PRIVATE -Wall -Wextra -Wpedantic)
  # Bandstufe ohne FMA-Kontraktion: dieselbe Rundung wie MSVC (/fp:precise) und der GPU-Rasterer.
  set_property(SOURCE native/renderer.cpp APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off)
endif()
option(RECVX_BUILD_RUNTIME "Build the native SDL window and room renderer" ON)
option(RECVX_FETCH_SDL2 "Download and build the pinned SDL2 dependency" OFF)
if(RECVX_BUILD_RUNTIME OR RECVX_ENABLE_OPENGL)
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
endif()
if(RECVX_ENABLE_OPENGL)
  target_link_libraries(recvx_scene PUBLIC SDL2::SDL2)
  if(WIN32)
    add_custom_command(TARGET recvx-gpu-compare POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:recvx-gpu-compare>)
  endif()
endif()
if(RECVX_BUILD_RUNTIME)
  add_executable(recvx-runtime native/runtime.cpp)
  # tools/build_native.py setzt major.minor.Commitzahl; ohne es gilt die Projektversion.
  set(RECVX_BUILD_VERSION "${PROJECT_VERSION}" CACHE STRING "Runtime version string")
  target_compile_definitions(recvx-runtime PRIVATE RECVX_BUILD_VERSION="${RECVX_BUILD_VERSION}")
  add_dependencies(recvx-runtime recvx-native-policy)
  add_custom_command(TARGET recvx-runtime POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_no_original_core.py"
      "${CMAKE_CURRENT_SOURCE_DIR}" --build-dir "${CMAKE_CURRENT_BINARY_DIR}" --binary "$<TARGET_FILE:recvx-runtime>"
    VERBATIM)
  add_library(recvx_input native/input.cpp native/frontend.cpp native/debug_menu.cpp native/third_person.cpp native/video_settings.cpp native/vibration.cpp)
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
    # RM_0090 Security Boxen obwp[5]/obwp[14]: Knotenschreiber der Ereignisse 7/18/19/30/31 und scd1.
    add_executable(recvx-rm0090-box-tests tests/native/rm0090_box_test.cpp)
    target_link_libraries(recvx-rm0090-box-tests PRIVATE recvx_input)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0090.RDT")
      add_test(NAME rm0090_security_box_nodes COMMAND recvx-rm0090-box-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
    endif()
    # Statusbildschirm-Taskrahmen (ui.h StatusTask): bhSysCallMonitor Modus 5, ItemTaskCheck/taskloop;
    # mit Daten zusaetzlich RM_0090 Ereignis 19 unter st_flg 0x8 (ohne RM_0090 nur der Rahmen).
    add_executable(recvx-status-screen-hold-tests tests/native/status_screen_hold_test.cpp)
    target_link_libraries(recvx-status-screen-hold-tests PRIVATE recvx_input)
    add_test(NAME status_screen_hold COMMAND recvx-status-screen-hold-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
    add_executable(recvx-room-motion-empty-slot-tests tests/native/room_motion_empty_slot_test.cpp)
    target_link_libraries(recvx-room-motion-empty-slot-tests PRIVATE recvx_input)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_3130.RDT")
      add_test(NAME room_motion_empty_rmthp_slot COMMAND recvx-room-motion-empty-slot-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
    endif()
    add_executable(recvx-input-tests tests/native/input_test.cpp)
    target_link_libraries(recvx-input-tests PRIVATE recvx_input)
    add_test(NAME keyboard_controller_config COMMAND recvx-input-tests)
    # message.c:269-295 / pad.c:275: real held direction versus newly pressed dialog bits.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0060.RDT")
      add_test(NAME message_dialog_pad_edges COMMAND recvx-input-tests --dialog-edges "${CMAKE_CURRENT_SOURCE_DIR}/data")
      add_test(NAME item_box_pad_frames COMMAND recvx-input-tests --box-pad "${CMAKE_CURRENT_SOURCE_DIR}/data")
      # sub1.c:3308 ItemTaskCheck: GetItem screen starts from StatusInit, no leftover MainCommand/Examine state.
      add_test(NAME pickup_status_reset COMMAND recvx-input-tests --pickup-reset "${CMAKE_CURRENT_SOURCE_DIR}/data")
      # sub1.c:3447 StatusMain subscreenmode 8: no CursorMove/MainCommand pad path on the GetItem screen,
      # also not in the pass between the answered question 157 and message 158.
      add_test(NAME pickup_gap_pad COMMAND recvx-input-tests --pickup-pad "${CMAKE_CURRENT_SOURCE_DIR}/data")
    endif()
  endif()
  # No original engine or historical console audit target is permitted.
  target_link_libraries(recvx-runtime PRIVATE recvx_input)
  if(WIN32)
    enable_language(RC)
    set(RECVX_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/native/resources/app-icon.ico")
    configure_file(native/resources/app-icon.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app-icon.rc" @ONLY)
    target_sources(recvx-runtime PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app-icon.rc")
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/app-icon.rc" PROPERTY OBJECT_DEPENDS "${RECVX_APP_ICON}")
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
  # This component test deliberately has no recvx_game/recvx_scene or original
  # engine archive dependency. Its generated input contains constants only.
  add_executable(recvx-weapon-flash-tests tests/native/weapon_flash_test.cpp
    native/weapon_flash.cpp native/core_random.cpp native/chunk_blend.cpp)
  target_include_directories(recvx-weapon-flash-tests PRIVATE native "${RECVX_TABLES}")
  target_compile_features(recvx-weapon-flash-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-weapon-flash-tests PRIVATE recvx_math)
  if(NOT MSVC)
    target_compile_options(recvx-weapon-flash-tests PRIVATE -ffp-contract=off)
  endif()
  add_test(NAME native_weapon_flash COMMAND recvx-weapon-flash-tests)
  add_executable(recvx-chunk-blend-tests tests/native/chunk_blend_test.cpp)
  target_link_libraries(recvx-chunk-blend-tests PRIVATE recvx_scene)
  add_test(NAME original_chunk_blend COMMAND recvx-chunk-blend-tests)
  add_executable(recvx-texture-clamp-tests tests/native/texture_clamp_test.cpp)
  target_link_libraries(recvx-texture-clamp-tests PRIVATE recvx_scene recvx_data)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_6050.RDT")
    add_test(NAME gs_texture_clamp COMMAND recvx-texture-clamp-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  else()
    add_test(NAME gs_texture_clamp COMMAND recvx-texture-clamp-tests)
  endif()
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
  add_executable(recvx-inventory-sort-tests tests/native/inventory_sort_test.cpp)
  target_link_libraries(recvx-inventory-sort-tests PRIVATE recvx_game)
  add_test(NAME inventory_item_sort COMMAND recvx-inventory-sort-tests)
  add_executable(recvx-save-slots-tests tests/native/save_slots_test.cpp)
  target_link_libraries(recvx-save-slots-tests PRIVATE recvx_game)
  add_test(NAME typewriter_save_slots COMMAND recvx-save-slots-tests)
  # Slotliste der Schreibmaschine: Loeschfarbe je Bild und einmaliges Fenstermischen
  # (bup_00.c:299-304, ps2_dummy.c:89, McSaveFile.c:554/1067).
  add_executable(recvx-typewriter-draw-tests tests/native/typewriter_draw_test.cpp)
  target_link_libraries(recvx-typewriter-draw-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/ITEM1/0144.bin")
    add_test(NAME typewriter_slot_list_draw COMMAND recvx-typewriter-draw-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # 2D-/UI-Farbkanaele: NJS_COLOR 0xAARRGGBB ist die Leinwandordnung (ps2_NaDraw2D.c:229-240);
  # Schreibmaschinen-Dateiname gm_mode 2 (McSaveFile.c:806), Farbcodes FE01..FE05 (message.c:161/401).
  add_executable(recvx-ui-color-tests tests/native/ui_color_test.cpp)
  target_link_libraries(recvx-ui-color-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/ITEM1/0144.bin")
    add_test(NAME ui_color_channels COMMAND recvx-ui-color-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # Ausruestungsflaeche im Kistenbildschirm: ArmsSet center 9 (sub1.c:4988-4996), pip[1]
  # nur in der Sicherheitskiste (ItemSet sub1.c:2453-2478).
  add_executable(recvx-item-box-equipment-tests tests/native/item_box_equipment_test.cpp)
  target_link_libraries(recvx-item-box-equipment-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/ITEM1/0144.bin")
    add_test(NAME item_box_equipment_slot COMMAND recvx-item-box-equipment-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-item-view-tests tests/native/item_view_test.cpp)
  target_link_libraries(recvx-item-view-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/ITEM1")
    add_test(NAME item_view_presentation COMMAND recvx-item-view-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  else()
    add_test(NAME item_view_presentation COMMAND recvx-item-view-tests)
  endif()
  add_executable(recvx-ui-quantity-tests tests/native/ui_quantity_test.cpp)
  target_link_libraries(recvx-ui-quantity-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/SYSTEM")
    add_test(NAME ui_quantity COMMAND recvx-ui-quantity-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  else()
    add_test(NAME ui_quantity COMMAND recvx-ui-quantity-tests)
  endif()
  add_executable(recvx-file-reader-tests tests/native/file_reader_test.cpp)
  target_link_libraries(recvx-file-reader-tests PRIVATE recvx_game)
  add_test(NAME original_file_reader COMMAND recvx-file-reader-tests)
  add_executable(recvx-door-tests tests/native/door_test.cpp)
  target_link_libraries(recvx-door-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/MRY")
    # With the cooked data the test plays all original door packets; without it
    # only the parameter layouts and the procedure binding are checked.
    add_test(NAME door_transition COMMAND recvx-door-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  else()
    add_test(NAME door_transition COMMAND recvx-door-tests)
  endif()
  add_executable(recvx-collision-tests tests/native/collision_test.cpp)
  add_executable(recvx-item-box-object-tests tests/native/item_box_object_test.cpp)
  target_link_libraries(recvx-item-box-object-tests PRIVATE recvx_game)
  add_test(NAME item_box_object COMMAND recvx-item-box-object-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  # C02: gemeinsamer Gegnerwork, bhSetMotion des Besitzers und Raumbindung
  # mit Originaldaten (RM_0130 fuenf ID-7-Works, RM_0080 Sprengfass).
  add_executable(recvx-enemy-work-tests tests/native/enemy_work_test.cpp)
  target_link_libraries(recvx-enemy-work-tests PRIVATE recvx_game)
  add_test(NAME enemy_work_contract COMMAND recvx-enemy-work-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-barrel-blast-tests tests/native/barrel_blast_test.cpp)
  target_link_libraries(recvx-barrel-blast-tests PRIVATE recvx_game)
  add_test(NAME native_barrel_blast COMMAND recvx-barrel-blast-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  # pwksub.c:244/weapon.c:614 Ziel-/Trefferfilter und pwksub.c:2172 bhCheckC2WallN (RM_0080).
  add_executable(recvx-combat-target-rules-tests tests/native/combat_target_rules_test.cpp)
  target_link_libraries(recvx-combat-target-rules-tests PRIVATE recvx_game)
  add_test(NAME combat_target_rules COMMAND recvx-combat-target-rules-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-effect-filter-tests tests/native/effect_filter_test.cpp)
  target_link_libraries(recvx-effect-filter-tests PRIVATE recvx_game)
  add_test(NAME effect_camera_filter COMMAND recvx-effect-filter-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-crane-ufo-tests tests/native/crane_ufo_test.cpp)
  target_link_libraries(recvx-crane-ufo-tests PRIVATE recvx_game)
  add_test(NAME crane_ufo_event COMMAND recvx-crane-ufo-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  target_link_libraries(recvx-collision-tests PRIVATE recvx_game)
  add_test(NAME wall_collision COMMAND recvx-collision-tests)
  add_executable(recvx-line-reflection-tests tests/native/line_reflection_test.cpp)
  target_link_libraries(recvx-line-reflection-tests PRIVATE recvx_game)
  if(NOT MSVC)
    target_compile_options(recvx-line-reflection-tests PRIVATE -ffp-contract=off)
  endif()
  add_test(NAME native_line_reflection COMMAND recvx-line-reflection-tests)
  add_executable(recvx-motion-collision-tests tests/native/motion_collision_test.cpp)
  target_link_libraries(recvx-motion-collision-tests PRIVATE recvx_game)
  if(NOT MSVC)
    target_compile_options(recvx-motion-collision-tests PRIVATE -ffp-contract=off)
  endif()
  add_test(NAME native_motion_collision COMMAND recvx-motion-collision-tests)
  add_executable(recvx-back-color-tests tests/native/back_color_test.cpp)
  target_link_libraries(recvx-back-color-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_7070.RDT")
    add_test(NAME original_back_color COMMAND recvx-back-color-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-ground-world-tests tests/native/ground_world_test.cpp)
  target_link_libraries(recvx-ground-world-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0050.RDT")
    add_test(NAME native_ground_world COMMAND recvx-ground-world-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # player.c:1365-1369/:1578-1655 Koerper-bhCheckWallEx mit plp->pxb an RM_0050 ATR-Wand 25.
  add_executable(recvx-body-wall-pass-tests tests/native/body_wall_pass_test.cpp)
  target_link_libraries(recvx-body-wall-pass-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0050.RDT")
    add_test(NAME native_body_wall_pass COMMAND recvx-body-wall-pass-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # event.c:12038-12065 / 12452-12490: Common_controll 7 setzt pxb mit, 10 nicht (RM_0080 ene[0], Spieler).
  add_executable(recvx-common10-pxb-tests tests/native/common10_pxb_test.cpp)
  target_link_libraries(recvx-common10-pxb-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0080.RDT")
    add_test(NAME native_common10_pxb COMMAND recvx-common10-pxb-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # effsub1b.c:1258-2121 bhEff138/bhDraw138 (RM_8001 Treibstoffumfuellung) auf RoomWorld.
  add_executable(recvx-oil-puzzle-world-tests tests/native/oil_puzzle_world_test.cpp)
  target_link_libraries(recvx-oil-puzzle-world-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_8001.RDT")
    add_test(NAME native_oil_puzzle_world COMMAND recvx-oil-puzzle-world-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # game.c:318-403 / objitm.c:497-550: sys->pt_flg als Zeichentor an RoomWorld::append (Raummodell 0x20,
  # Objektlisten 0x4, Raumitems 0x8); RM_8001 bhEff138 setzt pt_flg 0x10 (effsub1b.c:1641-1645).
  add_executable(recvx-presentation-gates-tests tests/native/presentation_gates_test.cpp)
  target_link_libraries(recvx-presentation-gates-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_8001.RDT")
    add_test(NAME native_presentation_gates COMMAND recvx-presentation-gates-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  if(TARGET recvx-runtime AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0100.RDT")
    add_test(NAME box_jump_landing COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/box_jump_landing_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  if(TARGET recvx-runtime AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_3060.RDT")
    add_test(NAME rm3060_water_action_se COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/rm3060_water_action_se_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # sync.c:99-137 bhControlEOR nach Task 7/8 (main.c:196, ps2_NaSystem.c:67-86): bhBGColorSet mit
  # bcl_ct 0 (event.c:7206) loescht schon das erste Bild der neuen Ereigniskamera (RM_1140 Hebel).
  if(TARGET recvx-runtime AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_1140.RDT")
    add_test(NAME rm1140_lever_back_color COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/rm1140_lever_back_color_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
    # player.c:1365: bhControlPlayerHead (:1683) und bhCalcModel(plp) (:1688) nur im Passtor sp_flg 0x1
    # und ohne plp->stflg 0x1000000 (Meldung im RM_1140-Hebelreplay: sp_flg 0xb8).
    add_test(NAME player_head_pass_gate COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/player_head_pass_gate_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
    # game.c:48-51: bhControlEffect nach bhControlPlayer/bhCalcModel(plp) (player.c:1688, effect.c:800-812).
    add_test(NAME effect_after_player COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/effect_after_player_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
    # main.c:195-201, game.c:58/289: erstes Bild nach bhCamSet 0 in der rohen bhSetEventCamera-Lage.
    add_test(NAME event_camera_first_frame COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/event_camera_first_frame_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # bhJumpEnemy[26] = bhEne26 (en26.c:628): RM_0110 Ereigniszombie (Fressszene, Szene, Angriff,
  # Init beim Wiederbetreten, Tod und bhEtcAtariEnePosSet des Schluesselgegenstands).
  if(TARGET recvx-runtime AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0110.RDT")
    add_test(NAME rm0110_event_zombie COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/rm0110_event_zombie_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # weapon.c bhCheckEnemies -> bhEne19_DmgCheck (en19.c:3193): Sofortwaffe trifft den nativen Tyrant-Work,
  # hp -= EneDam{Near,Mid,Far}[19][6] je Treffer (Trefferzuweisung A04 am EN19-Work).
  if(TARGET recvx-runtime AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_5840.RDT")
    add_test(NAME rm5840_tyrant_gun COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/rm5840_tyrant_gun_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # en03.c:6825 plp->mnwP = epw->mnwP im Griff (Dienst player_library vor dem Zurueckschreiben der Spielersicht).
  if(TARGET recvx-runtime AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_7250.RDT")
    add_test(NAME rm7250_spider_grab COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/rm7250_spider_grab_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  if(TARGET recvx-runtime AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_1090.RDT")
    add_test(NAME rm1090_terminal_wrong_password COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/rm1090_terminal_wrong_password_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  if(TARGET recvx-runtime AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0080.RDT")
    add_test(NAME zombie_blood_pool COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/zombie_blood_pool_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  if(TARGET recvx-runtime AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_1000.RDT")
    add_test(NAME title_and_room_entry_camera COMMAND "${Python3_EXECUTABLE}" -I
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/room_entry_camera_check.py"
      --runtime "$<TARGET_FILE:recvx-runtime>" --data "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-audio-sq-tests tests/native/audio_sq_test.cpp)
  target_link_libraries(recvx-audio-sq-tests PRIVATE recvx_game)
  add_test(NAME sony_sq_dictionary COMMAND recvx-audio-sq-tests)
  add_executable(recvx-audio-tests tests/native/audio_movie_test.cpp)
  target_link_libraries(recvx-audio-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/files/COMMON.MLT")
    add_test(NAME audio_movie COMMAND recvx-audio-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  else()
    add_test(NAME audio_movie COMMAND recvx-audio-tests)
  endif()
  # sdfunc.c Set3dSoundFlag/SetUserSoundVolume 6-8, Gegner-SE-Fade, VolDownTbl-Rand, CallDoorSe-Vorgaben.
  add_executable(recvx-audio-gaps-tests tests/native/audio_gaps_test.cpp)
  target_link_libraries(recvx-audio-gaps-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/files/COMMON.MLT")
    add_test(NAME audio_gaps COMMAND recvx-audio-gaps-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-audio-levels-tests tests/native/audio_levels_test.cpp)
  target_link_libraries(recvx-audio-levels-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/files/COMMON.MLT")
    add_test(NAME audio_levels COMMAND recvx-audio-levels-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-bgm-room-tests tests/native/bgm_room_test.cpp)
  target_link_libraries(recvx-bgm-room-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0050.RDT")
    add_test(NAME bgm_room_transition COMMAND recvx-bgm-room-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-computer-tests tests/native/computer_test.cpp)
  target_link_libraries(recvx-computer-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0020.RDT")
    add_test(NAME computer_terminal COMMAND recvx-computer-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  else()
    add_test(NAME computer_terminal COMMAND recvx-computer-tests)
  endif()
  add_executable(recvx-ammunition-tests tests/native/ammunition_test.cpp)
  target_link_libraries(recvx-ammunition-tests PRIVATE recvx_game)
  add_test(NAME ammunition_pickup_zero COMMAND recvx-ammunition-tests)
  add_executable(recvx-combat-tests tests/native/combat_test.cpp)
  target_link_libraries(recvx-combat-tests PRIVATE recvx_game)
  add_test(NAME combat_weapons_ammunition COMMAND recvx-combat-tests)
  add_executable(recvx-effect-debris-tests tests/native/effect_debris_test.cpp)
  target_link_libraries(recvx-effect-debris-tests PRIVATE recvx_game)
  add_test(NAME effect_collision_debris COMMAND recvx-effect-debris-tests)
  add_executable(recvx-effect-e03-acid-tests tests/native/effect_e03_acid_test.cpp)
  target_link_libraries(recvx-effect-e03-acid-tests PRIVATE recvx_game)
  add_test(NAME effect_e03_acid COMMAND recvx-effect-e03-acid-tests)
  add_executable(recvx-effect-magazine-tests tests/native/effect_magazine_test.cpp)
  target_link_libraries(recvx-effect-magazine-tests PRIVATE recvx_game)
  add_test(NAME effect_magazine COMMAND recvx-effect-magazine-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-effect-e00-fire-tests tests/native/effect_e00_fire_test.cpp)
  target_include_directories(recvx-effect-e00-fire-tests PRIVATE native native/enemies)
  target_compile_features(recvx-effect-e00-fire-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-effect-e00-fire-tests PRIVATE recvx_game recvx_math)
  add_test(NAME effect_e00_fire COMMAND recvx-effect-e00-fire-tests)
  add_executable(recvx-effect-eff350-tests tests/native/effect_eff350_test.cpp)
  target_link_libraries(recvx-effect-eff350-tests PRIVATE recvx_game)
  add_test(NAME effect_eff350 COMMAND recvx-effect-eff350-tests)
  add_executable(recvx-effect-eff301-tests tests/native/effect_eff301_test.cpp)
  target_link_libraries(recvx-effect-eff301-tests PRIVATE recvx_game)
  add_test(NAME effect_eff301 COMMAND recvx-effect-eff301-tests)
  add_executable(recvx-effect-e14-fire-tests tests/native/effect_e14_fire_test.cpp)
  target_link_libraries(recvx-effect-e14-fire-tests PRIVATE recvx_game)
  add_test(NAME effect_e14_fire COMMAND recvx-effect-e14-fire-tests)
  add_executable(recvx-effect-room-screen-tests tests/native/effect_room_screen_test.cpp)
  target_link_libraries(recvx-effect-room-screen-tests PRIVATE recvx_game)
  add_test(NAME effect_room_screen COMMAND recvx-effect-room-screen-tests ${CMAKE_CURRENT_SOURCE_DIR}/data)
  add_executable(recvx-mirror-pass-tests tests/native/mirror_pass_test.cpp)
  target_link_libraries(recvx-mirror-pass-tests PRIVATE recvx_game)
  add_test(NAME mirror_pass COMMAND recvx-mirror-pass-tests ${CMAKE_CURRENT_SOURCE_DIR}/data)
  add_executable(recvx-effect-eff085-tests tests/native/effect_eff085_test.cpp)
  target_link_libraries(recvx-effect-eff085-tests PRIVATE recvx_game)
  add_test(NAME effect_eff085 COMMAND recvx-effect-eff085-tests)
  add_executable(recvx-effect-eff2d-tests tests/native/effect_eff2d_test.cpp)
  target_link_libraries(recvx-effect-eff2d-tests PRIVATE recvx_game)
  add_test(NAME effect_eff2d COMMAND recvx-effect-eff2d-tests)
  add_executable(recvx-effect-pool-tests tests/native/effect_pool_test.cpp)
  target_link_libraries(recvx-effect-pool-tests PRIVATE recvx_game)
  add_test(NAME effect_pool_lifetime COMMAND recvx-effect-pool-tests)
  # Der positive Gegenstueck dazu: die Teilchenarbeiten gegen die
  # Originaltabellen PtclTbl/PtclDat und Eff302Prm. Committet (82f3c58), aber
  # bisher ohne Ziel, also nie gebaut und nie gelaufen.
  add_executable(recvx-effect-particles-tests tests/native/effect_particles_test.cpp)
  target_link_libraries(recvx-effect-particles-tests PRIVATE recvx_game)
  add_test(NAME original_effect_particles COMMAND recvx-effect-particles-tests)
  # D10 Kartenbildschirm: Datentabellen gegen map.c und der Taskautomat.
  add_executable(recvx-map-tests tests/native/map_screen_test.cpp)
  target_link_libraries(recvx-map-tests PRIVATE recvx_game)
  add_test(NAME original_map_screen COMMAND recvx-map-tests "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/data")
  # D10 Teil B: Bildschirmfade, 2D->Tiefenpuffer-Abbildung und das gerenderte Kartenbild an den Originaldaten.
  add_executable(recvx-map-render-tests tests/native/map_render_test.cpp)
  target_link_libraries(recvx-map-render-tests PRIVATE recvx_game)
  add_test(NAME original_map_render COMMAND recvx-map-render-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-effect-rap-camera-tests tests/native/effect_rap_camera_test.cpp)
  target_link_libraries(recvx-effect-rap-camera-tests PRIVATE recvx_game)
  add_test(NAME effect_rap_camera COMMAND recvx-effect-rap-camera-tests)
  add_executable(recvx-effect-rap-blood-tests tests/native/effect_rap_blood_test.cpp)
  target_link_libraries(recvx-effect-rap-blood-tests PRIVATE recvx_game)
  add_test(NAME effect_rap_blood COMMAND recvx-effect-rap-blood-tests)
  add_executable(recvx-effect-rap-claw-tests tests/native/effect_rap_claw_test.cpp)
  target_link_libraries(recvx-effect-rap-claw-tests PRIVATE recvx_game)
  add_test(NAME effect_rap_claw COMMAND recvx-effect-rap-claw-tests)
  add_executable(recvx-claw-model-light-tests tests/native/claw_model_light_test.cpp)
  target_link_libraries(recvx-claw-model-light-tests PRIVATE recvx_game)
  add_test(NAME claw_model_light COMMAND recvx-claw-model-light-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-effect-eff30x-tests tests/native/effect_eff30x_test.cpp)
  target_link_libraries(recvx-effect-eff30x-tests PRIVATE recvx_game)
  add_test(NAME effect_eff30x COMMAND recvx-effect-eff30x-tests)
  add_executable(recvx-effect-eff130-tests tests/native/effect_eff130_test.cpp)
  target_link_libraries(recvx-effect-eff130-tests PRIVATE recvx_game)
  add_test(NAME effect_eff130 COMMAND recvx-effect-eff130-tests)
  add_executable(recvx-effect-eff199-tests tests/native/effect_eff199_test.cpp)
  target_link_libraries(recvx-effect-eff199-tests PRIVATE recvx_game)
  add_test(NAME effect_eff199 COMMAND recvx-effect-eff199-tests)
  add_executable(recvx-effect-sub3x-tests tests/native/effect_sub3x_test.cpp)
  target_link_libraries(recvx-effect-sub3x-tests PRIVATE recvx_game)
  add_test(NAME effect_sub3x COMMAND recvx-effect-sub3x-tests)
  add_executable(recvx-effect-eff006-tests tests/native/effect_eff006_test.cpp)
  target_link_libraries(recvx-effect-eff006-tests PRIVATE recvx_game)
  add_test(NAME effect_eff006 COMMAND recvx-effect-eff006-tests)
  add_executable(recvx-script-effect-opcodes-tests tests/native/script_effect_opcodes_test.cpp)
  target_link_libraries(recvx-script-effect-opcodes-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_3130.RDT")
    add_test(NAME script_effect_opcodes COMMAND recvx-script-effect-opcodes-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-effect-room-tests tests/native/effect_room_sprites_test.cpp)
  target_link_libraries(recvx-effect-room-tests PRIVATE recvx_game)
  add_test(NAME effect_room_sprites COMMAND recvx-effect-room-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  # objitm.c:562/659/688: Schnitt-Beschraenkung der Raumobjekte/-items
  # (O_WRK.flg 0x40000000 und hide[0..3] aus dem ETTY_WORK).
  add_executable(recvx-object-cut-tests tests/native/object_cut_test.cpp)
  target_link_libraries(recvx-object-cut-tests PRIVATE recvx_game)
  add_test(NAME object_cut_restriction COMMAND recvx-object-cut-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  # objitm.c:496-618 bhDrawObjItm: Itemglanz (mdflg 0x80), Zaehlertor stflg 0x1000000, Nachwirkung von Licht 1
  # auf bhDrawEffect, Monitor-Einzellicht (screen.c:965-981) und ob_spc-Einzellicht unter gm_flg 0x80.
  add_executable(recvx-item-draw-light-tests tests/native/item_draw_light_test.cpp)
  target_link_libraries(recvx-item-draw-light-tests PRIVATE recvx_game)
  add_test(NAME item_draw_light COMMAND recvx-item-draw-light-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-lighter-tests tests/native/lighter_test.cpp)
  target_link_libraries(recvx-lighter-tests PRIVATE recvx_game)
  add_test(NAME native_lighter_effect COMMAND recvx-lighter-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  # light.c:486-657 Lichttypen 7..12 auf dem Raumlichtbesitzer (Formeln, Zufall, w*-Tor, Raumdaten).
  add_executable(recvx-light-types-tests tests/native/light_types_test.cpp)
  target_link_libraries(recvx-light-types-tests PRIVATE recvx_game)
  add_test(NAME light_types_7_12 COMMAND recvx-light-types-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-light-clip-tests tests/native/light_clip_test.cpp)
  target_link_libraries(recvx-light-clip-tests PRIVATE recvx_game)
  add_test(NAME light_clip COMMAND recvx-light-clip-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-easy-dir-light-tests tests/native/easy_dir_light_test.cpp)
  target_link_libraries(recvx-easy-dir-light-tests PRIVATE recvx_game)
  add_test(NAME easy_dir_light COMMAND recvx-easy-dir-light-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-light-effect-link-tests tests/native/light_effect_link_test.cpp)
  target_link_libraries(recvx-light-effect-link-tests PRIVATE recvx_game)
  add_test(NAME light_effect_link COMMAND recvx-light-effect-link-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-gameover-tests tests/native/gameover_test.cpp)
  target_link_libraries(recvx-gameover-tests PRIVATE recvx_game)
  add_test(NAME gameover_original COMMAND recvx-gameover-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  # screen.c sys->fade_*: ein prozessweiter Besitzer fuer alle Aufrufer (Status, Karte, Tuer,
  # Schreibmaschine, Game Over, bhFadeSet, Effekte, Spielstart); Folgen gegen die Originalzeilen.
  add_executable(recvx-screen-fade-owner-tests tests/native/screen_fade_owner_test.cpp)
  target_link_libraries(recvx-screen-fade-owner-tests PRIVATE recvx_game)
  add_test(NAME screen_fade_owner COMMAND recvx-screen-fade-owner-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-enemy-damage-tests tests/native/enemy_damage_test.cpp)
  target_link_libraries(recvx-enemy-damage-tests PRIVATE recvx_game)
  add_test(NAME enemy_damage_original_en01 COMMAND recvx-enemy-damage-tests)
  # Der Test zu native/subplayer.c stand bisher in keinem Ziel und lief nie
  # (tools/check_build_coverage.py). Er prueft beide ct2-Tabellen, alle elf
  # Bewegungszweige an ihrer Feldsignatur und die Bereichsgrenzen.
  # Wie recvx-weapon-flash-tests bewusst OHNE recvx_game/recvx_scene: der
  # Apparat braucht nur bhSetMotion/bhCalcModel als Zeiger (setzt der Test
  # selbst) und njSin/njCos. Aus enemy_work.h kommen nur Typen, keine Koerper.
  add_executable(recvx-subplayer-tests tests/native/subplayer_core_test.cpp native/subplayer.c native/enemies/enemy_work.c native/motion_set.cpp)
  target_include_directories(recvx-subplayer-tests PRIVATE native native/enemies)
  target_compile_features(recvx-subplayer-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-subplayer-tests PRIVATE recvx_math)
  add_test(NAME subplayer_apparatus COMMAND recvx-subplayer-tests)
  # Auch tests/native/enemy_spawn_test.cpp stand in keinem Ziel und lief nie.
  # Es prueft die Fallenaktivierung gegen zonzon.c (bhEne_EnemyAtariCheck,
  # bhEne_PosCheck) und en04.c (MVType01/MVType02/AtariCheck*) an echten
  # Nachschlagen: Feldaenderung, Bereichsverbrauch, Kontaktbit, Raumflag und
  # RNG-Fortschritt. native/enemies/enemy_spawn.cpp haengt nur an ninja_math, deshalb
  # wie oben isoliert und ohne recvx_game.
  add_executable(recvx-enemy-spawn-tests tests/native/enemy_spawn_test.cpp native/enemies/enemy_spawn.cpp)
  target_include_directories(recvx-enemy-spawn-tests PRIVATE native native/enemies)
  target_compile_features(recvx-enemy-spawn-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-enemy-spawn-tests PRIVATE recvx_math)
  add_test(NAME enemy_spawn_activation COMMAND recvx-enemy-spawn-tests)
  add_executable(recvx-c07-tests tests/native/enemy_c07_group_test.cpp)
  target_include_directories(recvx-c07-tests PRIVATE native native/enemies)
  target_compile_features(recvx-c07-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-c07-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_c07_group COMMAND recvx-c07-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  else()
    add_test(NAME enemy_c07_group COMMAND recvx-c07-tests)
  endif()
  add_executable(recvx-en07-tests tests/native/en07_test.cpp)
  target_include_directories(recvx-en07-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en07-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en07-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en07_component COMMAND recvx-en07-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  else()
    add_test(NAME enemy_en07_component COMMAND recvx-en07-tests)
  endif()
  add_executable(recvx-en04-tests tests/native/enemy_en04_test.cpp)
  target_include_directories(recvx-en04-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en04-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en04-tests PRIVATE recvx_game recvx_math)
  add_test(NAME enemy_en04_component COMMAND recvx-en04-tests)
  add_executable(recvx-en11-tests tests/native/en11_world_test.cpp)
  target_include_directories(recvx-en11-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en11-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en11-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en11_world COMMAND recvx-en11-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en02-tests tests/native/en02_world_test.cpp)
  target_include_directories(recvx-en02-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en02-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en02-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en02_world COMMAND recvx-en02-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-effect-model-mirror-tests tests/native/effect_model_mirror_test.cpp)
  target_include_directories(recvx-effect-model-mirror-tests PRIVATE native native/enemies)
  target_compile_features(recvx-effect-model-mirror-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-effect-model-mirror-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME effect_model_mirror COMMAND recvx-effect-model-mirror-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-effect-ry-shadow-tests tests/native/effect_ry_shadow_test.cpp)
  target_include_directories(recvx-effect-ry-shadow-tests PRIVATE native native/enemies)
  target_compile_features(recvx-effect-ry-shadow-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-effect-ry-shadow-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME effect_ry_shadow COMMAND recvx-effect-ry-shadow-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en29-tests tests/native/en29_world_test.cpp)
  target_include_directories(recvx-en29-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en29-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en29-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en29_world COMMAND recvx-en29-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # bhJumpEnemy[6] = bhEne06 (en06.c:229): RM_6030 Motten, Fluegelfragmente bhEne06s, Griff und Mottenei.
  add_executable(recvx-en06-tests tests/native/en06_world_test.cpp)
  target_include_directories(recvx-en06-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en06-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en06-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en06_world COMMAND recvx-en06-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # effsub4.c:1189/1667 bhEff_Sub356/363: Zombie-Kopf-/Beinexplosion (en01.c:10967/11012).
  add_executable(recvx-en01-exp-tests tests/native/en01_exp_effect_test.cpp)
  target_include_directories(recvx-en01-exp-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en01-exp-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en01-exp-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME effect_en01_exp COMMAND recvx-en01-exp-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # bhJumpEnemy[27] = bhEne27 (en27.c:41): Mottenei auf plp->mdl[6]/[7], bhCheckMothEgg (player.c:1042).
  add_executable(recvx-en27-tests tests/native/en27_world_test.cpp)
  target_include_directories(recvx-en27-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en27-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en27-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en27_world COMMAND recvx-en27-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # bhJumpEnemy[22] = bhEne22 (en22.c:512): RM_7100 Albinoid Adult, Blitzeffekte 353/354/364 und rom->lgtp[2].
  add_executable(recvx-en22-tests tests/native/en22_world_test.cpp)
  target_include_directories(recvx-en22-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en22-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en22-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en22_world COMMAND recvx-en22-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # bhJumpEnemy[21] = bhEne21 (en21.c:88): RM_3120 Typ 0 / RM_3140 Typ 1, Griff, Funken 399, Spur 396, Blutpfuetze.
  add_executable(recvx-en21-world-tests tests/native/en21_world_test.cpp)
  target_include_directories(recvx-en21-world-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en21-world-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en21-world-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en21_world COMMAND recvx-en21-world-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en03-tests tests/native/en03_world_test.cpp)
  target_include_directories(recvx-en03-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en03-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en03-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en03_world COMMAND recvx-en03-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en15-tests tests/native/en15_world_test.cpp)
  target_include_directories(recvx-en15-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en15-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en15-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en15_world COMMAND recvx-en15-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # objitm.c:2050 bhObj010, effsub1.c:8011 bhEff120, event.c:1965 bhEffectSensyaSet, OT-CLAMP-Route.
  add_executable(recvx-uv-scroll-tests tests/native/uv_scroll_test.cpp)
  target_link_libraries(recvx-uv-scroll-tests PRIVATE recvx_game)
  target_compile_definitions(recvx-uv-scroll-tests PRIVATE UV_PORT_AFTER=1)
  foreach(uv_case primitive effect route dispatch object5600 object6050 tank3020 tank7000)
    add_test(NAME uv_scroll_${uv_case} COMMAND recvx-uv-scroll-tests "${CMAKE_CURRENT_SOURCE_DIR}/data" ${uv_case})
  endforeach()
  add_executable(recvx-water-wave-tests tests/native/water_wave_test.cpp)
  target_include_directories(recvx-water-wave-tests PRIVATE native native/enemies)
  target_compile_features(recvx-water-wave-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-water-wave-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME object_water_wave COMMAND recvx-water-wave-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-floor-table-tests tests/native/floor_table_test.cpp)
  target_include_directories(recvx-floor-table-tests PRIVATE native native/enemies)
  target_compile_features(recvx-floor-table-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-floor-table-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME world_floor_table COMMAND recvx-floor-table-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-combat-player-work-tests tests/native/combat_player_work_test.cpp)
  target_include_directories(recvx-combat-player-work-tests PRIVATE native native/enemies)
  target_compile_features(recvx-combat-player-work-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-combat-player-work-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME combat_player_work COMMAND recvx-combat-player-work-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # event.c:7378 bhPlyHandChange (0xAF): Handobjekte RM_9110 und Waffenmodellbesitzer.
  add_executable(recvx-player-hand-change-tests tests/native/player_hand_change_test.cpp)
  target_include_directories(recvx-player-hand-change-tests PRIVATE native native/enemies)
  target_compile_features(recvx-player-hand-change-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-player-hand-change-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME player_hand_change COMMAND recvx-player-hand-change-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # plp->wpnr_no getrennt von pip[0]: WeaponSet, 0xC1, 0x27, bhSv 18/22, 0x26, Spielerwechsel, Sichern/Laden.
  add_executable(recvx-player-weapon-number-tests tests/native/player_weapon_number_test.cpp)
  target_include_directories(recvx-player-weapon-number-tests PRIVATE native native/enemies)
  target_compile_features(recvx-player-weapon-number-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-player-weapon-number-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME player_weapon_number COMMAND recvx-player-weapon-number-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # event.c:1986 bhEffectKokuenSet (0x97): Lage aus objP[0].pos des Taskworks.
  add_executable(recvx-event-kokuen-tests tests/native/event_kokuen_test.cpp)
  target_include_directories(recvx-event-kokuen-tests PRIVATE native native/enemies)
  target_compile_features(recvx-event-kokuen-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-event-kokuen-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME event_kokuen COMMAND recvx-event-kokuen-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # event.c:5263 bhObjLinkSetObjPly (0x6F): Spielerwurzel an einem Raumobjekt (RM_4011).
  add_executable(recvx-player-object-link-tests tests/native/player_object_link_test.cpp)
  target_include_directories(recvx-player-object-link-tests PRIVATE native native/enemies)
  target_compile_features(recvx-player-object-link-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-player-object-link-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME player_object_link COMMAND recvx-player-object-link-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  # Raumobjekte: objP nur ueber bhSetMotion (Common v0 0/2/5/6, bhSetObjMotion); RM_0090-Box.
  add_executable(recvx-object-motion-commit-tests tests/native/object_motion_commit_test.cpp)
  target_include_directories(recvx-object-motion-commit-tests PRIVATE native native/enemies)
  target_compile_features(recvx-object-motion-commit-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-object-motion-commit-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME object_motion_commit COMMAND recvx-object-motion-commit-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-effect-eff013-tests tests/native/effect_eff013_test.cpp)
  target_include_directories(recvx-effect-eff013-tests PRIVATE native native/enemies)
  target_compile_features(recvx-effect-eff013-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-effect-eff013-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME effect_eff013_world COMMAND recvx-effect-eff013-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-player-motion-blend-tests tests/native/player_motion_blend_test.cpp)
  target_include_directories(recvx-player-motion-blend-tests PRIVATE native native/enemies)
  target_compile_features(recvx-player-motion-blend-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-player-motion-blend-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME player_motion_blend COMMAND recvx-player-motion-blend-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-camera-follow-tests tests/native/camera_follow_target_test.cpp)
  target_include_directories(recvx-camera-follow-tests PRIVATE native native/enemies)
  target_compile_features(recvx-camera-follow-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-camera-follow-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME camera_follow_target COMMAND recvx-camera-follow-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en17-tests tests/native/en17_world_test.cpp)
  target_include_directories(recvx-en17-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en17-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en17-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en17_world COMMAND recvx-en17-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en19-tests tests/native/en19_world_test.cpp)
  target_include_directories(recvx-en19-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en19-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en19-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en19_world COMMAND recvx-en19-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-camera-offset-tests tests/native/camera_offset_test.cpp)
  target_include_directories(recvx-camera-offset-tests PRIVATE native native/enemies)
  target_compile_features(recvx-camera-offset-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-camera-offset-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME camera_offset COMMAND recvx-camera-offset-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-player-owp-order-tests tests/native/player_owp_order_test.cpp)
  target_include_directories(recvx-player-owp-order-tests PRIVATE native native/enemies)
  target_compile_features(recvx-player-owp-order-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-player-owp-order-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME player_owp_order COMMAND recvx-player-owp-order-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en53-tests tests/native/en53_test.cpp)
  target_include_directories(recvx-en53-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en53-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en53-tests PRIVATE recvx_game recvx_math)
  add_test(NAME enemy_en53_component COMMAND recvx-en53-tests)
  add_executable(recvx-en53-world-tests tests/native/en53_world_test.cpp)
  target_include_directories(recvx-en53-world-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en53-world-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en53-world-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en53_world COMMAND recvx-en53-world-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en09-tests tests/native/en09_world_test.cpp)
  target_include_directories(recvx-en09-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en09-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en09-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en09_world COMMAND recvx-en09-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en13-tests tests/native/en13_world_test.cpp)
  target_include_directories(recvx-en13-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en13-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en13-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en13_world COMMAND recvx-en13-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en14-tests tests/native/en14_world_test.cpp)
  target_include_directories(recvx-en14-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en14-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en14-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en14_world COMMAND recvx-en14-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-enemy-joint-mapping-tests tests/native/enemy_joint_mapping_test.cpp)
  target_include_directories(recvx-enemy-joint-mapping-tests PRIVATE native native/enemies)
  target_compile_features(recvx-enemy-joint-mapping-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-enemy-joint-mapping-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_joint_mapping COMMAND recvx-enemy-joint-mapping-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en23-tests tests/native/en23_world_test.cpp)
  target_include_directories(recvx-en23-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en23-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en23-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en23_world COMMAND recvx-en23-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en05-tests tests/native/en05_world_test.cpp)
  target_include_directories(recvx-en05-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en05-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en05-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_en05_world COMMAND recvx-en05-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-enemy-beam-tests tests/native/enemy_beam_test.cpp)
  target_include_directories(recvx-enemy-beam-tests PRIVATE native native/enemies)
  target_compile_features(recvx-enemy-beam-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-enemy-beam-tests PRIVATE recvx_game recvx_math)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms")
    add_test(NAME enemy_beam_effects COMMAND recvx-enemy-beam-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-en21-tests tests/native/en21_test.cpp)
  target_include_directories(recvx-en21-tests PRIVATE native native/enemies)
  target_compile_features(recvx-en21-tests PRIVATE cxx_std_17)
  target_link_libraries(recvx-en21-tests PRIVATE recvx_game recvx_math)
  add_test(NAME enemy_en21_component COMMAND recvx-en21-tests)
  # EN01-Kindfamilie (en01sub.c): Komponentenpruefung mit Testaufstellungen.
  add_executable(recvx-en01-sub-tests tests/native/en01_sub_test.c)
  target_include_directories(recvx-en01-sub-tests PRIVATE native native/enemies build-tools/ninja)
  target_link_libraries(recvx-en01-sub-tests PRIVATE recvx_game recvx_math)
  add_test(NAME enemy_en01_sub_component COMMAND recvx-en01-sub-tests)
  # EN01-Sterbezustaende DD00/DD01/DD02/DD02B -> bhSetBloodPoolLnk (en01.c:9990/9994/10122/10149, en01b.c:763).
  add_executable(recvx-en01-blood-pool-tests tests/native/en01_blood_pool_test.c)
  target_include_directories(recvx-en01-blood-pool-tests PRIVATE native native/enemies)
  target_link_libraries(recvx-en01-blood-pool-tests PRIVATE recvx_game recvx_math)
  add_test(NAME enemy_en01_blood_pool COMMAND recvx-en01-blood-pool-tests)
  # EN01 ExpLeg/ExpWorm-Splitter: njRandom() positiv (macros.h:105, Disassembly 0x1882C8/0x1885E8).
  add_executable(recvx-en01-exp-effect-tests tests/native/en01_exp_effect_test.c)
  target_include_directories(recvx-en01-exp-effect-tests PRIVATE native native/enemies)
  target_link_libraries(recvx-en01-exp-effect-tests PRIVATE recvx_game recvx_math)
  add_test(NAME enemy_en01_exp_effect COMMAND recvx-en01-exp-effect-tests)
  add_executable(recvx-gameplay-bindings-tests tests/native/gameplay_bindings_test.cpp)
  target_link_libraries(recvx-gameplay-bindings-tests PRIVATE recvx_game)
if(TARGET recvx_input)
 target_link_libraries(recvx-gameplay-bindings-tests PRIVATE recvx_input)
 target_compile_definitions(recvx-gameplay-bindings-tests PRIVATE RECVX_TEST_INPUT)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0050.RDT")
  add_test(NAME native-gameplay-bindings COMMAND recvx-gameplay-bindings-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  # A01-A04 Spielerkampf: Komponententest des Waffenzustandsautomaten.
  add_executable(recvx-combat-state-tests tests/native/combat_state_test.cpp)
  target_link_libraries(recvx-combat-state-tests PRIVATE recvx_game recvx_math)
  target_include_directories(recvx-combat-state-tests PRIVATE native native/enemies "${RECVX_TABLES}")
  add_test(NAME combat_player_state COMMAND recvx-combat-state-tests)
  # weapon.c:1632 bhSetGunSplash auf der sys->ef-Vorlage (native/combat.cpp gun_splash).
  add_executable(recvx-gun-splash-tests tests/native/gun_splash_test.cpp)
  target_link_libraries(recvx-gun-splash-tests PRIVATE recvx_game recvx_math)
  target_include_directories(recvx-gun-splash-tests PRIVATE native native/enemies "${RECVX_TABLES}")
  add_test(NAME gun_splash COMMAND recvx-gun-splash-tests)
  # eneset.c:797 bhDrawEnemy: EN01-Kappe mit mdl[0] = epw->mdl[slot] (en01.c:2612-2624), RM_3160.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_3160.RDT")
    add_executable(recvx-en01-child-draw-tests tests/native/en01_child_draw_test.cpp)
    target_include_directories(recvx-en01-child-draw-tests PRIVATE native native/enemies)
    target_compile_features(recvx-en01-child-draw-tests PRIVATE cxx_std_17)
    target_link_libraries(recvx-en01-child-draw-tests PRIVATE recvx_game recvx_math)
    add_test(NAME enemy_en01_child_draw COMMAND recvx-en01-child-draw-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
endif()
  # Das ILP32-Originalkern-Audit ist entfernt; Quellen in
  # der gesperrten Quarantaene. Kein historischer Audit-Build ist erlaubt.
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
  # Host-GPU-Rasterer bitgleich zur Bandstufe (Punktabtastung); ohne GPU-Backend uebersprungen.
  add_executable(recvx-gpu-raster-tests tests/native/gpu_raster_test.cpp)
  target_link_libraries(recvx-gpu-raster-tests PRIVATE recvx_scene)
  if(WIN32 AND RECVX_ENABLE_OPENGL)
    add_custom_command(TARGET recvx-gpu-raster-tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:recvx-gpu-raster-tests>)
  endif()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_1090.RDT")
    add_test(NAME gpu_raster_parity COMMAND recvx-gpu-raster-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  else()
    add_test(NAME gpu_raster_parity COMMAND recvx-gpu-raster-tests)
  endif()
  set_tests_properties(gpu_raster_parity PROPERTIES SKIP_RETURN_CODE 77)
  if(RECVX_ENABLE_OPENGL)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_1090.RDT")
      add_test(NAME gpu_raster_opengl_parity COMMAND recvx-gpu-raster-tests "${CMAKE_CURRENT_SOURCE_DIR}/data" --backend opengl)
      add_test(NAME gpu_raster_opengl_copy_parity COMMAND recvx-gpu-raster-tests "${CMAKE_CURRENT_SOURCE_DIR}/data" --backend opengl --force-copy)
    else()
      add_test(NAME gpu_raster_opengl_parity COMMAND recvx-gpu-raster-tests --backend opengl)
      add_test(NAME gpu_raster_opengl_copy_parity COMMAND recvx-gpu-raster-tests --backend opengl --force-copy)
    endif()
    set_tests_properties(gpu_raster_opengl_parity gpu_raster_opengl_copy_parity PROPERTIES SKIP_RETURN_CODE 77)
  endif()
  add_executable(recvx-camera-return-tests tests/native/camera_return_test.cpp)
  target_link_libraries(recvx-camera-return-tests PRIVATE recvx_game)
  add_test(NAME active_camera_return COMMAND recvx-camera-return-tests)
  # game.c:50/58/289: Effektpass vor bhControlCamera, bhCheckCut nach dem Zeichnen (RM_1020 evc 20).
  add_executable(recvx-camera-control-order-tests tests/native/camera_control_order_test.cpp)
  target_link_libraries(recvx-camera-control-order-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_1020.RDT")
    add_test(NAME camera_control_order COMMAND recvx-camera-control-order-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-scene-tests tests/native/scene_test.cpp)
  target_link_libraries(recvx-scene-tests PRIVATE recvx_scene)
  add_test(NAME scene_renderer COMMAND recvx-scene-tests)
  # F05: independent modifier asset and effect-state checks.
  add_executable(recvx-shadow-geometry-tests tests/native/shadow_geometry_test.cpp)
  target_link_libraries(recvx-shadow-geometry-tests PRIVATE recvx_scene)
  add_test(NAME shadow_original_geometry COMMAND recvx-shadow-geometry-tests
    "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/SYSTEM/0001.bin")
  add_executable(recvx-shadow-state-tests tests/native/shadow_state_test.cpp)
  target_link_libraries(recvx-shadow-state-tests PRIVATE recvx_scene)
  add_test(NAME shadow_effect_state COMMAND recvx-shadow-state-tests)
  add_executable(recvx-lighting-fog-tests tests/native/lighting_fog_test.cpp)
  target_link_libraries(recvx-lighting-fog-tests PRIVATE recvx_scene)
  add_test(NAME lighting_fog_semantics COMMAND recvx-lighting-fog-tests)
  add_executable(recvx-shadow-mask-tests tests/native/shadow_mask_test.cpp)
  target_link_libraries(recvx-shadow-mask-tests PRIVATE recvx_scene)
  add_test(NAME shadow_mask_arithmetic COMMAND recvx-shadow-mask-tests)
  add_executable(recvx-shadow-raster-tests tests/native/shadow_raster_test.cpp)
  target_link_libraries(recvx-shadow-raster-tests PRIVATE recvx_scene)
  add_test(NAME shadow_mask_raster COMMAND recvx-shadow-raster-tests)
  add_executable(recvx-shadow-projection-tests tests/native/shadow_projection_test.cpp)
  target_link_libraries(recvx-shadow-projection-tests PRIVATE recvx_scene)
  add_test(NAME shadow_original_projection COMMAND recvx-shadow-projection-tests
    "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/SYSTEM/0001.bin")
  add_executable(recvx-shadow-volume-tests tests/native/shadow_volume_test.cpp)
  target_link_libraries(recvx-shadow-volume-tests PRIVATE recvx_scene)
  add_test(NAME shadow_original_volume COMMAND recvx-shadow-volume-tests
    "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/SYSTEM/0001.bin")
  add_executable(recvx-shadow-depth-tests tests/native/shadow_depth_test.cpp)
  target_link_libraries(recvx-shadow-depth-tests PRIVATE recvx_scene)
  add_test(NAME shadow_depth_transport COMMAND recvx-shadow-depth-tests)
  add_test(NAME shadow_elf_semantics COMMAND "${Python3_EXECUTABLE}" -I
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_shadow_semantics.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/data/files/SLUS_201.84")


  add_executable(recvx-shadow-renderer-tests tests/native/shadow_renderer_test.cpp)
  target_link_libraries(recvx-shadow-renderer-tests PRIVATE recvx_scene)
  add_test(NAME shadow_modifier_pixels COMMAND recvx-shadow-renderer-tests
    "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/SYSTEM/0001.bin")


  add_executable(recvx-shadow-world-tests tests/native/shadow_world_test.cpp)
  target_link_libraries(recvx-shadow-world-tests PRIVATE recvx_game)
  add_test(NAME shadow_world_binding COMMAND recvx-shadow-world-tests
    "${CMAKE_CURRENT_SOURCE_DIR}/data")


  add_executable(recvx-face-tests tests/native/face_test.cpp)
  target_link_libraries(recvx-face-tests PRIVATE recvx_game)
  add_test(NAME original_face_geometry COMMAND recvx-face-tests)
  add_executable(recvx-actor-tests tests/native/actor_test.cpp)
  target_link_libraries(recvx-actor-tests PRIVATE recvx_scene)
  add_test(NAME player_motion_skin COMMAND recvx-actor-tests)
  add_executable(recvx-morph-tests tests/native/morph_test.cpp)
  target_link_libraries(recvx-morph-tests PRIVATE recvx_scene)
  add_test(NAME actor_morph_blend COMMAND recvx-morph-tests)
  add_executable(recvx-player-event-tests tests/native/player_event_test.cpp)
  target_link_libraries(recvx-player-event-tests PRIVATE recvx_game)
  add_test(NAME player_event COMMAND recvx-player-event-tests)
  # Motion.c:29/131: weniger Motionkanaele als Modellknoten (Wesker 0013 Motion 9).
  add_executable(recvx-player-short-motion-tests tests/native/player_short_motion_test.cpp)
  target_link_libraries(recvx-player-short-motion-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_3080.RDT")
    add_test(NAME player_short_motion COMMAND recvx-player-short-motion-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
  add_executable(recvx-player-action-tests tests/native/player_action_test.cpp)
  target_link_libraries(recvx-player-action-tests PRIVATE recvx_game)
  add_test(NAME player_action COMMAND recvx-player-action-tests)
  # player.c:1114 bhStandPlayerMotion auf dem Spielerwerk in RoomWorld (RM_4020 bhEff134 case 20).
  add_executable(recvx-stand-player-motion-tests tests/native/stand_player_motion_test.cpp)
  target_link_libraries(recvx-stand-player-motion-tests PRIVATE recvx_game)
  add_test(NAME stand_player_motion COMMAND recvx-stand-player-motion-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-morph-world-tests tests/native/morph_world_test.cpp)
  target_link_libraries(recvx-morph-world-tests PRIVATE recvx_game)
  add_test(NAME enemy_morph_selection COMMAND recvx-morph-world-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
endif()
install(TARGETS recvx-native RUNTIME DESTINATION bin)

if(BUILD_TESTING)
  add_executable(recvx-continuity-tests tests/native/continuity_test.cpp)
  target_link_libraries(recvx-continuity-tests PRIVATE recvx_game)
  add_test(NAME core_continuity COMMAND recvx-continuity-tests)

  # Haarkette des angehaengten Spielerobjekts obwp[2] (bhObjClpn/bhCalcHair).
  add_executable(recvx-player-hair-tests tests/native/player_hair_test.cpp)
  target_link_libraries(recvx-player-hair-tests PRIVATE recvx_game)
  add_test(NAME player_hair COMMAND recvx-player-hair-tests)
  # obwp[2] folgt seinem lkwkp (bhObjLinkSet/-Ply event.c:2177/2501, objitm.c:310-321/363-371/503/635-656).
  if(TARGET recvx_input)
    add_executable(recvx-player-hair-link-tests tests/native/player_hair_link_test.cpp)
    target_link_libraries(recvx-player-hair-link-tests PRIVATE recvx_input)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_1050.RDT")
      add_test(NAME player_hair_link COMMAND recvx-player-hair-link-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
    endif()
  endif()

  # Kopfsteuerung des Spielers (bhControlPlayerHead/bhLookNearEnemy/bhSetHeadRotation).
  add_executable(recvx-player-head-tests tests/native/player_head_test.cpp)
  target_link_libraries(recvx-player-head-tests PRIVATE recvx_game)
  add_test(NAME player_head COMMAND recvx-player-head-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-event-opcode-length-tests tests/native/event_opcode_length_test.cpp)
  target_link_libraries(recvx-event-opcode-length-tests PRIVATE recvx_game)
  add_test(NAME event_opcode_lengths COMMAND recvx-event-opcode-length-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-event-camera-hide-tests tests/native/event_camera_hide_test.cpp)
  target_link_libraries(recvx-event-camera-hide-tests PRIVATE recvx_game recvx_math)
  add_test(NAME event_camera_hide COMMAND recvx-event-camera-hide-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
endif()

# Original crate/player push state and Motion.c sampling.
# tools/generate_player_push.py ist Quarantäne (Policy 2026-10-09): die
# Spielerkollision (bhCheckWall/bhCheckFloorP/bhCheckDansa), bhObj001 und der
# Schiebeablauf fehlen bis zur nativen Neubestimmung, siehe
# native/player_push_core.c.
target_sources(recvx_game PRIVATE native/player_push.cpp native/player_push_core.c)

if(BUILD_TESTING)
 add_executable(recvx-push-tests tests/native/push_test.cpp)
 target_link_libraries(recvx-push-tests PRIVATE recvx_game)
 if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/SYSTEM/0010.bin")
  add_test(NAME player_push COMMAND recvx-push-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
 endif()
endif()

# objitm.c:1648 bhObj007 + player.c:4336 bhCPM2_act_rpsh: der Greif-und-Dreh-
# Gegenstand (RM_0150 Eisenstatue, Typ 7).
if(BUILD_TESTING)
 add_executable(recvx-turn-object-tests tests/native/turn_object_test.cpp)
 target_link_libraries(recvx-turn-object-tests PRIVATE recvx_game)
 if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/archives/SYSTEM/0010.bin")
  add_test(NAME turn_object COMMAND recvx-turn-object-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
 endif()
endif()

# tools/generate_player_combat.py ist Quarantäne (Policy 2026-10-09): der
# Spielerkampf (Zielen, Schuss, Messer, Mündungsfeuer, Zielaufschaltung) fehlt
# bis zur nativen Neubestimmung, siehe native/player_combat_core.c. Damit
# entfällt auch sein Typ-Include, das ohne denselben Generator nicht entsteht.
target_sources(recvx_game PRIVATE native/player_combat_core.c)
# A01-A04: native/combat_geometry.c traegt die Kollisionsprimitive des
# Spielerkampfs. Solange es hier fehlt, uebersetzt player_combat_core.c die
# Einheit mit; mit diesem Eintrag entfaellt das #include "combat_geometry.c".
target_sources(recvx_game PRIVATE native/combat_geometry.c)
# pwksub.c bhCheckC2Wall with its Ninja primitives (CheckCollision4Sound).
target_sources(recvx_game PRIVATE native/wall_capsule.cpp native/audio_pitch.cpp native/audio_sq.cpp native/audio_pan.cpp native/audio_spu.cpp)
if(NOT MSVC)
  set_property(SOURCE native/wall_capsule.cpp native/audio.cpp APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off)
endif()
# Preserve original C field aliasing and separate float multiply/add evaluation.
if(NOT MSVC)
 set_source_files_properties(native/player_push_core.c PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing;-ffp-contract=off")
endif()

# Explicit target builds must pass the same gate as the default/runtime build.
get_property(RECVX_LOCAL_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(RECVX_LOCAL_TARGET IN LISTS RECVX_LOCAL_TARGETS)
  if(NOT RECVX_LOCAL_TARGET STREQUAL "recvx-native-policy")
    get_target_property(RECVX_LOCAL_TYPE "${RECVX_LOCAL_TARGET}" TYPE)
    if(RECVX_LOCAL_TYPE MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|OBJECT_LIBRARY|MODULE_LIBRARY)$")
      add_dependencies("${RECVX_LOCAL_TARGET}" recvx-native-policy)
    endif()
  endif()
endforeach()

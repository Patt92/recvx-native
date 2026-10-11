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
  add_test(NAME native_port_policy_contracts
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/port_policy_test.py")
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
add_library(recvx_game native/file_folder.cpp native/effect_collision.cpp native/gameover.cpp native/effect_particles.cpp native/computer.cpp native/computer_core.c native/computer_tables.c native/subplayer.c native/enemies/en01_damage.c native/enemies/enemy_spawn.cpp native/effect_sprites.cpp native/inspection.cpp native/combat.cpp native/weapon_flash.cpp native/targeting.cpp native/weapon_asset.cpp native/box_menu.cpp native/inventory.cpp native/messages.cpp native/ui.cpp native/events.cpp native/audio.cpp native/movie.cpp native/world.cpp native/collision.cpp native/door_core.c native/doors.cpp native/item_tables.c native/status_pulse.cpp native/snapshot.cpp native/view_clip.cpp native/player_stairs.c native/player_hair.c native/player_head.c native/adx_tables.c)
target_sources(recvx_game PRIVATE native/core_random.cpp native/object_itmbox.cpp native/world_itmbox.cpp native/geometry_motion.cpp)
# C02: gemeinsamer Gegnerwork am WorldEntity-Besitzer, native Dispatchkette,
# bhSetMotion/bhCalcModel des Besitzers und alle nativ vorhandenen Gegner-
# routinen. en54/en55 fehlen bis zum Port von en17.c (Kettenwerk-Besitzer).
target_sources(recvx_game PRIVATE native/world_enemies.cpp native/motion_set.cpp
  native/enemies/enemy_work.c native/enemies/enemy_dispatch.c
  native/enemies/en00.c native/enemies/en07.c native/enemies/en08.c native/enemies/en10.c
  native/enemies/en16.c native/enemies/en20.c native/enemies/en24.c native/enemies/en25.c
  native/enemies/en56.c native/enemies/en71.c
  native/enemies/en03_lib.c native/enemies/en03_collision.c)
if(NOT MSVC)
  # Preserve separate float multiply/add steps also in production-only builds.
  set_property(SOURCE native/effect_collision.cpp native/geometry_motion.cpp native/motion_set.cpp native/enemies/en00.c native/enemies/en07.c native/enemies/en08.c native/enemies/en10.c native/enemies/en16.c native/enemies/en20.c native/enemies/en24.c native/enemies/en25.c native/enemies/en56.c native/enemies/en71.c native/enemies/en03_lib.c native/enemies/en03_collision.c APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off)
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
add_library(recvx_scene native/scene.cpp native/renderer.cpp native/actor.cpp native/face.cpp native/item_asset.cpp native/chunk_blend.cpp)
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
  add_dependencies(recvx-runtime recvx-native-policy)
  add_custom_command(TARGET recvx-runtime POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_no_original_core.py"
      "${CMAKE_CURRENT_SOURCE_DIR}" --build-dir "${CMAKE_CURRENT_BINARY_DIR}" --binary "$<TARGET_FILE:recvx-runtime>"
    VERBATIM)
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
  # No original engine or historical console audit target is permitted.
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
  add_executable(recvx-ground-world-tests tests/native/ground_world_test.cpp)
  target_link_libraries(recvx-ground-world-tests PRIVATE recvx_game)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/rooms/RM_0050.RDT")
    add_test(NAME native_ground_world COMMAND recvx-ground-world-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  endif()
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
  add_executable(recvx-effect-pool-tests tests/native/effect_pool_test.cpp)
  target_link_libraries(recvx-effect-pool-tests PRIVATE recvx_game)
  add_test(NAME effect_pool_lifetime COMMAND recvx-effect-pool-tests)
  add_executable(recvx-effect-room-tests tests/native/effect_room_sprites_test.cpp)
  target_link_libraries(recvx-effect-room-tests PRIVATE recvx_game)
  add_test(NAME effect_room_sprites COMMAND recvx-effect-room-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-lighter-tests tests/native/lighter_test.cpp)
  target_link_libraries(recvx-lighter-tests PRIVATE recvx_game)
  add_test(NAME native_lighter_effect COMMAND recvx-lighter-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-gameover-tests tests/native/gameover_test.cpp)
  target_link_libraries(recvx-gameover-tests PRIVATE recvx_game)
  add_test(NAME gameover_original COMMAND recvx-gameover-tests "${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_executable(recvx-enemy-damage-tests tests/native/enemy_damage_test.cpp)
  target_link_libraries(recvx-enemy-damage-tests PRIVATE recvx_game)
  add_test(NAME enemy_damage_original_en01 COMMAND recvx-enemy-damage-tests)
  # Der Test zu native/subplayer.c stand bisher in keinem Ziel und lief nie
  # (tools/check_build_coverage.py). Er prueft beide ct2-Tabellen, alle elf
  # Bewegungszweige an ihrer Feldsignatur und die Bereichsgrenzen.
  # Wie recvx-weapon-flash-tests bewusst OHNE recvx_game/recvx_scene: der
  # Apparat braucht nur bhSetMotion/bhCalcModel als Zeiger (setzt der Test
  # selbst) und njSin/njCos. Aus enemy_work.h kommen nur Typen, keine Koerper.
  add_executable(recvx-subplayer-tests tests/native/subplayer_test.cpp native/subplayer.c)
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
  add_executable(recvx-continuity-tests tests/native/continuity_test.cpp)
  target_link_libraries(recvx-continuity-tests PRIVATE recvx_game)
  add_test(NAME core_continuity COMMAND recvx-continuity-tests)

  # Haarkette des angehaengten Spielerobjekts obwp[2] (bhObjClpn/bhCalcHair).
  add_executable(recvx-player-hair-tests tests/native/player_hair_test.cpp)
  target_link_libraries(recvx-player-hair-tests PRIVATE recvx_game)
  add_test(NAME player_hair COMMAND recvx-player-hair-tests)

  # Kopfsteuerung des Spielers (bhControlPlayerHead/bhLookNearEnemy/bhSetHeadRotation).
  add_executable(recvx-player-head-tests tests/native/player_head_test.cpp)
  target_link_libraries(recvx-player-head-tests PRIVATE recvx_game)
  add_test(NAME player_head COMMAND recvx-player-head-tests)
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

# tools/generate_player_combat.py ist Quarantäne (Policy 2026-10-09): der
# Spielerkampf (Zielen, Schuss, Messer, Mündungsfeuer, Zielaufschaltung) fehlt
# bis zur nativen Neubestimmung, siehe native/player_combat_core.c. Damit
# entfällt auch sein Typ-Include, das ohne denselben Generator nicht entsteht.
target_sources(recvx_game PRIVATE native/player_combat_core.c)
# A01-A04: native/combat_geometry.c traegt die Kollisionsprimitive des
# Spielerkampfs. Solange es hier fehlt, uebersetzt player_combat_core.c die
# Einheit mit; mit diesem Eintrag entfaellt das #include "combat_geometry.c".
target_sources(recvx_game PRIVATE native/combat_geometry.c)
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

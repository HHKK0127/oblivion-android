cmake_minimum_required(VERSION 3.18.1)
project(oblivion_native)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Jolt Physics integration
# ============================================================================
set(JOLT_PHYSICS_DIR "${CMAKE_SOURCE_DIR}/third_party/JoltPhysics")

# Add Jolt as subdirectory (recommended)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)  # Disable Vulkan (not needed for Android OpenGL ES)

add_subdirectory(${JOLT_PHYSICS_DIR}/Build ${CMAKE_BINARY_DIR}/JoltPhysics)

# Compile flags for Jolt
# JPH_DISABLE_CUSTOM_ALLOCATOR is not used (Jolt library provides its own allocator)
target_compile_definitions(Jolt PRIVATE
    JPH_ENABLE_ASSERTS
)

# ============================================================================

# Add main library
add_library(native-lib SHARED
    # Engine Core
    engine/renderer.cpp
    engine/shader.cpp
    engine/camera.cpp
    engine/texture_loader.cpp
    engine/graphics/retro_filter.cpp
    engine/imperial_weave.cpp

    # Phase 44: Performance Optimization
    engine/memory_pool.cpp
    engine/render_optimizer.cpp
    engine/async_task_manager.cpp
    engine/cache_manager.cpp
    engine/profiler_dashboard.cpp
    # Phase 42: Game Loop Integration
    engine/state_manager.cpp
    engine/input_router.cpp
    engine/game_loop_coordinator.cpp
    engine/scene_renderer.cpp
    engine/debug_console.cpp
    engine/performance_profiler.cpp
    ui/spell_selection_panel.cpp
    audio/audio_subscriber.cpp
    animation/animation_subscriber.cpp

    # Geometry
    geometry/cube.cpp
    geometry/mesh.cpp
    geometry/skin_partition_packer.cpp
    geometry/material.cpp
    animation/skeleton.cpp
    animation/animation_player.cpp
    collision/aabb_tree.cpp
    collision/collision_world.cpp
    collision/character_controller.cpp

    # World System (Phase 3+)
    world/world_manager.cpp
    world/world_item_manager.cpp
    world/cell.cpp
    world/cell_loader.cpp
    world/cell_transition_manager.cpp
    world/door.cpp
    world/grid.cpp
    world/terrain.cpp

    # Phase 31: World Entity & Loader
    world/world_loader.cpp

    # Phase 38: Full World Load (ESM -> 3D Space)
    world/landscape_renderer.cpp
    world/object_placer.cpp
    world/texture_manager.cpp
    world/lod_system.cpp
    world/world_builder.cpp

    # Phase 50: Distant LOD System
    world/distant_lod/distant_lod_manager.cpp
    world/distant_lod/distant_lod_horizon.cpp

    # Game Systems
    game/npc.cpp
    game/npc_manager.cpp
    game/combat_manager.cpp
    game/quest_manager.cpp
    game/spell_manager.cpp
    game/navmesh_manager.cpp
    game/ai_package.cpp
    game/ai_scheduler.cpp
    game/loot_generator.cpp
    game/book_reader.cpp
    game/clothing_converter.cpp
    game/alchemy_system.cpp
    game/misc_item_converter.cpp
    game/faction_manager.cpp
    game/dialogue.cpp
    game/door.cpp
    game/interactable.cpp
    game/container.cpp
    game/interaction_manager.cpp
    game/merchant.cpp

    # Player & Movement (Phase 3+)
    game/player.cpp
    game/player_controller.cpp
    input/touch_input_handler.cpp

    # Inventory System (Phase 3+)
    game/inventory.cpp
    game/inventory_manager.cpp
    game/consumable_system.cpp
    game/equipment_effect_system.cpp
    game/book_database.cpp

    # UI Systems
    ui/title_screen.cpp
    ui/launcher_screen.cpp
    ui/quest_ui.cpp
    ui/inventory_ui.cpp
    ui/text_renderer.cpp
    ui/debug_hud.cpp
    ui/game_console.cpp
    ui/debug_menu.cpp
    ui/texture_viewer.cpp
    ui/model_viewer.cpp
    ui/world_viewer.cpp
    ui/npc_debug_visualizer.cpp
    ui/world_debug_info.cpp
    ui/performance_graph.cpp
    ui/settings_ui.cpp
    ui/save_load_ui.cpp
    ui/ui_component.cpp
    ui/ui_system.cpp
    ui/ui_panel.cpp
    ui/ui_button.cpp
    ui/ui_draw_helper.cpp
    ui/ui_map_panel.cpp
    ui/ui_inventory_panel.cpp
    ui/ui_character_sheet.cpp
    ui/ui_spellbook.cpp
    ui/ui_quest_log.cpp
    ui/ui_dialogue.cpp
    ui/ui_shop.cpp
    ui/ui_character_creation.cpp
    ui/ui_pause_menu.cpp
    ui/ui_message_box.cpp
    ui/ui_toast.cpp
    ui/ui_quick_slot_bar.cpp
    ui/ui_hud_status_display.cpp
    ui/ui_hud_compass.cpp
    ui/ui_floating_text.cpp
    ui/ui_target_info.cpp
    ui/ui_action_prompt.cpp
    ui/ui_active_effects.cpp
    ui/ui_level_progress.cpp
    ui/ui_alert_notification.cpp
    ui/ui_minimap.cpp
    ui/ui_player_stats.cpp
    ui/ui_manager.cpp
    ui/ui_joystick.cpp
    ui/placeholder_assets.cpp
    ui/hud_renderer.cpp

    # Phase 43: UI/UX Final Adjustments
    ui/touch_gesture_handler.cpp
    ui/menu_transition_manager.cpp
    ui/hud_layout.cpp
    ui/control_scheme_manager.cpp
    ui/accessibility_manager.cpp

    # Phase 49: Control System Optimization
    input/touch_calibration.cpp
    input/gamepad_mapper.cpp
    input/input_visualizer.cpp
    ui/hud_customizer.cpp

    # Map System (Phase 9.1)
    map/map_system.cpp

    # Inventory System (Phase 9.2)
    inventory/inventory_grid.cpp
    inventory/equipment_manager.cpp
    inventory/item_factory.cpp
    inventory/inventory_coordinator.cpp

    # System
    system/settings_manager.cpp
    system/debug_system.cpp

    # Localization
    localization/localization_manager.cpp

    # Profiling
    profiling/performance_monitor.cpp

    # Save System
    save_system/save_manager.cpp
    save_system/save_slot_manager.cpp

    # Audio System (Phase 8+)
    audio/audio_manager.cpp
    audio/audio_3d.cpp
    audio/jni_audio_bridge.cpp

    # Phase 47: Audio System Integration
    audio/audio_decoder.cpp
    audio/bgm_manager.cpp
    audio/sound_effect_manager.cpp

    # Asset System
    assets/nif_parser.cpp
    assets/nif_block_type_map.cpp
    assets/dds_loader.cpp
    assets/asset_manager.cpp
    assets/bsa_reader.cpp
    assets/esm_reader.cpp

    # Phase 46: Asset Pipeline
    assets/texture_manager.cpp
    assets/mesh_loader.cpp
    assets/world_data_loader.cpp

    # Tests (Phase 30 Step 13)
    tests/phase30_integration_test.cpp

    # Tests (Phase 38: Script VM)
    tests/script_vm_tests.cpp

    # Tests (Phase 45: Unit Tests)
    tests/phase45_unit_tests.cpp

    # Tests (Phase 48: Game Loop Integration & Stress)
    tests/phase48_integration_test.cpp
    tests/phase48_stress_test.cpp

    # Physics System (Phase 36 - Jolt Physics)
    physics/physics_manager.cpp

    # Script VM (Phase 37 - Oblivion Script System)
    script/script_vm.cpp
    script/script_context.cpp
    script/script_manager.cpp
    script/script_functions.cpp
    script/script_disasm.cpp

    # Quest System (Phase 39 - Quest Flow)
    quest/quest_record.cpp
    quest/quest_stage_manager.cpp
    quest/quest_objective_tracker.cpp
    quest/quest_rewards.cpp
    quest/quest_flow_controller.cpp

    # Dialogue System (Phase 40 - NPC Dialogue Tree)
    dialogue/dialogue_record.cpp
    dialogue/dialogue_tree.cpp
    dialogue/dialogue_filter_engine.cpp
    dialogue/dialogue_runner.cpp
    dialogue/dialogue_history.cpp
    dialogue/dialogue_integration.cpp

    # Phase 51: SpeedTree Vegetation System
    vegetation/speed_tree_manager.cpp
    vegetation/tree_wind_field.cpp
    vegetation/tree_emitter.cpp

    # Phase 52: FaceGen System
    character/face_gen_morpher.cpp
    character/face_gen_cache.cpp

    # Phase 53: Bink Video replacement (MediaCodec-based video playback)
    video/bink_video_player.cpp
    video/video_decoder_jni.cpp
    video/video_clip_manager.cpp
    video/video_renderer.cpp

    # JNI Bridge (Java/C++ communication)
    jni_bridge.cpp
)

# Include directories
target_include_directories(native-lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/game
    ${CMAKE_CURRENT_SOURCE_DIR}/world
    ${CMAKE_CURRENT_SOURCE_DIR}/player
    ${CMAKE_CURRENT_SOURCE_DIR}/input
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry
    ${CMAKE_CURRENT_SOURCE_DIR}/math
    ${CMAKE_CURRENT_SOURCE_DIR}/localization
    ${CMAKE_CURRENT_SOURCE_DIR}/profiling
    ${CMAKE_CURRENT_SOURCE_DIR}/save_system
    ${CMAKE_CURRENT_SOURCE_DIR}/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/map
    ${CMAKE_CURRENT_SOURCE_DIR}/inventory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/collision
    ${CMAKE_CURRENT_SOURCE_DIR}/animation
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/physics
    ${CMAKE_CURRENT_SOURCE_DIR}/script
    ${CMAKE_CURRENT_SOURCE_DIR}/quest
    ${CMAKE_CURRENT_SOURCE_DIR}/dialogue
    ${CMAKE_CURRENT_SOURCE_DIR}/vegetation
    ${CMAKE_CURRENT_SOURCE_DIR}/character
    ${CMAKE_CURRENT_SOURCE_DIR}/video
    ${CMAKE_CURRENT_SOURCE_DIR}/system
)

# Find OpenAL-Soft (optional for now)
find_package(OpenAL CONFIG QUIET)
if(NOT OpenAL_FOUND)
    # Try to find OpenAL in standard paths
    find_package(OpenAL QUIET)
endif()

# Audio system: Using Android MediaPlayer via JNI bridge (no OpenAL dependency)
# Message output for build info
if(OpenAL_FOUND)
    message(STATUS "OpenAL found but not used - JNI bridge approach")
else()
    message(STATUS "Using Android MediaPlayer via JNI for audio playback")
endif()

# Link Android native libraries
target_link_libraries(native-lib
    Jolt
    android
    EGL
    GLESv3
    log
    z  # Android's built-in libz for BSA ZLib decompression
)

# Link OpenAL if found
if(OpenAL_FOUND)
    target_link_libraries(native-lib OpenAL::OpenAL)
endif()

# Compiler flags
if (ANDROID_ABI STREQUAL arm64-v8a)
    target_compile_options(native-lib PRIVATE -O3 -march=armv8-a -Wno-macro-redefined)
elseif (ANDROID_ABI STREQUAL armeabi-v7a)
    target_compile_options(native-lib PRIVATE -O3 -march=armv7-a -mfloat-abi=softfp -Wno-macro-redefined)
elseif (ANDROID_ABI STREQUAL x86)
    target_compile_options(native-lib PRIVATE -O3 -march=i686 -Wno-macro-redefined)
elseif (ANDROID_ABI STREQUAL x86_64)
    target_compile_options(native-lib PRIVATE -O3 -march=x86-64 -Wno-macro-redefined)
endif ()

# Common flags
target_compile_options(native-lib PRIVATE
    -Wall
    -Wextra
    -Werror=format
)

# Enable audio system (OpenAL stub headers used)
target_compile_definitions(native-lib PRIVATE AUDIO_SYSTEM_ENABLED)

# Warning suppression for specific headers
set_source_files_properties(
    game/npc_manager.cpp
    game/quest_manager.cpp
    PROPERTIES
    COMPILE_FLAGS "-Wno-unused-variable"
)

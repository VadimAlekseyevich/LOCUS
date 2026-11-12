cmake_minimum_required(VERSION 3.25)

# LOCUS game and headless tests.
project(LOCUS VERSION 0.1.0 LANGUAGES C CXX)

option(LOCUS_BUILD_TESTS "Build and register core tests with CTest" OFF)
option(LOCUS_BUILD_BENCHMARKS "Build optional headless route preview profiler" OFF)

# Pinned raylib and JSON dependencies.
include(cmake/dependencies.cmake)

add_library(locus_core STATIC
    src/core/route.cpp
    src/core/route_view.cpp
    src/core/timed_route.cpp
    src/core/judgement.cpp
    src/core/song_clock.cpp
    src/core/demo_audio.cpp
    src/core/music_file.cpp
    src/core/chart_json.cpp
    src/core/chart_editor.cpp
    src/core/chart_difficulty.cpp
)
target_compile_features(locus_core PUBLIC cxx_std_17)
target_link_libraries(locus_core PRIVATE nlohmann_json::nlohmann_json)
set_target_properties(locus_core PROPERTIES
    CXX_EXTENSIONS OFF
)
target_include_directories(locus_core PUBLIC
    "${PROJECT_SOURCE_DIR}/src"
)

# Catalog scanner and disk writers form one cohesive module set. Keep
# executable/test target dependencies synchronized in exactly one location.
set(LOCUS_SONG_LIBRARY_SOURCES
    src/app/song_library.cpp
    src/app/song_library_scan.cpp
    src/app/song_library_metadata.cpp
    src/app/song_library_packages.cpp
    src/app/song_library_install.cpp
    src/app/song_library_trash.cpp
)

add_executable(locus
    src/app/main.cpp
    src/app/app_runtime_policy.cpp
    src/app/play_screen_actions.cpp
    src/app/play_presentation.cpp
    src/app/app_smoke_checks.cpp
    src/app/app_frame_renderer.cpp
    src/app/home_brand_assets.cpp
    src/app/image_asset_texture.cpp
    src/app/app_input.cpp
    src/app/menu_controller.cpp
    src/app/gameplay_ui.cpp
    src/app/screen_flow.cpp
    src/app/session_transition.cpp
    src/app/quit_media.cpp
    src/app/editor_discard_guard.cpp
    src/app/native_close.cpp
    src/app/open_folder.cpp
    src/app/window_mode.cpp
    src/app/demo_transport.cpp
    src/app/audio_device.cpp
    src/app/menu_music_preview.cpp
    src/app/local_music.cpp
    src/app/local_file_io.cpp
    src/app/music_picker.cpp
    src/app/chart_file.cpp
    src/app/chart_save.cpp
    src/app/song_set_storage.cpp
    src/app/editor_document.cpp
    src/app/editor_session.cpp
    src/app/editor_file_actions.cpp
    src/app/editor_set_controller.cpp
    src/app/editor_comparison.cpp
    src/app/editor_screen_actions.cpp
    src/app/listen_session.cpp
    src/app/chart_playback.cpp
    src/app/game_session.cpp
        ${LOCUS_SONG_LIBRARY_SOURCES}
    src/app/song_archive.cpp
    src/app/library_playback.cpp
    src/app/library_browser.cpp
    src/app/library_file_actions.cpp
    src/app/library_launch_actions.cpp
    src/app/game_settings.cpp
    src/app/skin.cpp
    src/app/skin_profile_actions.cpp
    src/app/skin_controls.cpp
    src/app/skin_menu.cpp
    src/app/skin_menu_draw.cpp
    src/app/menu_ui.cpp
    src/app/chart_editor_ui.cpp
    src/app/chart_editor_actions.cpp
    src/app/editor_timeline_actions.cpp
    src/app/editor_pattern_actions.cpp
    src/app/editor_text_fields.cpp
    src/app/editor_input.cpp
    src/app/route_renderer.cpp
    src/app/path_mesh.cpp
)
# Keep approved Home raster assets next to the executable regardless of the
# current working directory (launcher BAT, IDE, packaged release).
add_custom_command(TARGET locus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:locus>/ui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/assets/ui/wordmark.png"
        "$<TARGET_FILE_DIR:locus>/ui/wordmark.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/assets/ui/app-icon.png"
        "$<TARGET_FILE_DIR:locus>/ui/app-icon.png"
    VERBATIM)
target_link_libraries(locus PRIVATE locus_core raylib nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(locus PRIVATE comdlg32 user32)
endif()
set_target_properties(locus PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    # Text literals and UTF-8 test fixtures must not depend on system ANSI locale.
    add_compile_options(/utf-8)
    target_compile_options(locus_core PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(locus PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(locus_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(locus PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LOCUS_BUILD_BENCHMARKS)
    add_executable(locus_route_preview_bench
        benchmarks/route_preview_bench.cpp)
    target_compile_features(locus_route_preview_bench PRIVATE cxx_std_17)
    target_link_libraries(locus_route_preview_bench PRIVATE locus_core)
endif()

if(LOCUS_BUILD_TESTS)
    enable_testing()
    add_executable(locus_route_tests tests/route_tests.cpp)
    target_compile_features(locus_route_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_route_tests PRIVATE locus_core)
    add_test(NAME locus_demo_route COMMAND locus_route_tests)

    # PLAY uses exact song-time route boundaries, independent of editor ticks.
    add_executable(locus_timed_route_tests tests/timed_route_tests.cpp)
    target_compile_features(locus_timed_route_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_timed_route_tests PRIVATE locus_core)
    add_test(NAME locus_exact_time_route COMMAND locus_timed_route_tests)

    add_executable(locus_route_view_tests tests/route_view_tests.cpp)
    target_compile_features(locus_route_view_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_route_view_tests PRIVATE locus_core)
    add_test(NAME locus_route_view COMMAND locus_route_view_tests)

    add_executable(locus_music_tests tests/music_tests.cpp)
    target_compile_features(locus_music_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_music_tests PRIVATE locus_core)
    add_test(NAME locus_demo_audio_clock COMMAND locus_music_tests)

    add_executable(locus_playback_cursor_tests tests/playback_cursor_tests.cpp)
    target_compile_features(locus_playback_cursor_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_playback_cursor_tests PRIVATE locus_core)
    add_test(NAME locus_slow_playback_cursor COMMAND locus_playback_cursor_tests)

    add_executable(locus_music_file_tests tests/music_file_tests.cpp)
    target_compile_features(locus_music_file_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_music_file_tests PRIVATE locus_core)
    add_test(NAME locus_local_music_files COMMAND locus_music_file_tests)

    add_executable(locus_chart_json_tests tests/chart_json_tests.cpp)
    target_compile_features(locus_chart_json_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_chart_json_tests PRIVATE locus_core)
    add_test(NAME locus_chart_json_compatibility COMMAND locus_chart_json_tests)
    add_executable(locus_tempo_map_tests tests/tempo_map_tests.cpp)
    target_compile_features(locus_tempo_map_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_tempo_map_tests PRIVATE locus_core)
    add_test(NAME locus_song_tempo_map COMMAND locus_tempo_map_tests)
    # Keep the shipped example checked by the same parser as real files.
    set_tests_properties(locus_chart_json_compatibility PROPERTIES
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

    add_executable(locus_chart_difficulty_tests tests/chart_difficulty_tests.cpp)
    target_compile_features(locus_chart_difficulty_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_chart_difficulty_tests PRIVATE locus_core)
    add_test(NAME locus_chart_difficulty_v2 COMMAND locus_chart_difficulty_tests)

    add_executable(locus_chart_editor_tests tests/chart_editor_tests.cpp)
    target_compile_features(locus_chart_editor_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_chart_editor_tests PRIVATE locus_core)
    add_test(NAME locus_chart_editing COMMAND locus_chart_editor_tests)

    add_executable(locus_quit_dialog_input_tests
        tests/quit_dialog_input_tests.cpp)
    target_compile_features(locus_quit_dialog_input_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_quit_dialog_input_tests PRIVATE locus_core raylib)
    add_test(NAME locus_quit_dialog_input COMMAND locus_quit_dialog_input_tests)

    # Editor actions and hitboxes run headlessly; native polling is only linked.
    add_executable(locus_editor_ui_tests
        tests/editor_ui_tests.cpp
        src/app/editor_comparison.cpp
        src/app/chart_editor_ui.cpp
        src/app/chart_editor_actions.cpp
        src/app/editor_timeline_actions.cpp
        src/app/editor_pattern_actions.cpp
        src/app/editor_text_fields.cpp
        src/app/editor_input.cpp
        src/app/game_settings.cpp
        src/app/route_renderer.cpp
        src/app/path_mesh.cpp
    )
    target_compile_features(locus_editor_ui_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_ui_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    add_test(NAME locus_editor_controls COMMAND locus_editor_ui_tests)

    add_executable(locus_editor_comparison_tests
        tests/editor_comparison_tests.cpp
        src/app/editor_comparison.cpp)
    target_compile_features(locus_editor_comparison_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_comparison_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    add_test(NAME locus_editor_comparison COMMAND locus_editor_comparison_tests)

    add_executable(locus_chart_file_tests
        tests/chart_file_tests.cpp
        src/app/chart_file.cpp
        src/app/chart_save.cpp
        src/app/local_file_io.cpp
        src/app/song_set_storage.cpp
)
    target_compile_features(locus_chart_file_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_chart_file_tests PRIVATE locus_core nlohmann_json::nlohmann_json)
    add_test(NAME locus_chart_local_file COMMAND locus_chart_file_tests)

    # Document load/save orchestration must preserve an unsaved editor draft
    # across malformed charts, failed audio validation and failed saves.
    # Editor timeline and TEST preparation have no window or audio-device
    # dependency: invalid transport states must preserve game chart/session.
    # Native dialogs are injected; missing audio and failed/canceled selection
    # cannot erase an existing draft, chart path or browser state.
    # Listening picker can be canceled or fail without losing loaded-song
    # identity, and all transport actions use post-dialog monotonic time.
    add_executable(locus_listen_session_tests
        tests/listen_session_tests.cpp
        src/app/listen_session.cpp
        src/app/local_music.cpp
        src/app/local_file_io.cpp
    )
    target_compile_features(locus_listen_session_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_listen_session_tests PRIVATE locus_core raylib)
    add_test(NAME locus_listening_picker_lifecycle COMMAND locus_listen_session_tests)

    add_executable(locus_editor_file_actions_tests
        tests/editor_file_actions_tests.cpp
        src/app/editor_file_actions.cpp
        src/app/editor_set_controller.cpp
        src/app/editor_comparison.cpp
        src/app/open_folder.cpp
        src/app/editor_document.cpp
        src/app/music_picker.cpp
        src/app/local_music.cpp
        src/app/local_file_io.cpp
        src/app/chart_file.cpp
        src/app/chart_save.cpp
        src/app/chart_playback.cpp
            ${LOCUS_SONG_LIBRARY_SOURCES}
        src/app/song_archive.cpp
        src/app/song_set_storage.cpp
)
    target_compile_features(locus_editor_file_actions_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_file_actions_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    if(WIN32)
        target_link_libraries(locus_editor_file_actions_tests PRIVATE comdlg32 user32)
    endif()
    add_test(NAME locus_editor_file_actions_lifecycle COMMAND locus_editor_file_actions_tests)

    add_executable(locus_editor_session_tests
        tests/editor_session_tests.cpp
        src/app/editor_session.cpp
        src/app/editor_comparison.cpp
        src/app/chart_editor_ui.cpp
        src/app/chart_editor_actions.cpp
        src/app/editor_timeline_actions.cpp
        src/app/editor_pattern_actions.cpp
        src/app/editor_text_fields.cpp
        src/app/editor_input.cpp
        src/app/route_renderer.cpp
        src/app/path_mesh.cpp
        src/app/game_settings.cpp
        src/app/chart_playback.cpp
        src/app/game_session.cpp
        src/app/demo_transport.cpp
        src/app/local_music.cpp
        src/app/local_file_io.cpp
    )
    target_compile_features(locus_editor_session_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_session_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    add_test(NAME locus_editor_session_lifecycle COMMAND locus_editor_session_tests)

    add_executable(locus_editor_document_tests
        tests/editor_document_tests.cpp
        src/app/editor_document.cpp
        src/app/local_music.cpp
        src/app/local_file_io.cpp
        src/app/chart_file.cpp
        src/app/chart_save.cpp
        src/app/music_picker.cpp
        src/app/song_set_storage.cpp
)
    target_compile_features(locus_editor_document_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_document_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    if(WIN32)
        target_link_libraries(locus_editor_document_tests PRIVATE comdlg32 user32)
    endif()
    add_test(NAME locus_editor_document_lifecycle COMMAND locus_editor_document_tests)

    # No physical device needed: a failed demo initialization cannot claim
    # or close the audio device owned by the application.
    # Exercise screen-boundary effects without a window or audio device.
    # The pure navigation policy remains in screen_flow.
    add_executable(locus_session_transition_tests
        tests/session_transition_tests.cpp
        src/app/play_screen_actions.cpp
        src/app/app_runtime_policy.cpp
        src/app/screen_flow.cpp
        src/app/session_transition.cpp
        src/app/game_session.cpp
        src/app/chart_playback.cpp
        src/app/demo_transport.cpp
        src/app/local_music.cpp
        src/app/local_file_io.cpp
    )
    target_compile_features(locus_session_transition_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_session_transition_tests PRIVATE locus_core raylib)
    add_test(NAME locus_session_transition_lifecycle COMMAND locus_session_transition_tests)

    # Gameplay adjustments repeat on held keys; no window or audio is needed.
    add_executable(locus_play_adjustment_tests tests/play_adjustment_tests.cpp)
    target_compile_features(locus_play_adjustment_tests PRIVATE cxx_std_17)
    target_include_directories(locus_play_adjustment_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_gameplay_adjustment_repeat COMMAND locus_play_adjustment_tests)

    add_executable(locus_audio_fade_tests tests/audio_fade_tests.cpp)
    target_compile_features(locus_audio_fade_tests PRIVATE cxx_std_17)
    target_include_directories(locus_audio_fade_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_music_transition_easing COMMAND locus_audio_fade_tests)

    # Stateful visual playback runs headlessly; it does not draw or
    # manipulate the scored transport.
    add_executable(locus_play_presentation_tests
        tests/play_presentation_tests.cpp
        src/app/play_presentation.cpp
    )
    target_compile_features(locus_play_presentation_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_play_presentation_tests PRIVATE locus_core raylib)
    add_test(NAME locus_play_presentation_lifecycle COMMAND locus_play_presentation_tests)

    add_executable(locus_play_outro_tests tests/play_outro_tests.cpp)
    target_compile_features(locus_play_outro_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_play_outro_tests PRIVATE locus_core)
    add_test(NAME locus_scored_outro_transition COMMAND locus_play_outro_tests)

    add_executable(locus_screen_fade_tests tests/screen_fade_tests.cpp)
    target_compile_features(locus_screen_fade_tests PRIVATE cxx_std_17)
    target_include_directories(locus_screen_fade_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_screen_fade_transition COMMAND locus_screen_fade_tests)

    add_executable(locus_image_asset_tests tests/image_asset_tests.cpp)
    target_compile_features(locus_image_asset_tests PRIVATE cxx_std_17)
    target_include_directories(locus_image_asset_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(locus_image_asset_tests PRIVATE raylib)
    add_test(NAME locus_image_asset_geometry_and_validation COMMAND locus_image_asset_tests)

    add_executable(locus_editor_entry_tests tests/editor_entry_policy_tests.cpp)
    target_compile_features(locus_editor_entry_tests PRIVATE cxx_std_17)
    target_include_directories(locus_editor_entry_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_editor_entry_policy COMMAND locus_editor_entry_tests)

    # Exit modal policy is pure; test playback state changes without opening
    # a native window or relying on an actual sound device.
    # Editor discard confirmation is independent of native keyboard polling.
    add_executable(locus_editor_discard_tests
        tests/editor_discard_tests.cpp
        src/app/editor_discard_guard.cpp
    )
    target_compile_features(locus_editor_discard_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_discard_tests PRIVATE locus_core raylib)
    add_test(NAME locus_editor_unsaved_discard_guard COMMAND locus_editor_discard_tests)

    add_executable(locus_quit_media_tests
        tests/quit_media_tests.cpp
        src/app/quit_media.cpp
        src/app/game_session.cpp
        src/app/chart_playback.cpp
        src/app/demo_transport.cpp
        src/app/local_music.cpp
        src/app/local_file_io.cpp
    )
    target_compile_features(locus_quit_media_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_quit_media_tests PRIVATE locus_core raylib)
    add_test(NAME locus_quit_modal_audio_lifecycle COMMAND locus_quit_media_tests)

    add_executable(locus_audio_lifecycle_tests
        tests/audio_lifecycle_tests.cpp
        src/app/audio_device.cpp
        src/app/demo_transport.cpp
    )
    target_compile_features(locus_audio_lifecycle_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_audio_lifecycle_tests PRIVATE locus_core raylib)
    add_test(NAME locus_audio_device_lifecycle COMMAND locus_audio_lifecycle_tests)

    # Validate the semicircular reverse marker's orientation headlessly.
    # The cap has no impact on chart geometry, timing or saved data.
    # Deterministic, frame-rate-independent experimental camera projection.
    add_executable(locus_adaptive_camera_tests tests/adaptive_camera_tests.cpp)
    target_compile_features(locus_adaptive_camera_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_adaptive_camera_tests PRIVATE locus_core)
    target_include_directories(locus_adaptive_camera_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_adaptive_camera_projection COMMAND locus_adaptive_camera_tests)

    add_executable(locus_reverse_cap_tests tests/reverse_cap_tests.cpp)
    target_compile_features(locus_reverse_cap_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_reverse_cap_tests PRIVATE locus_core raylib)
    target_include_directories(locus_reverse_cap_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_reverse_cap_orientation COMMAND locus_reverse_cap_tests)

    # The scored route is a single connected, tapered screen-space ribbon.
    # Regression checks cover bends, fractional playheads and reversals.
    add_executable(locus_path_mesh_tests
        tests/path_mesh_tests.cpp
        src/app/path_mesh.cpp
    )
    target_compile_features(locus_path_mesh_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_path_mesh_tests PRIVATE locus_core raylib)
    add_test(NAME locus_continuous_path_mesh COMMAND locus_path_mesh_tests)

    add_executable(locus_path_fade_tests tests/path_fade_tests.cpp)
    target_compile_features(locus_path_fade_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_path_fade_tests PRIVATE raylib)
    target_include_directories(locus_path_fade_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_forward_only_path_fade COMMAND locus_path_fade_tests)

    add_executable(locus_hit_flash_tests tests/hit_flash_tests.cpp)
    target_compile_features(locus_hit_flash_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_hit_flash_tests PRIVATE locus_core)
    add_test(NAME locus_player_hit_flash COMMAND locus_hit_flash_tests)

    # Library activation must preserve live chart/audio state on decoder
    # failure and validate route timing before publishing selected difficulty.
    add_executable(locus_library_playback_tests
        tests/library_playback_tests.cpp
        src/app/library_playback.cpp
        src/app/chart_playback.cpp
        src/app/game_session.cpp
        src/app/demo_transport.cpp
        src/app/local_music.cpp
        src/app/local_file_io.cpp
    )
    target_compile_features(locus_library_playback_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_library_playback_tests PRIVATE locus_core raylib)
    add_test(NAME locus_library_playback_transaction COMMAND locus_library_playback_tests)

    # Library refresh must preserve the selected chart across reordering;
    # failed imports keep the prior view, not a half-updated menu selection.
    add_executable(locus_library_browser_tests
        tests/library_browser_tests.cpp
        src/app/library_browser.cpp
        src/app/library_file_actions.cpp
        src/app/menu_ui.cpp
        src/app/home_brand_assets.cpp
        src/app/game_settings.cpp
            ${LOCUS_SONG_LIBRARY_SOURCES}
        src/app/song_archive.cpp
        src/app/chart_file.cpp
        src/app/chart_save.cpp
        src/app/local_file_io.cpp
        src/app/song_set_storage.cpp
)
    target_compile_features(locus_library_browser_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_library_browser_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    add_test(NAME locus_library_browser_selection COMMAND locus_library_browser_tests)

    add_executable(locus_song_library_tests
        tests/song_library_tests.cpp
            ${LOCUS_SONG_LIBRARY_SOURCES}
        src/app/song_archive.cpp
        src/app/chart_file.cpp
        src/app/chart_save.cpp
        src/app/local_file_io.cpp
        src/app/song_set_storage.cpp
)
    target_compile_features(locus_song_library_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_song_library_tests PRIVATE locus_core nlohmann_json::nlohmann_json)
    add_test(NAME locus_persistent_song_library COMMAND locus_song_library_tests)

    add_executable(locus_game_settings_tests
        tests/game_settings_tests.cpp
        src/app/game_settings.cpp
    )
    target_compile_features(locus_game_settings_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_game_settings_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json
    )
    add_test(NAME locus_persistent_game_settings COMMAND locus_game_settings_tests)

    add_executable(locus_global_offset_calibration_tests
        tests/global_offset_calibration_tests.cpp)
    target_compile_features(locus_global_offset_calibration_tests PRIVATE cxx_std_17)
    target_include_directories(locus_global_offset_calibration_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_global_audio_offset_calibration
             COMMAND locus_global_offset_calibration_tests)

    # New UI foundation is pure presentation/model code, no window required.
    add_executable(locus_ui_foundation_tests tests/ui_foundation_tests.cpp)
    target_compile_features(locus_ui_foundation_tests PRIVATE cxx_std_17)
    target_include_directories(locus_ui_foundation_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(locus_ui_foundation_tests PRIVATE raylib)
    add_test(NAME locus_ui_foundation COMMAND locus_ui_foundation_tests)

    # Skin Load/Save/Save As/Reset must not publish an invalid draft,
    # and canceled native profile dialogs must preserve active appearance.
    add_executable(locus_skin_profile_actions_tests
        tests/skin_profile_actions_tests.cpp
        src/app/skin_profile_actions.cpp
        src/app/open_folder.cpp
        src/app/song_set_storage.cpp
        src/app/skin.cpp
    )
    target_compile_features(locus_skin_profile_actions_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_skin_profile_actions_tests PRIVATE
        raylib nlohmann_json::nlohmann_json)
    target_include_directories(locus_skin_profile_actions_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_skin_profile_file_actions COMMAND locus_skin_profile_actions_tests)

    add_executable(locus_skin_tests
        tests/skin_tests.cpp
        src/app/skin.cpp
        src/app/skin_controls.cpp
    )
    target_compile_features(locus_skin_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_skin_tests PRIVATE raylib nlohmann_json::nlohmann_json)
    target_include_directories(locus_skin_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_independent_skin_json COMMAND locus_skin_tests)

    add_executable(locus_judgement_tests tests/judgement_tests.cpp)
    target_compile_features(locus_judgement_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_judgement_tests PRIVATE locus_core)
    add_test(NAME locus_turn_judgement COMMAND locus_judgement_tests)
endif()

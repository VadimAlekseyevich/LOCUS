cmake_minimum_required(VERSION 3.25)

# LOCUS game and headless tests.
project(LOCUS VERSION 0.1.0 LANGUAGES C CXX)

option(LOCUS_BUILD_TESTS "Build and register core tests with CTest" OFF)

# Pinned raylib and JSON dependencies.
include(cmake/dependencies.cmake)

add_library(locus_core STATIC
    src/core/route.cpp
    src/core/arrow_chord.cpp
    src/core/route_view.cpp
    src/core/judgement.cpp
    src/core/song_clock.cpp
    src/core/demo_audio.cpp
    src/core/music_file.cpp
    src/core/chart_json.cpp
    src/core/chart_editor.cpp
)
target_compile_features(locus_core PUBLIC cxx_std_17)
target_link_libraries(locus_core PRIVATE nlohmann_json::nlohmann_json)
set_target_properties(locus_core PROPERTIES
    CXX_EXTENSIONS OFF
)
target_include_directories(locus_core PUBLIC
    "${PROJECT_SOURCE_DIR}/src"
)

add_executable(locus
    src/app/main.cpp
    src/app/app_input.cpp
    src/app/menu_controller.cpp
    src/app/gameplay_ui.cpp
    src/app/screen_flow.cpp
    src/app/session_transition.cpp
    src/app/quit_media.cpp
    src/app/native_close.cpp
    src/app/demo_transport.cpp
    src/app/audio_device.cpp
    src/app/local_music.cpp
    src/app/local_file_io.cpp
    src/app/music_picker.cpp
    src/app/chart_file.cpp
    src/app/chart_save.cpp
    src/app/editor_document.cpp
    src/app/chart_playback.cpp
    src/app/game_session.cpp
    src/app/song_library.cpp
    src/app/audio_revision.cpp
    src/app/game_settings.cpp
    src/app/skin.cpp
    src/app/skin_controls.cpp
    src/app/skin_menu.cpp
    src/app/menu_ui.cpp
    src/app/chart_editor_ui.cpp
    src/app/chart_editor_actions.cpp
    src/app/route_renderer.cpp
)
target_link_libraries(locus PRIVATE locus_core raylib nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(locus PRIVATE comdlg32 user32)
endif()
set_target_properties(locus PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(locus_core PRIVATE /W4 /permissive-)
    target_compile_options(locus PRIVATE /W4 /permissive-)
else()
    target_compile_options(locus_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(locus PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LOCUS_BUILD_TESTS)
    enable_testing()
    add_executable(locus_route_tests tests/route_tests.cpp)
    target_compile_features(locus_route_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_route_tests PRIVATE locus_core)
    add_test(NAME locus_demo_route COMMAND locus_route_tests)

    add_executable(locus_arrow_chord_tests tests/arrow_chord_tests.cpp)
    target_compile_features(locus_arrow_chord_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_arrow_chord_tests PRIVATE locus_core)
    add_test(NAME locus_diagonal_arrow_chords COMMAND locus_arrow_chord_tests)

    add_executable(locus_route_view_tests tests/route_view_tests.cpp)
    target_compile_features(locus_route_view_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_route_view_tests PRIVATE locus_core)
    add_test(NAME locus_route_view COMMAND locus_route_view_tests)

    add_executable(locus_music_tests tests/music_tests.cpp)
    target_compile_features(locus_music_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_music_tests PRIVATE locus_core)
    add_test(NAME locus_demo_audio_clock COMMAND locus_music_tests)

    add_executable(locus_music_file_tests tests/music_file_tests.cpp)
    target_compile_features(locus_music_file_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_music_file_tests PRIVATE locus_core)
    add_test(NAME locus_local_music_files COMMAND locus_music_file_tests)

    add_executable(locus_chart_json_tests tests/chart_json_tests.cpp)
    target_compile_features(locus_chart_json_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_chart_json_tests PRIVATE locus_core)
    add_test(NAME locus_chart_json_compatibility COMMAND locus_chart_json_tests)
    # Keep the shipped example checked by the same parser as real files.
    set_tests_properties(locus_chart_json_compatibility PROPERTIES
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

    add_executable(locus_chart_editor_tests tests/chart_editor_tests.cpp)
    target_compile_features(locus_chart_editor_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_chart_editor_tests PRIVATE locus_core)
    add_test(NAME locus_chart_editing COMMAND locus_chart_editor_tests)

    # Editor input/hitboxes are pure while no window is created.
    add_executable(locus_editor_ui_tests
        tests/editor_ui_tests.cpp
        src/app/chart_editor_ui.cpp
        src/app/chart_editor_actions.cpp
        src/app/game_settings.cpp
        src/app/route_renderer.cpp
    )
    target_compile_features(locus_editor_ui_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_ui_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    add_test(NAME locus_editor_controls COMMAND locus_editor_ui_tests)

    add_executable(locus_chart_file_tests
        tests/chart_file_tests.cpp
        src/app/chart_file.cpp
        src/app/chart_save.cpp
        src/app/local_file_io.cpp
    )
    target_compile_features(locus_chart_file_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_chart_file_tests PRIVATE locus_core)
    add_test(NAME locus_chart_local_file COMMAND locus_chart_file_tests)

    # Document load/save orchestration must preserve an unsaved editor draft
    # across malformed charts, failed audio validation and failed saves.
    add_executable(locus_editor_document_tests
        tests/editor_document_tests.cpp
        src/app/editor_document.cpp
        src/app/local_music.cpp
        src/app/local_file_io.cpp
        src/app/chart_file.cpp
        src/app/chart_save.cpp
        src/app/music_picker.cpp
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

    # Exit modal policy is pure; test playback state changes without opening
    # a native window or relying on an actual sound device.
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

    add_executable(locus_path_fade_tests tests/path_fade_tests.cpp)
    target_compile_features(locus_path_fade_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_path_fade_tests PRIVATE raylib)
    target_include_directories(locus_path_fade_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_forward_only_path_fade COMMAND locus_path_fade_tests)

    add_executable(locus_hit_flash_tests tests/hit_flash_tests.cpp)
    target_compile_features(locus_hit_flash_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_hit_flash_tests PRIVATE locus_core)
    add_test(NAME locus_player_hit_flash COMMAND locus_hit_flash_tests)

    add_executable(locus_song_library_tests
        tests/song_library_tests.cpp
        src/app/song_library.cpp
        src/app/audio_revision.cpp
        src/app/chart_file.cpp
        src/app/chart_save.cpp
        src/app/local_file_io.cpp
    )
    target_compile_features(locus_song_library_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_song_library_tests PRIVATE locus_core)
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

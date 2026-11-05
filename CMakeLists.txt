cmake_minimum_required(VERSION 3.25)

# Production LOCUS build. The A-07 raylib probe is intentionally separate.
project(LOCUS VERSION 0.1.0 LANGUAGES C CXX)

option(LOCUS_BUILD_TESTS "Build and register core tests with CTest" OFF)
option(LOCUS_VERIFY_DEPENDENCIES "Build a headless dependency integration smoke executable" ON)

# Versions and source revisions are pinned in this small CMake module (F-02).
include(cmake/dependencies.cmake)

add_library(locus_core STATIC
    src/core/foundation.cpp
    src/core/route.cpp
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
    src/app/demo_transport.cpp
    src/app/local_music.cpp
    src/app/local_file_io.cpp
    src/app/music_picker.cpp
    src/app/chart_file.cpp
    src/app/chart_save.cpp
    src/app/chart_playback.cpp
    src/app/chart_editor_ui.cpp
    src/app/route_renderer.cpp
)
target_link_libraries(locus PRIVATE locus_core raylib)
if(WIN32)
    target_link_libraries(locus PRIVATE comdlg32)
endif()
set_target_properties(locus PROPERTIES
    CXX_EXTENSIONS OFF
)

# Dependency compatibility is checked without opening a window or audio device.
# This target is a build-time verification only, not a CTest suite (F-04).
if(LOCUS_VERIFY_DEPENDENCIES)
    add_executable(locus_dependencies_smoke tests/dependency_smoke.cpp)
    target_compile_features(locus_dependencies_smoke PRIVATE cxx_std_17)
    target_link_libraries(locus_dependencies_smoke PRIVATE
        raylib locus_raygui nlohmann_json::nlohmann_json
    )
endif()

if(MSVC)
    target_compile_options(locus_core PRIVATE /W4 /permissive-)
    target_compile_options(locus PRIVATE /W4 /permissive-)
else()
    target_compile_options(locus_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(locus PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LOCUS_BUILD_TESTS)
    enable_testing()
    add_executable(locus_core_tests tests/core_tests.cpp)
    target_compile_features(locus_core_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_core_tests PRIVATE locus_core)
    add_test(NAME locus_core_foundation COMMAND locus_core_tests)

    add_executable(locus_route_tests tests/route_tests.cpp)
    target_compile_features(locus_route_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_route_tests PRIVATE locus_core)
    add_test(NAME locus_demo_route COMMAND locus_route_tests)

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
    add_test(NAME locus_chart_v1_json COMMAND locus_chart_json_tests)

    add_executable(locus_chart_editor_tests tests/chart_editor_tests.cpp)
    target_compile_features(locus_chart_editor_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_chart_editor_tests PRIVATE locus_core)
    add_test(NAME locus_chart_editing COMMAND locus_chart_editor_tests)

    # Editor input/hitboxes are pure while no window is created.
    add_executable(locus_editor_ui_tests
        tests/editor_ui_tests.cpp
        src/app/chart_editor_ui.cpp
        src/app/route_renderer.cpp
    )
    target_compile_features(locus_editor_ui_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_ui_tests PRIVATE locus_core raylib)
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

    add_executable(locus_judgement_tests tests/judgement_tests.cpp)
    target_compile_features(locus_judgement_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_judgement_tests PRIVATE locus_core)
    add_test(NAME locus_turn_judgement COMMAND locus_judgement_tests)
endif()

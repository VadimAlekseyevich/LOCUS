cmake_minimum_required(VERSION 3.25)

# LOCUS game and headless tests.
project(LOCUS VERSION 0.1.0 LANGUAGES C CXX)

option(LOCUS_BUILD_TESTS "Build and register tests with CTest" OFF)
option(LOCUS_BUILD_APP "Build the desktop game and application-side tests" ON)
option(LOCUS_BUILD_BENCHMARKS "Build optional headless route preview profiler" OFF)
option(LOCUS_ENABLE_RMLUI "Use responsive RmlUi for non-gameplay UI screens" ON)

# The core configuration never fetches graphics, font or audio libraries.
include(cmake/core_dependencies.cmake)

add_library(locus_core STATIC
    src/core/route.cpp
    src/core/route_view.cpp
    src/core/timed_route.cpp
    src/core/judgement.cpp
    src/core/replay.cpp
    src/core/score_verification.cpp
    src/core/song_clock.cpp
    src/core/demo_audio.cpp
    src/core/music_file.cpp
    src/core/chart_json.cpp
    src/core/chart_editor.cpp
    src/core/chart_difficulty.cpp
    src/core/performance.cpp
    src/core/unicode_search.cpp
)
target_compile_features(locus_core PUBLIC cxx_std_17)
target_link_libraries(locus_core PRIVATE nlohmann_json::nlohmann_json utf8proc::utf8proc)
set_target_properties(locus_core PROPERTIES
    CXX_EXTENSIONS OFF
)
target_include_directories(locus_core PUBLIC
    "${PROJECT_SOURCE_DIR}/src"
)

if(MSVC)
    # UTF-8 must also apply to the core-only test targets.
    add_compile_options(/utf-8)
    target_compile_options(locus_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(locus_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Portable package codec/validation; no desktop or audio dependencies.
add_library(locus_package STATIC
    src/app/song_set_manifest.cpp
    src/app/song_archive.cpp
    src/app/package_validation.cpp
    src/app/publication_revision.cpp
)
target_link_libraries(locus_package PUBLIC locus_core PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(locus_package SYSTEM PRIVATE "${picosha2_SOURCE_DIR}")
set_target_properties(locus_package PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(locus_package PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(locus_package PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Portable local attempt model; persistence and account identity remain adapters.
add_library(locus_result_model STATIC src/app/run_result_model.cpp)
target_link_libraries(locus_result_model PUBLIC locus_core)
set_target_properties(locus_result_model PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(locus_result_model PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(locus_result_model PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(locus_player_profile STATIC src/app/player_profile.cpp)
target_link_libraries(locus_player_profile PUBLIC locus_core PRIVATE nlohmann_json::nlohmann_json)
set_target_properties(locus_player_profile PROPERTIES CXX_EXTENSIONS OFF)

# Native OAuth boundary: portable callback validation, Windows CNG primitives.
# No dependency on gameplay, graphics, audio, or a third-party crypto library.
add_library(locus_native_oauth STATIC src/app/native_oauth.cpp)
target_compile_features(locus_native_oauth PUBLIC cxx_std_17)
target_include_directories(locus_native_oauth PUBLIC "${PROJECT_SOURCE_DIR}/src")
set_target_properties(locus_native_oauth PROPERTIES CXX_EXTENSIONS OFF)
if(WIN32)
    target_sources(locus_native_oauth PRIVATE src/app/native_oauth_win32.cpp)
    target_link_libraries(locus_native_oauth PRIVATE bcrypt)
endif()
if(MSVC)
    target_compile_options(locus_native_oauth PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(locus_native_oauth PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(LOCUS_BUILD_TESTS)
    enable_testing()
    add_executable(locus_native_oauth_tests tests/native_oauth_tests.cpp)
    target_link_libraries(locus_native_oauth_tests PRIVATE locus_native_oauth)
    add_test(NAME locus_native_oauth_contract COMMAND locus_native_oauth_tests)
endif()

# Internal offline worker boundary, built in the Linux core configuration.
add_library(locus_verification_job STATIC workers/verification_job.cpp)
target_link_libraries(locus_verification_job PUBLIC locus_package)
target_include_directories(locus_verification_job PUBLIC "${PROJECT_SOURCE_DIR}/workers")
add_executable(locus_worker workers/main.cpp)
target_link_libraries(locus_worker PRIVATE locus_verification_job nlohmann_json::nlohmann_json)
set_target_properties(locus_worker locus_verification_job PROPERTIES CXX_EXTENSIONS OFF)

if(LOCUS_BUILD_TESTS)
    enable_testing()
    include(cmake/core_tests.cmake)
endif()
if(LOCUS_BUILD_BENCHMARKS)
    add_executable(locus_editor_bench benchmarks/editor_bench.cpp)
    target_compile_features(locus_editor_bench PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_bench PRIVATE locus_core)
endif()

include(cmake/verify_source_ownership.cmake)
if(NOT LOCUS_BUILD_APP)
    locus_verify_source_ownership()
    return()
endif()

include(cmake/dependencies.cmake)
find_package(Threads REQUIRED)
include(cmake/storage_targets.cmake)
include(cmake/runtime_targets.cmake)
include(cmake/editor_targets.cmake)
include(cmake/application_targets.cmake)

add_executable(locus
    src/app/main.cpp
    src/app/app_smoke_checks.cpp
    src/app/app_frame_renderer.cpp
    src/app/image_asset_texture.cpp
    src/app/app_input.cpp
    src/app/menu_controller.cpp
    src/app/gameplay_ui.cpp
    src/app/gameplay_hud.cpp
    src/app/native_close.cpp
    src/app/window_mode.cpp
    src/app/menu_music_preview.cpp
    src/app/editor_screen_actions.cpp
    src/app/library_launch_actions.cpp
    src/app/skin_menu_draw.cpp
)
target_link_libraries(locus PRIVATE locus_skin_profiles locus_player_profile)
target_link_libraries(locus PRIVATE locus_listen_session)
target_link_libraries(locus PRIVATE locus_screen_flow locus_session_actions locus_play_presentation locus_library_activation locus_library_browser locus_brand_assets locus_menu_canvas locus_native_input)
target_link_libraries(locus PRIVATE locus_platform_dialogs locus_game_settings locus_skin_model locus_editor_comparison locus_editor_controls locus_editor_canvas locus_editor_document locus_editor_workspace locus_editor_session locus_editor_discard)
target_link_libraries(locus PRIVATE locus_transport locus_game_session locus_result_store locus_path_mesh locus_route_renderer)
target_link_libraries(locus PRIVATE locus_song_library locus_file_io locus_chart_storage locus_library_selection)
if(LOCUS_ENABLE_RMLUI)
    target_sources(locus PRIVATE
        src/app/rml_raylib_renderer.cpp
        src/app/native_menu_ui.cpp
        src/app/native_settings_ui.cpp
    )
    target_link_libraries(locus PRIVATE locus_native_editor locus_native_library locus_native_skin)
    target_link_libraries(locus PRIVATE RmlUi::Core)
    target_compile_definitions(locus PRIVATE LOCUS_WITH_RMLUI=1)
endif()

# Runtime assets are staged independently of the executable, so updating
# RML/RCSS files never depends on a relink or a full application rebuild.
set(LOCUS_UI_SOURCE_DIR "${PROJECT_SOURCE_DIR}/assets/ui")
include(cmake/ui_assets_manifest.cmake)
# Compile the same manifest into the no-GPU packaged asset check.
set(LOCUS_ASSET_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/locus_required_assets.h")
set(LOCUS_ASSET_HEADER_TEXT "#pragma once\ninline constexpr const char* kLocusRequiredUiAssets[] = {\n")
foreach(relative IN LISTS LOCUS_REQUIRED_UI_ASSETS)
    string(APPEND LOCUS_ASSET_HEADER_TEXT "    \"${relative}\",\n")
endforeach()
string(APPEND LOCUS_ASSET_HEADER_TEXT "};\n")
file(GENERATE OUTPUT "${LOCUS_ASSET_HEADER}" CONTENT "${LOCUS_ASSET_HEADER_TEXT}")
target_include_directories(locus PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
foreach(relative IN LISTS LOCUS_REQUIRED_UI_ASSETS)
    if(NOT EXISTS "${LOCUS_UI_SOURCE_DIR}/${relative}")
        message(FATAL_ERROR "Missing required LOCUS UI asset: ${relative}")
    endif()
endforeach()

# CONFIGURE_DEPENDS detects added/removed files; a generated file-inventory
# dependency also invalidates staging on removals, when the deleted file is
# no longer in DEPENDS (and otherwise could leave stale packaged resources).
file(GLOB_RECURSE LOCUS_UI_ASSET_FILES CONFIGURE_DEPENDS
    LIST_DIRECTORIES false "${LOCUS_UI_SOURCE_DIR}/*")
list(SORT LOCUS_UI_ASSET_FILES)
set(LOCUS_UI_FILE_LIST "${CMAKE_CURRENT_BINARY_DIR}/ui-assets/source-files.txt")
file(GENERATE OUTPUT "${LOCUS_UI_FILE_LIST}"
    CONTENT "${LOCUS_UI_ASSET_FILES}\n")
set(LOCUS_UI_STAMP "${CMAKE_CURRENT_BINARY_DIR}/ui-assets/$<CONFIG>/staged.stamp")
add_custom_command(
    OUTPUT "${LOCUS_UI_STAMP}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/ui-assets/$<CONFIG>"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf
        "$<TARGET_FILE_DIR:locus>/assets/ui"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${LOCUS_UI_SOURCE_DIR}" "$<TARGET_FILE_DIR:locus>/assets/ui"
    COMMAND "${CMAKE_COMMAND}"
        "-DLOCUS_UI_ASSETS_DIR=$<TARGET_FILE_DIR:locus>/assets/ui"
        "-DLOCUS_UI_SOURCE_DIR=${LOCUS_UI_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/cmake/verify_ui_assets.cmake"
    COMMAND "${CMAKE_COMMAND}" -E touch "${LOCUS_UI_STAMP}"
    DEPENDS ${LOCUS_UI_ASSET_FILES} "${LOCUS_UI_FILE_LIST}"
        "${PROJECT_SOURCE_DIR}/cmake/ui_assets_manifest.cmake"
        "${PROJECT_SOURCE_DIR}/cmake/verify_ui_assets.cmake"
    COMMENT "Synchronizing LOCUS UI resources beside the executable"
    VERBATIM
)
add_custom_target(locus_ui_assets ALL DEPENDS "${LOCUS_UI_STAMP}")
add_dependencies(locus locus_ui_assets)

# The same portable layout is used by local installs and Windows CI artifacts.
install(TARGETS locus RUNTIME DESTINATION ".")
install(FILES "${picosha2_SOURCE_DIR}/LICENSE" DESTINATION "licenses" RENAME PicoSHA2.txt)
install(DIRECTORY "${LOCUS_UI_SOURCE_DIR}/" DESTINATION "assets/ui")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/examples/" DESTINATION "examples")
if(WIN32)
    install(FILES "${PROJECT_SOURCE_DIR}/cmake/portable/Start LOCUS.bat" DESTINATION ".")
    # App-local release CRT. Use the toolchain's redistributable DLLs rather
    # than requiring Visual Studio or a system-wide installation.
    if(MSVC)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
        include(InstallRequiredSystemLibraries)
    endif()
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_FILE_NAME "LOCUS-windows-x64")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    include(CPack)
endif()
target_link_libraries(locus PRIVATE locus_core raylib locus_soundtouch nlohmann_json::nlohmann_json Threads::Threads)
if(WIN32)
    target_link_libraries(locus PRIVATE comdlg32 user32)
endif()
set_target_properties(locus PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(locus PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(locus PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LOCUS_BUILD_BENCHMARKS)
    add_executable(locus_route_preview_bench
        benchmarks/route_preview_bench.cpp
    )
    target_link_libraries(locus_route_preview_bench PRIVATE locus_path_mesh)
    target_compile_features(locus_route_preview_bench PRIVATE cxx_std_17)
    target_link_libraries(locus_route_preview_bench PRIVATE locus_core raylib)
endif()

if(LOCUS_BUILD_TESTS)
    add_test(NAME locus_packaged_ui_assets
        COMMAND "${CMAKE_COMMAND}"
            "-DLOCUS_UI_ASSETS_DIR=$<TARGET_FILE_DIR:locus>/assets/ui"
            "-DLOCUS_UI_SOURCE_DIR=${LOCUS_UI_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/verify_ui_assets.cmake")
    if(LOCUS_ENABLE_RMLUI)
        add_executable(locus_rml_layout_tests tests/rml_ui_layout_tests.cpp)
        target_compile_features(locus_rml_layout_tests PRIVATE cxx_std_17)
        target_include_directories(locus_rml_layout_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
        target_link_libraries(locus_rml_layout_tests PRIVATE RmlUi::Core)
        add_dependencies(locus_rml_layout_tests locus_ui_assets)
        add_test(NAME locus_rml_responsive_play
                 COMMAND locus_rml_layout_tests)
        set_tests_properties(locus_rml_responsive_play PROPERTIES
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:locus>")
    endif()
    # A production navigation -> judgement -> persisted-results journey,
    # without a GPU, device clock or native file dialogs.
    # T03: timestamped physical key transitions, independent of rendered FPS.
    if(WIN32)
        # Exercises the production Win32 key translator and time-origin
        # adapter without requiring a desktop or physical keyboard.
        add_executable(locus_win32_direction_events_tests
            tests/win32_direction_events_tests.cpp
        )
        target_link_libraries(locus_win32_direction_events_tests PRIVATE locus_native_input)
        target_compile_features(locus_win32_direction_events_tests PRIVATE cxx_std_17)
        target_link_libraries(locus_win32_direction_events_tests PRIVATE
            locus_core user32)
        add_test(NAME locus_win32_message_time_and_key_mapping
                 COMMAND locus_win32_direction_events_tests)
    endif()

    # T04: transport time contract at 100%, 50%, 10%, silent preroll,
    # pause/rate changes, seek, finished audio and missing output device.
    add_executable(locus_clock_contract_tests
        tests/clock_contract_tests.cpp
    )
    target_link_libraries(locus_clock_contract_tests PRIVATE locus_transport locus_game_session)
    target_link_libraries(locus_clock_contract_tests PRIVATE locus_file_io)
    target_compile_features(locus_clock_contract_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_clock_contract_tests PRIVATE
        locus_core raylib locus_soundtouch)
    add_test(NAME locus_chart_clock_transport_contract
             COMMAND locus_clock_contract_tests)

    add_executable(locus_app_journey_tests
        tests/app_journey_tests.cpp
    )
    target_link_libraries(locus_app_journey_tests PRIVATE locus_screen_flow)
    target_link_libraries(locus_app_journey_tests PRIVATE locus_result_store)
    target_link_libraries(locus_app_journey_tests PRIVATE locus_song_library locus_file_io locus_chart_storage)
    target_compile_features(locus_app_journey_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_app_journey_tests PRIVATE
        locus_core nlohmann_json::nlohmann_json)
    add_test(NAME locus_navigation_judgement_results_journey
             COMMAND locus_app_journey_tests)
    add_executable(locus_run_results_tests
        tests/run_results_tests.cpp
    )
    target_link_libraries(locus_run_results_tests PRIVATE locus_result_store)
    target_compile_features(locus_run_results_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_run_results_tests PRIVATE
        locus_core nlohmann_json::nlohmann_json Threads::Threads)
    add_test(NAME locus_completed_run_history COMMAND locus_run_results_tests)

    add_executable(locus_results_background_tests
        tests/results_background_tests.cpp)
    target_compile_features(locus_results_background_tests PRIVATE cxx_std_17)
    target_include_directories(locus_results_background_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(locus_results_background_tests PRIVATE locus_core)
    add_test(NAME locus_saved_result_background
             COMMAND locus_results_background_tests)

    add_executable(locus_quit_dialog_input_tests
        tests/quit_dialog_input_tests.cpp)
    target_compile_features(locus_quit_dialog_input_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_quit_dialog_input_tests PRIVATE locus_core raylib)
    add_test(NAME locus_quit_dialog_input COMMAND locus_quit_dialog_input_tests)

    # Editor actions and hitboxes run headlessly; native polling is only linked.
    add_executable(locus_editor_ui_tests
        tests/editor_ui_tests.cpp
    )
    target_link_libraries(locus_editor_ui_tests PRIVATE locus_game_settings locus_editor_comparison locus_editor_controls locus_editor_canvas)
    target_link_libraries(locus_editor_ui_tests PRIVATE locus_path_mesh locus_route_renderer)
    target_compile_features(locus_editor_ui_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_ui_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    add_test(NAME locus_editor_controls COMMAND locus_editor_ui_tests)
    if(LOCUS_ENABLE_RMLUI)
        target_sources(locus_editor_ui_tests PRIVATE
            tests/native_editor_ui_tests.cpp
        )
        target_link_libraries(locus_editor_ui_tests PRIVATE locus_native_editor)
        target_compile_definitions(locus_editor_ui_tests PRIVATE LOCUS_TEST_NATIVE_EDITOR=1)
        target_link_libraries(locus_editor_ui_tests PRIVATE RmlUi::Core)
        add_dependencies(locus_editor_ui_tests locus_ui_assets)
        set_tests_properties(locus_editor_controls PROPERTIES
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:locus>")
    endif()

    add_executable(locus_editor_comparison_tests
        tests/editor_comparison_tests.cpp
    )
    target_link_libraries(locus_editor_comparison_tests PRIVATE locus_editor_comparison)
    target_compile_features(locus_editor_comparison_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_comparison_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    add_test(NAME locus_editor_comparison COMMAND locus_editor_comparison_tests)

    add_executable(locus_chart_file_tests
        tests/chart_file_tests.cpp
    )
    target_link_libraries(locus_chart_file_tests PRIVATE locus_file_io locus_chart_storage)
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
    )
    target_link_libraries(locus_listen_session_tests PRIVATE locus_listen_session)
    target_link_libraries(locus_listen_session_tests PRIVATE locus_transport)
    target_link_libraries(locus_listen_session_tests PRIVATE locus_file_io)
    target_compile_features(locus_listen_session_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_listen_session_tests PRIVATE locus_core raylib)
    add_test(NAME locus_listening_picker_lifecycle COMMAND locus_listen_session_tests)

    add_executable(locus_editor_file_actions_tests
        tests/editor_file_actions_tests.cpp
    )
    target_link_libraries(locus_editor_file_actions_tests PRIVATE locus_platform_dialogs locus_editor_comparison locus_editor_document locus_editor_workspace)
    target_link_libraries(locus_editor_file_actions_tests PRIVATE locus_transport)
    target_link_libraries(locus_editor_file_actions_tests PRIVATE locus_song_library locus_file_io locus_chart_storage)
    target_compile_features(locus_editor_file_actions_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_file_actions_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    if(WIN32)
        target_link_libraries(locus_editor_file_actions_tests PRIVATE comdlg32 user32)
    endif()
    add_test(NAME locus_editor_file_actions_lifecycle COMMAND locus_editor_file_actions_tests)

    # End-to-end editor transaction on real app controllers and a real WAV;
    # only the sound device, file-dialog result and clock are injected.
    add_executable(locus_editor_workflow_tests
        tests/editor_workflow_tests.cpp
    )
    target_link_libraries(locus_editor_workflow_tests PRIVATE locus_screen_flow locus_session_actions locus_library_activation)
    target_link_libraries(locus_editor_workflow_tests PRIVATE locus_platform_dialogs locus_game_settings locus_editor_comparison locus_editor_controls locus_editor_canvas locus_editor_document locus_editor_workspace locus_editor_session locus_editor_discard)
    target_link_libraries(locus_editor_workflow_tests PRIVATE locus_transport locus_game_session locus_result_store locus_path_mesh locus_route_renderer)
    target_link_libraries(locus_editor_workflow_tests PRIVATE locus_song_library locus_file_io locus_chart_storage)
    target_compile_features(locus_editor_workflow_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_workflow_tests PRIVATE
        locus_core raylib locus_soundtouch nlohmann_json::nlohmann_json)
    if(WIN32)
        target_link_libraries(locus_editor_workflow_tests PRIVATE comdlg32 user32)
    endif()
    add_test(NAME locus_editor_to_play_to_results_journey
             COMMAND locus_editor_workflow_tests)

    add_executable(locus_editor_session_tests
        tests/editor_session_tests.cpp
    )
    target_link_libraries(locus_editor_session_tests PRIVATE locus_game_settings locus_editor_comparison locus_editor_controls locus_editor_canvas locus_editor_session)
    target_link_libraries(locus_editor_session_tests PRIVATE locus_transport locus_game_session locus_path_mesh locus_route_renderer)
    target_link_libraries(locus_editor_session_tests PRIVATE locus_file_io)
    target_compile_features(locus_editor_session_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_session_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    add_test(NAME locus_editor_session_lifecycle COMMAND locus_editor_session_tests)

    add_executable(locus_editor_document_tests
        tests/editor_document_tests.cpp
    )
    target_link_libraries(locus_editor_document_tests PRIVATE locus_platform_dialogs locus_editor_document)
    target_link_libraries(locus_editor_document_tests PRIVATE locus_transport)
    target_link_libraries(locus_editor_document_tests PRIVATE locus_file_io locus_chart_storage)
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
    )
    target_link_libraries(locus_session_transition_tests PRIVATE locus_screen_flow locus_session_actions)
    target_link_libraries(locus_session_transition_tests PRIVATE locus_transport locus_game_session)
    target_link_libraries(locus_session_transition_tests PRIVATE locus_file_io)
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
    )
    target_link_libraries(locus_play_presentation_tests PRIVATE locus_play_presentation)
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
    )
    target_link_libraries(locus_editor_discard_tests PRIVATE locus_editor_discard)
    target_compile_features(locus_editor_discard_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_editor_discard_tests PRIVATE locus_core raylib)
    add_test(NAME locus_editor_unsaved_discard_guard COMMAND locus_editor_discard_tests)

    add_executable(locus_quit_media_tests
        tests/quit_media_tests.cpp
    )
    target_link_libraries(locus_quit_media_tests PRIVATE locus_session_actions)
    target_link_libraries(locus_quit_media_tests PRIVATE locus_transport locus_game_session)
    target_link_libraries(locus_quit_media_tests PRIVATE locus_file_io)
    target_compile_features(locus_quit_media_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_quit_media_tests PRIVATE locus_core raylib)
    add_test(NAME locus_quit_modal_audio_lifecycle COMMAND locus_quit_media_tests)

    add_executable(locus_audio_lifecycle_tests
        tests/audio_lifecycle_tests.cpp
    )
    target_link_libraries(locus_audio_lifecycle_tests PRIVATE locus_transport)
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
    )
    target_link_libraries(locus_path_mesh_tests PRIVATE locus_path_mesh)
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
    )
    target_link_libraries(locus_library_playback_tests PRIVATE locus_library_activation)
    target_link_libraries(locus_library_playback_tests PRIVATE locus_transport locus_game_session)
    target_link_libraries(locus_library_playback_tests PRIVATE locus_file_io locus_chart_storage locus_song_library)
    target_compile_features(locus_library_playback_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_library_playback_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json)
    add_test(NAME locus_library_playback_transaction COMMAND locus_library_playback_tests)

    # Library refresh must preserve the selected chart across reordering;
    # failed imports keep the prior view, not a half-updated menu selection.
    add_executable(locus_library_browser_tests
        tests/library_browser_tests.cpp
    )
    target_link_libraries(locus_library_browser_tests PRIVATE locus_library_browser locus_brand_assets locus_menu_canvas)
    target_link_libraries(locus_library_browser_tests PRIVATE locus_game_settings)
    target_link_libraries(locus_library_browser_tests PRIVATE locus_result_store)
    target_link_libraries(locus_library_browser_tests PRIVATE locus_song_library locus_file_io locus_chart_storage locus_library_selection)
    target_compile_features(locus_library_browser_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_library_browser_tests PRIVATE
        locus_core raylib nlohmann_json::nlohmann_json Threads::Threads)
    add_test(NAME locus_library_browser_selection COMMAND locus_library_browser_tests)
    if(LOCUS_ENABLE_RMLUI)
        target_sources(locus_library_browser_tests PRIVATE
            tests/native_play_ui_tests.cpp
            tests/native_results_ui_tests.cpp
        )
        target_link_libraries(locus_library_browser_tests PRIVATE locus_native_library)
        target_compile_definitions(locus_library_browser_tests PRIVATE LOCUS_TEST_NATIVE_PLAY=1)
        target_link_libraries(locus_library_browser_tests PRIVATE RmlUi::Core)
        add_dependencies(locus_library_browser_tests locus_ui_assets)
        set_tests_properties(locus_library_browser_selection PROPERTIES
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:locus>")
    endif()

    add_executable(locus_song_library_tests
        tests/song_library_tests.cpp
    )
    target_link_libraries(locus_song_library_tests PRIVATE locus_song_library locus_file_io locus_chart_storage locus_library_selection)
    target_compile_features(locus_song_library_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_song_library_tests PRIVATE locus_core nlohmann_json::nlohmann_json)
    add_test(NAME locus_persistent_song_library COMMAND locus_song_library_tests)

    add_executable(locus_game_settings_tests
        tests/game_settings_tests.cpp
    )
    target_link_libraries(locus_game_settings_tests PRIVATE locus_game_settings)
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
    )
    target_link_libraries(locus_skin_profile_actions_tests PRIVATE locus_skin_profiles)
    target_link_libraries(locus_skin_profile_actions_tests PRIVATE locus_platform_dialogs locus_skin_model)
    target_link_libraries(locus_skin_profile_actions_tests PRIVATE locus_chart_storage)
    target_compile_features(locus_skin_profile_actions_tests PRIVATE cxx_std_17)
    target_link_libraries(locus_skin_profile_actions_tests PRIVATE
        raylib nlohmann_json::nlohmann_json)
    target_include_directories(locus_skin_profile_actions_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_skin_profile_file_actions COMMAND locus_skin_profile_actions_tests)

    add_executable(locus_skin_tests
        tests/skin_tests.cpp
    )
    target_link_libraries(locus_skin_tests PRIVATE locus_skin_model)
    target_compile_features(locus_skin_tests PRIVATE cxx_std_17)
    target_compile_definitions(locus_skin_tests PRIVATE LOCUS_HITSOUND_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/hitsounds")
    target_link_libraries(locus_skin_tests PRIVATE raylib nlohmann_json::nlohmann_json)
    target_include_directories(locus_skin_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME locus_independent_skin_json COMMAND locus_skin_tests)
    if(LOCUS_ENABLE_RMLUI)
        target_sources(locus_skin_tests PRIVATE
            tests/native_skin_ui_tests.cpp
        )
        target_link_libraries(locus_skin_tests PRIVATE locus_skin_model locus_native_skin)
        target_compile_definitions(locus_skin_tests PRIVATE LOCUS_TEST_NATIVE_SKIN=1)
        target_link_libraries(locus_skin_tests PRIVATE RmlUi::Core)
        add_dependencies(locus_skin_tests locus_ui_assets)
        set_tests_properties(locus_independent_skin_json PROPERTIES
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:locus>")
    endif()

endif()

locus_verify_source_ownership()

# Shared screen/session actions and library presentation. Main retains
# orchestration, platform polling, and resource lifecycle.

add_library(locus_screen_flow STATIC
    src/app/screen_flow.cpp
)
target_link_libraries(locus_screen_flow PUBLIC locus_core)

add_library(locus_session_actions STATIC
    src/app/app_runtime_policy.cpp
    src/app/play_screen_actions.cpp
    src/app/session_transition.cpp
    src/app/quit_media.cpp
)
target_link_libraries(locus_session_actions PUBLIC locus_game_session locus_screen_flow)

add_library(locus_play_presentation STATIC
    src/app/play_presentation.cpp
)
target_link_libraries(locus_play_presentation PUBLIC locus_core raylib)

add_library(locus_library_activation STATIC
    src/app/library_playback.cpp
)
target_link_libraries(locus_library_activation PUBLIC locus_song_library locus_game_session
    PRIVATE locus_result_store)

add_library(locus_library_browser STATIC
    src/app/library_browser.cpp
    src/app/async_library_refresh.cpp
    src/app/library_file_actions.cpp
)
target_link_libraries(locus_library_browser PUBLIC locus_library_selection raylib
    PRIVATE Threads::Threads)

add_library(locus_brand_assets STATIC
    src/app/home_brand_assets.cpp
    src/app/executable_directory.cpp
)
target_link_libraries(locus_brand_assets PUBLIC locus_core raylib)

add_library(locus_menu_canvas STATIC
    src/app/menu_ui.cpp
)
target_link_libraries(locus_menu_canvas PUBLIC locus_brand_assets locus_game_settings locus_library_selection locus_result_store)

add_library(locus_native_input STATIC
    src/app/native_key_capture.cpp
)
target_link_libraries(locus_native_input PUBLIC locus_core)

add_library(locus_listen_session STATIC src/app/listen_session.cpp)
target_link_libraries(locus_listen_session PUBLIC locus_transport locus_screen_flow)

add_library(locus_skin_profiles STATIC src/app/skin_profile_actions.cpp)
target_link_libraries(locus_skin_profiles PUBLIC locus_skin_model locus_chart_storage locus_platform_dialogs)

if(WIN32)
    target_link_libraries(locus_native_input PRIVATE user32)
endif()
foreach(module IN ITEMS locus_screen_flow locus_session_actions locus_play_presentation locus_library_activation locus_library_browser locus_brand_assets locus_menu_canvas locus_native_input locus_listen_session locus_skin_profiles)
    set_target_properties(${module} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${module} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${module} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

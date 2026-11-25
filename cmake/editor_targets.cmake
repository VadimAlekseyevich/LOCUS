# Shared editor document, input, presentation and platform adapter targets.
# Keep authoring and session logic separately linkable from drawing.

add_library(locus_platform_dialogs STATIC
    src/app/open_folder.cpp
    src/app/music_picker.cpp
)
target_link_libraries(locus_platform_dialogs PUBLIC locus_core)

add_library(locus_game_settings STATIC
    src/app/game_settings.cpp
)
target_link_libraries(locus_game_settings PUBLIC locus_core raylib
    PRIVATE nlohmann_json::nlohmann_json)

add_library(locus_skin_model STATIC
    src/app/skin.cpp
    src/app/skin_controls.cpp
    src/app/skin_menu.cpp
)
target_link_libraries(locus_skin_model PUBLIC locus_core raylib
    PRIVATE nlohmann_json::nlohmann_json)

add_library(locus_editor_comparison STATIC
    src/app/editor_comparison.cpp
)
target_link_libraries(locus_editor_comparison PUBLIC locus_core raylib)

add_library(locus_editor_controls STATIC
    src/app/chart_editor_actions.cpp
    src/app/editor_timeline_actions.cpp
    src/app/editor_pattern_actions.cpp
    src/app/editor_text_fields.cpp
    src/app/editor_input.cpp
)
target_link_libraries(locus_editor_controls PUBLIC locus_core raylib)

add_library(locus_editor_canvas STATIC
    src/app/chart_editor_ui.cpp
)
target_link_libraries(locus_editor_canvas PUBLIC locus_editor_comparison locus_route_renderer)

add_library(locus_editor_document STATIC
    src/app/editor_document.cpp
)
target_link_libraries(locus_editor_document PUBLIC locus_chart_storage locus_transport
    PRIVATE locus_platform_dialogs)

add_library(locus_editor_workspace STATIC
    src/app/editor_file_actions.cpp
    src/app/editor_set_controller.cpp
)
target_link_libraries(locus_editor_workspace PUBLIC locus_editor_document locus_song_library locus_editor_comparison
    PRIVATE locus_platform_dialogs locus_library_selection)

add_library(locus_editor_session STATIC
    src/app/editor_session.cpp
)
target_link_libraries(locus_editor_session PUBLIC locus_editor_controls locus_game_session)

add_library(locus_editor_discard STATIC
    src/app/editor_discard_guard.cpp
)
target_link_libraries(locus_editor_discard PUBLIC locus_core raylib)

if(WIN32)
    target_link_libraries(locus_platform_dialogs PRIVATE comdlg32 shell32 user32)
endif()

set(LOCUS_EDITOR_TARGETS locus_platform_dialogs locus_game_settings locus_skin_model locus_editor_comparison locus_editor_controls locus_editor_canvas locus_editor_document locus_editor_workspace locus_editor_session locus_editor_discard)
if(LOCUS_ENABLE_RMLUI)
    add_library(locus_native_editor STATIC
        src/app/native_editor_ui.cpp
    )
    target_link_libraries(locus_native_editor PUBLIC RmlUi::Core locus_editor_comparison locus_game_settings)

    add_library(locus_native_library STATIC
        src/app/native_play_ui.cpp
        src/app/native_results_ui.cpp
    )
    target_link_libraries(locus_native_library PUBLIC RmlUi::Core locus_library_selection locus_result_store raylib)

    add_library(locus_native_skin STATIC
        src/app/native_skin_ui.cpp
    )
    target_link_libraries(locus_native_skin PUBLIC RmlUi::Core locus_skin_model)
    list(APPEND LOCUS_EDITOR_TARGETS locus_native_editor locus_native_library locus_native_skin)
endif()

foreach(module IN LISTS LOCUS_EDITOR_TARGETS)
    set_target_properties(${module} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${module} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${module} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
unset(LOCUS_EDITOR_TARGETS)

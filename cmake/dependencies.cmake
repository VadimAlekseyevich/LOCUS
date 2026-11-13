# Pinned, reproducible source dependencies for LOCUS.
# No global package paths or user-specific install locations are required.

include(FetchContent)

# With immutable commit pins, reconfiguration may update an existing source
# if the pin is deliberately changed. Fresh builds remain reproducible.

# Keep the raylib build restricted to what LOCUS needs.
set(BUILD_EXAMPLES OFF CACHE BOOL "Do not build raylib examples" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "Do not build raylib games" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static raylib" FORCE)
set(JSON_BuildTests OFF CACHE BOOL "Do not build third-party JSON tests" FORCE)

# Tag 6.0: https://github.com/raysan5/raylib/tree/6.0
# Verified tag target: dbc56a87da87d973a9c5baa4e7438a9d20121d28
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG dbc56a87da87d973a9c5baa4e7438a9d20121d28
)

# Annotated tag v3.12.0 resolves to commit:
# 55f93686c01528224f448c19128836e7df245f72
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG 55f93686c01528224f448c19128836e7df245f72
)

FetchContent_MakeAvailable(raylib nlohmann_json)
# raylib 6 disables JPEG by default, while LOCUS allows JPEG song artwork.
# Set this on the decoder's actual library target, not just our game executable.
target_compile_definitions(raylib PRIVATE SUPPORT_FILEFORMAT_JPG=1)


# Native responsive menu layer. Source pins are immutable for reproducible CI.
# RmlUi handles menu/editor layout only; raylib still renders scored gameplay.
if(LOCUS_ENABLE_RMLUI)
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG 42608f77f20749dd6ddc9e0536788eaad70ea4b5
    )
    FetchContent_MakeAvailable(freetype)
    if(NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()

    set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "" FORCE)
    set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        rmlui
        GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
        GIT_TAG ba95ffe8bfb6370efb2cdcca927eaad4710c5413
    )
    FetchContent_MakeAvailable(rmlui)
endif()

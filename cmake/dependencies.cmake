# F-02: immutable, source-based dependencies for the Windows MVP.
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

# Tag 5.0: https://github.com/raysan5/raygui/tree/5.0
# raygui is an implementation-header library, without a root CMakeLists.txt.
# Verified tag target: 020a61bebcbe288b4414de3416e219ef40af847a
FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG 020a61bebcbe288b4414de3416e219ef40af847a
)

# Annotated tag v3.12.0 resolves to commit:
# 55f93686c01528224f448c19128836e7df245f72
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG 55f93686c01528224f448c19128836e7df245f72
)

FetchContent_MakeAvailable(raylib raygui nlohmann_json)

# Do not compile the RAYGUI_IMPLEMENTATION macro in a public interface target.
# Its implementation belongs to exactly one compilation unit when used by UI.
add_library(locus_raygui INTERFACE)
target_include_directories(locus_raygui SYSTEM INTERFACE
    "${raygui_SOURCE_DIR}/src"
)
target_link_libraries(locus_raygui INTERFACE raylib)

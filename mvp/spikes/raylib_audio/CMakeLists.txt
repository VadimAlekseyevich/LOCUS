cmake_minimum_required(VERSION 3.24)

# This is a disposable diagnostic probe, not the production LOCUS project.
project(locus_raylib_audio_spike LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "Skip raylib examples" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "Skip raylib games" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Prefer static raylib" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(locus_raylib_audio_spike main.cpp)
target_link_libraries(locus_raylib_audio_spike PRIVATE raylib)

if(MSVC)
    target_compile_options(locus_raylib_audio_spike PRIVATE /W4 /permissive-)
else()
    target_compile_options(locus_raylib_audio_spike PRIVATE -Wall -Wextra -Wpedantic)
endif()

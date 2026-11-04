cmake_minimum_required(VERSION 3.24)

# Production LOCUS build. The A-07 raylib probe is intentionally separate.
project(LOCUS VERSION 0.1.0 LANGUAGES CXX)

option(LOCUS_BUILD_TESTS "Enable LOCUS tests (test runner arrives in F-04)" OFF)

add_library(locus_core STATIC
    src/core/foundation.cpp
)
target_compile_features(locus_core PUBLIC cxx_std_17)
set_target_properties(locus_core PROPERTIES
    CXX_EXTENSIONS OFF
)
target_include_directories(locus_core PUBLIC
    "${PROJECT_SOURCE_DIR}/src"
)

add_executable(locus
    src/app/main.cpp
)
target_link_libraries(locus PRIVATE locus_core)
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
    # F-04 adds a test runner and CTest registrations.
endif()

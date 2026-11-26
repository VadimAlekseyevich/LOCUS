# Device-independent installed chart storage and catalog.
# Each implementation is compiled once, and shared by the application,
# headless tests and optional benchmarks. No raylib or RmlUi dependency.

add_library(locus_file_io STATIC
    src/app/local_file_io.cpp
)
target_link_libraries(locus_file_io PUBLIC locus_core)

add_library(locus_chart_storage STATIC
    src/app/chart_file.cpp
    src/app/chart_save.cpp
    src/app/song_set_storage.cpp
)
target_link_libraries(locus_chart_storage PUBLIC locus_package locus_core
    PRIVATE locus_file_io nlohmann_json::nlohmann_json)

add_library(locus_song_library STATIC
    src/app/library_chart_payload.cpp
    src/app/song_library.cpp
    src/app/song_library_scan.cpp
    src/app/song_library_metadata.cpp
    src/app/song_library_packages.cpp
    src/app/song_library_install.cpp
    src/app/song_library_trash.cpp
)
target_link_libraries(locus_song_library PUBLIC locus_chart_storage)

add_library(locus_library_selection STATIC
    src/app/library_selection.cpp
)
target_link_libraries(locus_library_selection PUBLIC locus_song_library)

foreach(module IN ITEMS locus_file_io locus_chart_storage locus_song_library locus_library_selection)
    set_target_properties(${module} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${module} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${module} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
if(WIN32)
    target_link_libraries(locus_chart_storage PRIVATE shell32)
endif()

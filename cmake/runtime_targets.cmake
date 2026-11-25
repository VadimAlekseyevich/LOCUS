# Runtime modules with explicit dependencies. Tests still inject their
# music output backend through LocalMusic; no test-only source variants.
# The pure mesh builder stays separately linkable from GPU submission.

add_library(locus_transport STATIC
    src/app/audio_device.cpp
    src/app/demo_transport.cpp
    src/app/local_music.cpp
    src/app/pitch_preserving_stream.cpp
    src/app/chart_playback.cpp
)
target_link_libraries(locus_transport PUBLIC locus_core raylib locus_soundtouch
    PRIVATE locus_file_io)

add_library(locus_game_session STATIC
    src/app/game_session.cpp
)
target_link_libraries(locus_game_session PUBLIC locus_transport)

add_library(locus_result_store STATIC
    src/app/run_results.cpp
    src/app/async_result_history.cpp
)
target_link_libraries(locus_result_store PUBLIC locus_core
    PRIVATE nlohmann_json::nlohmann_json Threads::Threads)

add_library(locus_path_mesh STATIC
    src/app/path_mesh.cpp
)
target_link_libraries(locus_path_mesh PUBLIC locus_core raylib)

add_library(locus_route_renderer STATIC
    src/app/route_renderer.cpp
)
target_link_libraries(locus_route_renderer PUBLIC locus_path_mesh)

foreach(module IN ITEMS locus_transport locus_game_session locus_result_store locus_path_mesh locus_route_renderer)
    set_target_properties(${module} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${module} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${module} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

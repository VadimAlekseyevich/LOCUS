# Validate an executable's UI resources or a portable installed package.
# LOCUS_UI_SOURCE_DIR is optional; when supplied, compare exact file lists
# and contents to catch stale edits and files deleted from the source.
if(NOT DEFINED LOCUS_UI_ASSETS_DIR OR LOCUS_UI_ASSETS_DIR STREQUAL "")
    message(FATAL_ERROR "LOCUS_UI_ASSETS_DIR must be provided")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/ui_assets_manifest.cmake")

foreach(relative IN LISTS LOCUS_REQUIRED_UI_ASSETS)
    if(NOT EXISTS "${LOCUS_UI_ASSETS_DIR}/${relative}" OR
       IS_DIRECTORY "${LOCUS_UI_ASSETS_DIR}/${relative}")
        message(FATAL_ERROR "Missing packaged LOCUS UI asset: ${relative}")
    endif()
endforeach()

if(DEFINED LOCUS_UI_SOURCE_DIR AND NOT LOCUS_UI_SOURCE_DIR STREQUAL "")
    file(GLOB_RECURSE source_files RELATIVE "${LOCUS_UI_SOURCE_DIR}"
        LIST_DIRECTORIES false "${LOCUS_UI_SOURCE_DIR}/*")
    file(GLOB_RECURSE delivered_files RELATIVE "${LOCUS_UI_ASSETS_DIR}"
        LIST_DIRECTORIES false "${LOCUS_UI_ASSETS_DIR}/*")
    list(SORT source_files)
    list(SORT delivered_files)
    if(NOT "${source_files}" STREQUAL "${delivered_files}")
        message(FATAL_ERROR
            "Packaged LOCUS UI inventory differs from assets/ui. "
            "Source: [${source_files}]; delivered: [${delivered_files}]")
    endif()
    foreach(relative IN LISTS source_files)
        file(SHA256 "${LOCUS_UI_SOURCE_DIR}/${relative}" source_hash)
        file(SHA256 "${LOCUS_UI_ASSETS_DIR}/${relative}" delivered_hash)
        if(NOT source_hash STREQUAL delivered_hash)
            message(FATAL_ERROR "Stale packaged LOCUS UI asset: ${relative}")
        endif()
    endforeach()
endif()

message(STATUS "PACKAGED_UI_ASSETS_PASS")

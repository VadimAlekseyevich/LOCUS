# Only dependencies required by the portable rhythm/chart model.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static dependencies" FORCE)
set(JSON_BuildTests OFF CACHE BOOL "Do not build third-party JSON tests" FORCE)

# Annotated tag v3.12.0 resolves to commit:
# 55f93686c01528224f448c19128836e7df245f72
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG 55f93686c01528224f448c19128836e7df245f72
)

FetchContent_MakeAvailable(nlohmann_json)

# UTF-8 NFKC case folding for language-independent library search.
# utf8proc 2.12.0 (MIT); immutable v2.12.0 release commit.
set(UTF8PROC_INSTALL OFF CACHE BOOL "Do not install utf8proc separately" FORCE)
set(UTF8PROC_ENABLE_TESTING OFF CACHE BOOL "Use LOCUS tests instead" FORCE)
FetchContent_Declare(
    utf8proc
    GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
    GIT_TAG 79cdd5ab40c79afa559f689ffc13b76812dee1ac
)
FetchContent_MakeAvailable(utf8proc)

# SHA-256 for content addressing, not authentication or password hashing.
# PicoSHA2 (MIT), pinned immutable commit; no platform crypto dependency.
FetchContent_Declare(
    picosha2
    GIT_REPOSITORY https://github.com/okdshin/PicoSHA2.git
    GIT_TAG 161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29
    SOURCE_SUBDIR locus-header-only
)
FetchContent_MakeAvailable(picosha2)

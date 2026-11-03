# Third-party dependencies. Each is taken from the system when a matching package is found
# (FIND_PACKAGE_ARGS) and fetched at the pinned tag otherwise. SYSTEM keeps their headers
# out of our warnings, and EXCLUDE_FROM_ALL keeps their extra targets out of the default build.
include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
  SYSTEM
  EXCLUDE_FROM_ALL
  FIND_PACKAGE_ARGS 5.5 NAMES raylib
)

# JSON for the content catalogs (docs/03_GEOLOGY.md, section 4.3).
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE
  SYSTEM
  EXCLUDE_FROM_ALL
  FIND_PACKAGE_ARGS 3.11 NAMES nlohmann_json
)

FetchContent_MakeAvailable(raylib nlohmann_json)

if (INFINITESIA_BUILD_TESTS)
  # doctest's own CMakeLists predates the minimum policy version of CMake 4.
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
  set(DOCTEST_WITH_TESTS OFF CACHE INTERNAL "")
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
    GIT_SHALLOW TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS 2.4 NAMES doctest
  )
  FetchContent_MakeAvailable(doctest)
endif()

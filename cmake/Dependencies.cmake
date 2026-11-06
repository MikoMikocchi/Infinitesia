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

# SDL3 is the platform layer for the OpenGL renderer that replaces raylib.
# Only the subsystems the game uses are built, to keep the dependency small.
set(SDL_SHARED        OFF CACHE BOOL "" FORCE)
set(SDL_STATIC        ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY  OFF CACHE BOOL "" FORCE)
set(SDL_TESTS         OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES      OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL       OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO         OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK      OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC        OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI        OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR        OFF CACHE BOOL "" FORCE)
set(SDL_POWER         OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG        OFF CACHE BOOL "" FORCE)
set(SDL_TRAY          OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA        OFF CACHE BOOL "" FORCE)
set(SDL_GPU           OFF CACHE BOOL "" FORCE)
set(SDL_RENDER        OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN        OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.20
  GIT_SHALLOW TRUE
  SYSTEM
  EXCLUDE_FROM_ALL
  FIND_PACKAGE_ARGS 3.2 NAMES SDL3
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

# XML for the content definitions (docs/DataDriven.md, section 4.8) and the
# loaders that read them.
set(PUGIXML_BUILD_TESTS OFF CACHE INTERNAL "")
set(PUGIXML_INSTALL OFF CACHE INTERNAL "")
FetchContent_Declare(
  pugixml
  GIT_REPOSITORY https://github.com/zeux/pugixml.git
  GIT_TAG v1.14
  GIT_SHALLOW TRUE
  SYSTEM
  EXCLUDE_FROM_ALL
  FIND_PACKAGE_ARGS NAMES pugixml
)

FetchContent_MakeAvailable(SDL3 raylib nlohmann_json pugixml)

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

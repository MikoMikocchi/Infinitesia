cmake_minimum_required(VERSION 3.21)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(Infinitesia LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

# Warnings apply to project targets only (never to fetched dependencies).
function(infinitesia_enable_warnings target)
  if (MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
endfunction()

# Planet generation has no raylib dependency, so it is built as its own
# library and shared by the game and the tests.
file(GLOB_RECURSE INFINITESIA_PLANET_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/planet/*.cpp"
)
file(GLOB_RECURSE INFINITESIA_PLANET_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/planet/*.h"
)
add_library(InfinitesiaPlanet STATIC ${INFINITESIA_PLANET_SOURCES} ${INFINITESIA_PLANET_HEADERS})
target_include_directories(InfinitesiaPlanet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
infinitesia_enable_warnings(InfinitesiaPlanet)

# The block world has no raylib dependency either (rendering lives outside it), so
# it is its own library, shared by the game and the tests.
file(GLOB_RECURSE INFINITESIA_WORLD_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/world/*.cpp"
)
file(GLOB_RECURSE INFINITESIA_WORLD_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/world/*.h"
)
add_library(InfinitesiaWorld STATIC ${INFINITESIA_WORLD_SOURCES} ${INFINITESIA_WORLD_HEADERS})
target_include_directories(InfinitesiaWorld PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(InfinitesiaWorld PUBLIC InfinitesiaPlanet)
infinitesia_enable_warnings(InfinitesiaWorld)

file(GLOB_RECURSE INFINITESIA_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
list(FILTER INFINITESIA_SOURCES EXCLUDE REGEX "/src/planet/")
list(FILTER INFINITESIA_SOURCES EXCLUDE REGEX "/src/world/")
add_executable(Infinitesia ${INFINITESIA_SOURCES})
infinitesia_enable_warnings(Infinitesia)

file(GLOB_RECURSE INFINITESIA_ASSET_FILES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/assets/*"
)
set_source_files_properties(${INFINITESIA_ASSET_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(Infinitesia PRIVATE ${INFINITESIA_ASSET_FILES})

add_custom_command(TARGET Infinitesia POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:Infinitesia>/assets"
)

target_include_directories(Infinitesia PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(Infinitesia PRIVATE raylib InfinitesiaWorld InfinitesiaPlanet)

# Treat raylib headers as system headers so their warnings do not drown ours.
get_target_property(RAYLIB_INCLUDE_DIRS raylib INTERFACE_INCLUDE_DIRECTORIES)
if (RAYLIB_INCLUDE_DIRS)
  target_include_directories(Infinitesia SYSTEM PRIVATE ${RAYLIB_INCLUDE_DIRS})
endif()

if (WIN32)
  target_link_libraries(Infinitesia PRIVATE winmm)
endif()

# Command-line tool: an isometric block diagram of the generated terrain (no window).
add_executable(TerrainSnapshot tools/TerrainSnapshot.cpp)
target_link_libraries(TerrainSnapshot PRIVATE InfinitesiaWorld InfinitesiaPlanet)
infinitesia_enable_warnings(TerrainSnapshot)

include(CTest)
if (BUILD_TESTING)
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
    GIT_SHALLOW TRUE
  )
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
  FetchContent_MakeAvailable(doctest)

  add_executable(PlanetGenerationTests tests/PlanetGenerationTests.cpp)
  target_link_libraries(PlanetGenerationTests PRIVATE InfinitesiaPlanet doctest::doctest)
  infinitesia_enable_warnings(PlanetGenerationTests)

  # Registers every TEST_CASE as its own CTest test.
  include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
  doctest_discover_tests(PlanetGenerationTests)

  add_executable(RegionTests tests/RegionTests.cpp)
  target_link_libraries(RegionTests PRIVATE InfinitesiaPlanet doctest::doctest)
  infinitesia_enable_warnings(RegionTests)
  doctest_discover_tests(RegionTests)

  add_executable(WorldTests tests/WorldTests.cpp)
  target_link_libraries(WorldTests PRIVATE InfinitesiaWorld InfinitesiaPlanet doctest::doctest)
  infinitesia_enable_warnings(WorldTests)
  doctest_discover_tests(WorldTests)
endif()

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

# JSON for the content catalogs (docs/03_GEOLOGY.md, section 4.3).
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
set(JSON_SystemInclude ON CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# Warnings apply to project targets only (never to fetched dependencies).
function(infinitesia_enable_warnings target)
  if (MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
endfunction()

# Declares one module (a layer or domain of the project) as a static library built from
# every .cpp/.h under the given src/ directories. Dependencies between modules are declared
# explicitly here; tools/check_dependencies.py enforces the same layering on the #include
# graph, so a module cannot quietly reach into one that is not below it.
#   infinitesia_add_module(<target> DIRS <src subdirs>... [PUBLIC <libs>...] [PRIVATE <libs>...])
function(infinitesia_add_module target)
  cmake_parse_arguments(MODULE "" "" "DIRS;PUBLIC;PRIVATE" ${ARGN})
  set(module_globs "")
  foreach(dir IN LISTS MODULE_DIRS)
    list(APPEND module_globs
      "${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.h")
  endforeach()
  file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS ${module_globs})
  add_library(${target} STATIC ${module_sources})
  target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
  if (MODULE_PUBLIC)
    target_link_libraries(${target} PUBLIC ${MODULE_PUBLIC})
  endif()
  if (MODULE_PRIVATE)
    target_link_libraries(${target} PRIVATE ${MODULE_PRIVATE})
  endif()
  infinitesia_enable_warnings(${target})
endfunction()

# Layers, bottom to top:
#   foundation (core, coordinates)  ids, time, load progress, coordinates; no domain knowledge
#   chemistry                       elements, compositions, reactions
#   planet                          planet generation pipeline (no raylib)
#   geology                         what the ground is made of; content from assets/geology/*.json
#   world                           the block world and its objects (no raylib)
#   (executable)                    app, input, ui, rendering, assets, commands
infinitesia_add_module(InfinitesiaFoundation DIRS core coordinates)
infinitesia_add_module(InfinitesiaChemistry DIRS chemistry PUBLIC InfinitesiaFoundation)
infinitesia_add_module(InfinitesiaPlanet DIRS planet PUBLIC InfinitesiaFoundation)
infinitesia_add_module(InfinitesiaGeology DIRS geology
  PUBLIC InfinitesiaPlanet InfinitesiaChemistry PRIVATE nlohmann_json::nlohmann_json)

# Where the source tree keeps the catalogs; tools and tests that run from a build
# directory read them from here (the game reads the copy next to its executable).
set(INFINITESIA_GEOLOGY_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/geology")

infinitesia_add_module(InfinitesiaWorld DIRS world PUBLIC InfinitesiaPlanet InfinitesiaGeology)

file(GLOB_RECURSE INFINITESIA_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
foreach(module_dir IN ITEMS core coordinates chemistry geology planet world)
  list(FILTER INFINITESIA_SOURCES EXCLUDE REGEX "/src/${module_dir}/")
endforeach()
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

target_link_libraries(Infinitesia PRIVATE raylib InfinitesiaWorld InfinitesiaGeology InfinitesiaPlanet InfinitesiaFoundation)

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
target_link_libraries(TerrainSnapshot PRIVATE InfinitesiaWorld InfinitesiaGeology InfinitesiaPlanet)
target_compile_definitions(TerrainSnapshot PRIVATE INFINITESIA_GEOLOGY_ASSET_DIR="${INFINITESIA_GEOLOGY_ASSET_DIR}")
infinitesia_enable_warnings(TerrainSnapshot)

# Command-line tool: asks the geology what lies at a point of the planet and why (no window).
add_executable(GeologyInspector tools/GeologyInspector.cpp)
target_link_libraries(GeologyInspector PRIVATE InfinitesiaGeology InfinitesiaPlanet)
target_compile_definitions(GeologyInspector PRIVATE INFINITESIA_GEOLOGY_ASSET_DIR="${INFINITESIA_GEOLOGY_ASSET_DIR}")
infinitesia_enable_warnings(GeologyInspector)

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
  target_link_libraries(WorldTests PRIVATE InfinitesiaWorld InfinitesiaGeology InfinitesiaPlanet doctest::doctest)
  target_compile_definitions(WorldTests PRIVATE INFINITESIA_GEOLOGY_ASSET_DIR="${INFINITESIA_GEOLOGY_ASSET_DIR}")
  infinitesia_enable_warnings(WorldTests)
  doctest_discover_tests(WorldTests)

  add_executable(PlanetGeologyTests tests/PlanetGeologyTests.cpp)
  target_link_libraries(PlanetGeologyTests PRIVATE InfinitesiaPlanet doctest::doctest)
  infinitesia_enable_warnings(PlanetGeologyTests)
  doctest_discover_tests(PlanetGeologyTests)

  add_executable(GeologyTests tests/GeologyTests.cpp)
  target_link_libraries(GeologyTests PRIVATE InfinitesiaGeology InfinitesiaPlanet InfinitesiaChemistry doctest::doctest)
  target_compile_definitions(GeologyTests PRIVATE INFINITESIA_GEOLOGY_ASSET_DIR="${INFINITESIA_GEOLOGY_ASSET_DIR}")
  infinitesia_enable_warnings(GeologyTests)
  doctest_discover_tests(GeologyTests)

  add_executable(ChemistryTests tests/ChemistryTests.cpp)
  target_link_libraries(ChemistryTests PRIVATE InfinitesiaChemistry doctest::doctest)
  infinitesia_enable_warnings(ChemistryTests)
  doctest_discover_tests(ChemistryTests)
endif()

# Layering check: fails when a module includes one that is not below it.
find_package(Python3 COMPONENTS Interpreter)
if (BUILD_TESTING AND Python3_FOUND)
  add_test(NAME LayeringCheck
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dependencies.py)
endif()

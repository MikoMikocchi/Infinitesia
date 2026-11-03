cmake_minimum_required(VERSION 3.26)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(Infinitesia LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(infinitesia_tests_default ON)
else()
  set(infinitesia_tests_default OFF)
endif()
option(INFINITESIA_BUILD_TESTS "Build the tests" ${infinitesia_tests_default})
option(INFINITESIA_BUILD_TOOLS "Build the command-line tools" ON)
option(INFINITESIA_WARNINGS_AS_ERRORS "Treat warnings of project targets as errors" OFF)
option(INFINITESIA_ENABLE_LTO "Link-time optimization for Release builds, when supported" ON)

# Faster rebuilds when ccache is installed.
find_program(INFINITESIA_CCACHE ccache)
if (INFINITESIA_CCACHE)
  set(CMAKE_C_COMPILER_LAUNCHER   ${INFINITESIA_CCACHE})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${INFINITESIA_CCACHE})
endif()

if (INFINITESIA_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT infinitesia_ipo_supported OUTPUT infinitesia_ipo_log LANGUAGES CXX)
  if (infinitesia_ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  endif()
endif()

if (INFINITESIA_BUILD_TESTS)
  include(CTest)
endif()

include(cmake/Dependencies.cmake)
include(cmake/Modules.cmake)

# Layers, bottom to top:
#   foundation                      ids, time, load progress, coordinates; no domain knowledge
#   domains/natural/chemistry       elements, compositions, reactions
#   domains/natural/planetary       planet generation pipeline (no raylib)
#   materials                       every material of the game (rocks, minerals, fluids, built); assets/materials/materials.json
#   domains/natural/geology         what the ground is made of: province columns of rocks; assets/geology/*.json
#   blocks                          shapes of a world cell and the dense tables over materials; assets/blocks/*.json
#   world                           the block terrain: grid, chunks, terrain sources (no raylib)
#   simulation                      system scheduler running on the fixed tick of foundation/Simulation (no raylib)
#   gameplay                        entities, items, work, commands, pathfinding (no raylib)
#   (executable)                    presentation (rendering, ui, input, assets) and app
infinitesia_add_module(InfinitesiaFoundation DIRS foundation)
infinitesia_add_module(InfinitesiaChemistry DIRS domains/natural/chemistry PUBLIC InfinitesiaFoundation)
infinitesia_add_module(InfinitesiaPlanet DIRS domains/natural/planetary PUBLIC InfinitesiaFoundation)
infinitesia_add_module(InfinitesiaMaterials DIRS materials
  PUBLIC InfinitesiaChemistry PRIVATE nlohmann_json::nlohmann_json)
infinitesia_add_module(InfinitesiaGeology DIRS domains/natural/geology
  PUBLIC InfinitesiaPlanet InfinitesiaChemistry InfinitesiaMaterials PRIVATE nlohmann_json::nlohmann_json)
infinitesia_add_module(InfinitesiaBlocks DIRS blocks
  PUBLIC InfinitesiaMaterials PRIVATE nlohmann_json::nlohmann_json)
infinitesia_add_module(InfinitesiaWorld DIRS world PUBLIC InfinitesiaPlanet InfinitesiaGeology InfinitesiaBlocks)
infinitesia_add_module(InfinitesiaSimulation DIRS simulation PUBLIC InfinitesiaFoundation)
infinitesia_add_module(InfinitesiaGameplay DIRS gameplay
  PUBLIC InfinitesiaWorld InfinitesiaSimulation PRIVATE nlohmann_json::nlohmann_json)

# The executable is presentation and app: everything under src/ that no module owns.
file(GLOB_RECURSE INFINITESIA_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/*.h"
)
get_property(INFINITESIA_MODULE_DIRS GLOBAL PROPERTY INFINITESIA_MODULE_DIRS)
foreach(module_dir IN LISTS INFINITESIA_MODULE_DIRS)
  list(FILTER INFINITESIA_SOURCES EXCLUDE REGEX "/src/${module_dir}/")
endforeach()
add_executable(Infinitesia ${INFINITESIA_SOURCES})
target_link_libraries(Infinitesia PRIVATE raylib InfinitesiaGameplay)
infinitesia_enable_warnings(Infinitesia)

if (WIN32)
  target_link_libraries(Infinitesia PRIVATE winmm)
endif()

# Assets are listed as sources for the IDE only, and copied next to the executable after
# linking (only the files that changed).
file(GLOB_RECURSE INFINITESIA_ASSET_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/*")
set_source_files_properties(${INFINITESIA_ASSET_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(Infinitesia PRIVATE ${INFINITESIA_ASSET_FILES})
add_custom_command(TARGET Infinitesia POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    "${PROJECT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:Infinitesia>/assets"
  VERBATIM
)

if (INFINITESIA_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if (INFINITESIA_BUILD_TESTS)
  add_subdirectory(tests)
endif()

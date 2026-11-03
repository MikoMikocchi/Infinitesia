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
# every .cpp/.h under the given src/ directories (nested ones such as domains/natural/geology are fine). Dependencies between modules are declared
# explicitly here; tools/check_dependencies.py enforces the same layering on the #include
# graph, so a module cannot quietly reach into one that is not below it.
#   infinitesia_add_module(<target> DIRS <src subdirs>... [PUBLIC <libs>...] [PRIVATE <libs>...])
function(infinitesia_add_module target)
  cmake_parse_arguments(MODULE "" "" "DIRS;PUBLIC;PRIVATE" ${ARGN})
  # Every module directory is left out of the executable's own sources (see below).
  set_property(GLOBAL APPEND PROPERTY INFINITESIA_MODULE_DIRS ${MODULE_DIRS})
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

# Where the source tree keeps the catalogs; tools and tests that run from a build
# directory read them from here (the game reads the copy next to its executable).
infinitesia_add_module(InfinitesiaBlocks DIRS blocks
  PUBLIC InfinitesiaMaterials PRIVATE nlohmann_json::nlohmann_json)

set(INFINITESIA_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(INFINITESIA_GAMEPLAY_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/gameplay")

infinitesia_add_module(InfinitesiaWorld DIRS world PUBLIC InfinitesiaPlanet InfinitesiaGeology InfinitesiaBlocks)
infinitesia_add_module(InfinitesiaSimulation DIRS simulation PUBLIC InfinitesiaFoundation)
infinitesia_add_module(InfinitesiaGameplay DIRS gameplay
  PUBLIC InfinitesiaWorld InfinitesiaSimulation PRIVATE nlohmann_json::nlohmann_json)

file(GLOB_RECURSE INFINITESIA_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
get_property(INFINITESIA_MODULE_DIRS GLOBAL PROPERTY INFINITESIA_MODULE_DIRS)
foreach(module_dir IN LISTS INFINITESIA_MODULE_DIRS)
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

target_link_libraries(Infinitesia PRIVATE raylib InfinitesiaGameplay InfinitesiaSimulation InfinitesiaWorld InfinitesiaBlocks InfinitesiaGeology InfinitesiaMaterials InfinitesiaPlanet InfinitesiaFoundation)

# Treat raylib headers as system headers so their warnings do not drown ours.
get_target_property(RAYLIB_INCLUDE_DIRS raylib INTERFACE_INCLUDE_DIRECTORIES)
if (RAYLIB_INCLUDE_DIRS)
  target_include_directories(Infinitesia SYSTEM PRIVATE ${RAYLIB_INCLUDE_DIRS})
endif()

if (WIN32)
  target_link_libraries(Infinitesia PRIVATE winmm)
endif()

# Command-line tool: an isometric block diagram of the generated terrain (no window).
add_executable(TerrainSnapshot
  tools/TerrainSnapshot.cpp
  tools/terrain_snapshot/BlockDiagram.cpp
  tools/terrain_snapshot/GeologicalMap.cpp
  tools/terrain_snapshot/Image.cpp
  tools/terrain_snapshot/Options.cpp
  tools/terrain_snapshot/Reports.cpp
  tools/terrain_snapshot/SnapshotWorld.cpp)
target_include_directories(TerrainSnapshot PRIVATE tools)
target_link_libraries(TerrainSnapshot PRIVATE InfinitesiaWorld InfinitesiaBlocks InfinitesiaGeology InfinitesiaMaterials InfinitesiaPlanet)
target_compile_definitions(TerrainSnapshot PRIVATE INFINITESIA_ASSET_DIR="${INFINITESIA_ASSET_DIR}")
infinitesia_enable_warnings(TerrainSnapshot)

# Command-line tool: asks the geology what lies at a point of the planet and why (no window).
add_executable(GeologyInspector tools/GeologyInspector.cpp)
target_link_libraries(GeologyInspector PRIVATE InfinitesiaGeology InfinitesiaMaterials InfinitesiaPlanet)
target_compile_definitions(GeologyInspector PRIVATE INFINITESIA_ASSET_DIR="${INFINITESIA_ASSET_DIR}")
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

  # Shared test helpers (tests/support/) are included as "support/...".
  add_library(TestSupport INTERFACE)
  target_include_directories(TestSupport INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(TestSupport INTERFACE doctest::doctest)

  add_executable(PlanetGenerationTests
    tests/support/DoctestMain.cpp
    tests/domains/natural/planetary/StarSystemTests.cpp
    tests/domains/natural/planetary/PlanetMapTests.cpp
    tests/domains/natural/planetary/PlateKinematicsTests.cpp
    tests/domains/natural/planetary/HydrologyTests.cpp
    tests/domains/natural/planetary/EdgeCaseTests.cpp)
  target_link_libraries(PlanetGenerationTests PRIVATE InfinitesiaPlanet TestSupport)
  infinitesia_enable_warnings(PlanetGenerationTests)

  # Registers every TEST_CASE as its own CTest test.
  include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
  doctest_discover_tests(PlanetGenerationTests)

  add_executable(PlanetPipelineTests tests/domains/natural/planetary/PlanetPipelineTests.cpp)
  target_link_libraries(PlanetPipelineTests PRIVATE InfinitesiaPlanet doctest::doctest)
  infinitesia_enable_warnings(PlanetPipelineTests)
  doctest_discover_tests(PlanetPipelineTests)

  add_executable(PlanetGoldenTests tests/domains/natural/planetary/PlanetGoldenTests.cpp)
  target_link_libraries(PlanetGoldenTests PRIVATE InfinitesiaPlanet doctest::doctest)
  infinitesia_enable_warnings(PlanetGoldenTests)
  doctest_discover_tests(PlanetGoldenTests)

  add_executable(RegionTests tests/domains/natural/planetary/RegionTests.cpp)
  target_link_libraries(RegionTests PRIVATE InfinitesiaPlanet doctest::doctest)
  infinitesia_enable_warnings(RegionTests)
  doctest_discover_tests(RegionTests)

  add_executable(WorldTests tests/WorldTests.cpp)
  target_link_libraries(WorldTests PRIVATE InfinitesiaGameplay InfinitesiaWorld InfinitesiaBlocks InfinitesiaGeology InfinitesiaMaterials InfinitesiaPlanet doctest::doctest)
  target_compile_definitions(WorldTests PRIVATE
    INFINITESIA_ASSET_DIR="${INFINITESIA_ASSET_DIR}"
    INFINITESIA_GAMEPLAY_ASSET_DIR="${INFINITESIA_GAMEPLAY_ASSET_DIR}")
  infinitesia_enable_warnings(WorldTests)
  doctest_discover_tests(WorldTests)

  add_executable(LocalGameTests tests/LocalGameTests.cpp)
  target_link_libraries(LocalGameTests PRIVATE InfinitesiaGameplay InfinitesiaWorld InfinitesiaBlocks InfinitesiaGeology InfinitesiaMaterials InfinitesiaPlanet doctest::doctest)
  target_compile_definitions(LocalGameTests PRIVATE INFINITESIA_GAMEPLAY_ASSET_DIR="${INFINITESIA_GAMEPLAY_ASSET_DIR}")
  infinitesia_enable_warnings(LocalGameTests)
  doctest_discover_tests(LocalGameTests)

  add_executable(PlanetGeologyTests
    tests/support/DoctestMain.cpp
    tests/domains/natural/planetary/GeologyFieldsTests.cpp
    tests/domains/natural/planetary/ProvinceTests.cpp
    tests/domains/natural/planetary/EpochTests.cpp)
  target_link_libraries(PlanetGeologyTests PRIVATE InfinitesiaPlanet TestSupport)
  infinitesia_enable_warnings(PlanetGeologyTests)
  doctest_discover_tests(PlanetGeologyTests)

  add_executable(MaterialsTests
    tests/support/DoctestMain.cpp
    tests/materials/MaterialCatalogTests.cpp)
  target_link_libraries(MaterialsTests PRIVATE InfinitesiaMaterials InfinitesiaChemistry nlohmann_json::nlohmann_json TestSupport)
  target_compile_definitions(MaterialsTests PRIVATE INFINITESIA_ASSET_DIR="${INFINITESIA_ASSET_DIR}")
  infinitesia_enable_warnings(MaterialsTests)
  doctest_discover_tests(MaterialsTests)

  add_executable(GeologyTests
    tests/support/DoctestMain.cpp
    tests/domains/natural/geology/ColumnTests.cpp
    tests/domains/natural/geology/PileTests.cpp
    tests/domains/natural/geology/FaultTests.cpp
    tests/domains/natural/geology/HistoryTests.cpp)
  target_link_libraries(GeologyTests PRIVATE InfinitesiaGeology InfinitesiaMaterials InfinitesiaPlanet InfinitesiaChemistry nlohmann_json::nlohmann_json TestSupport)
  target_compile_definitions(GeologyTests PRIVATE INFINITESIA_ASSET_DIR="${INFINITESIA_ASSET_DIR}")
  infinitesia_enable_warnings(GeologyTests)
  doctest_discover_tests(GeologyTests)

  add_executable(BlocksTests
    tests/support/DoctestMain.cpp
    tests/blocks/RegistryTests.cpp)
  target_link_libraries(BlocksTests PRIVATE InfinitesiaBlocks InfinitesiaMaterials InfinitesiaChemistry nlohmann_json::nlohmann_json TestSupport)
  target_compile_definitions(BlocksTests PRIVATE INFINITESIA_ASSET_DIR="${INFINITESIA_ASSET_DIR}")
  infinitesia_enable_warnings(BlocksTests)
  doctest_discover_tests(BlocksTests)

  add_executable(ContentTests tests/ContentTests.cpp)
  target_link_libraries(ContentTests PRIVATE InfinitesiaGameplay InfinitesiaWorld InfinitesiaBlocks InfinitesiaGeology InfinitesiaMaterials InfinitesiaPlanet doctest::doctest)
  target_compile_definitions(ContentTests PRIVATE INFINITESIA_GAMEPLAY_ASSET_DIR="${INFINITESIA_GAMEPLAY_ASSET_DIR}")
  infinitesia_enable_warnings(ContentTests)
  doctest_discover_tests(ContentTests)

  add_executable(SimulationTests
    tests/support/DoctestMain.cpp
    tests/simulation/RegistryTests.cpp
    tests/simulation/SchedulerTests.cpp
    tests/simulation/ExecutorTests.cpp
    tests/simulation/RuntimeTests.cpp)
  target_link_libraries(SimulationTests PRIVATE InfinitesiaSimulation InfinitesiaFoundation TestSupport)
  infinitesia_enable_warnings(SimulationTests)
  doctest_discover_tests(SimulationTests)

  add_executable(ChemistryTests tests/domains/natural/chemistry/ChemistryTests.cpp)
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

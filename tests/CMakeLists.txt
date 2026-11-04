include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")

# Shared test helpers (tests/support/) are included as "support/...".
add_library(TestSupport INTERFACE)
target_include_directories(TestSupport INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TestSupport INTERFACE doctest::doctest)

infinitesia_add_test(PlanetGenerationTests MAIN
  SOURCES
    domains/natural/planetary/StarSystemTests.cpp
    domains/natural/planetary/PlanetMapTests.cpp
    domains/natural/planetary/PlateKinematicsTests.cpp
    domains/natural/planetary/HydrologyTests.cpp
    domains/natural/planetary/EdgeCaseTests.cpp
  LINK InfinitesiaPlanet)

infinitesia_add_test(PlanetPipelineTests
  SOURCES domains/natural/planetary/PlanetPipelineTests.cpp
  LINK InfinitesiaPlanet)

infinitesia_add_test(PlanetGoldenTests
  SOURCES domains/natural/planetary/PlanetGoldenTests.cpp
  LINK InfinitesiaPlanet)

infinitesia_add_test(RegionTests
  SOURCES domains/natural/planetary/RegionTests.cpp
  LINK InfinitesiaPlanet)

infinitesia_add_test(PlanetGeologyTests MAIN
  SOURCES
    domains/natural/planetary/GeologyFieldsTests.cpp
    domains/natural/planetary/ProvinceTests.cpp
    domains/natural/planetary/EpochTests.cpp
  LINK InfinitesiaPlanet)

infinitesia_add_test(ChemistryTests
  SOURCES domains/natural/chemistry/ChemistryTests.cpp
  LINK InfinitesiaChemistry)

infinitesia_add_test(MaterialsTests MAIN ASSET_DIR
  SOURCES materials/MaterialCatalogTests.cpp
  LINK InfinitesiaMaterials nlohmann_json::nlohmann_json)

infinitesia_add_test(BiologyTests MAIN ASSET_DIR
  SOURCES
    domains/natural/biology/SpeciesCatalogTests.cpp
    domains/natural/biology/SuitabilityTests.cpp
    domains/natural/biology/TreeTests.cpp
    domains/natural/biology/FloraModelTests.cpp
    domains/natural/biology/VegetationTests.cpp
  LINK InfinitesiaBiology nlohmann_json::nlohmann_json)

infinitesia_add_test(GeologyTests MAIN ASSET_DIR
  SOURCES
    domains/natural/geology/ColumnTests.cpp
    domains/natural/geology/PileTests.cpp
    domains/natural/geology/FaultTests.cpp
    domains/natural/geology/HistoryTests.cpp
  LINK InfinitesiaGeology nlohmann_json::nlohmann_json)

infinitesia_add_test(BlocksTests MAIN ASSET_DIR
  SOURCES blocks/RegistryTests.cpp
  LINK InfinitesiaBlocks nlohmann_json::nlohmann_json)

infinitesia_add_test(SimulationTests MAIN
  SOURCES
    simulation/RegistryTests.cpp
    simulation/SchedulerTests.cpp
    simulation/ExecutorTests.cpp
    simulation/RuntimeTests.cpp
  LINK InfinitesiaSimulation)

infinitesia_add_test(WorldTests ASSET_DIR GAMEPLAY_ASSET_DIR
  SOURCES WorldTests.cpp
  LINK InfinitesiaGameplay)

infinitesia_add_test(LocalGameTests GAMEPLAY_ASSET_DIR
  SOURCES LocalGameTests.cpp
  LINK InfinitesiaGameplay)

infinitesia_add_test(ContentTests GAMEPLAY_ASSET_DIR
  SOURCES ContentTests.cpp
  LINK InfinitesiaGameplay)

# Layering check: fails when a module includes one that is not below it.
find_package(Python3 COMPONENTS Interpreter)
if (Python3_FOUND)
  add_test(NAME LayeringCheck
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_dependencies.py)
endif()

cmake_minimum_required(VERSION 3.10)

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

add_executable(Infinitesia
  src/main.cpp
  src/assets/TextureAssetManager.h
  src/assets/TextureAssetManager.cpp
  src/app/Application.h
  src/app/ApplicationConfig.h
  src/app/internal/AppHelpers.h
  src/app/internal/AppHelpers.cpp
  src/app/Application.cpp
  src/app/ApplicationRun.cpp
  src/app/ApplicationUpdate.cpp
  src/app/ApplicationRender.cpp
  src/core/Time.h
  src/core/Time.cpp
  src/core/Simulation.h
  src/core/Simulation.cpp
  src/coordinates/PlanetaryCoordinates.h
  src/planet/PlanetParameters.h
  src/planet/Planet.h
  src/planet/Planet.cpp
  src/planet/PlanetMap.h
  src/planet/PlanetMap.cpp
  src/world/Grid.h
  src/world/Grid.cpp
  src/world/Block.h
  src/world/Chunk.h
  src/world/GridPathfinder.h
  src/world/GridPathfinder.cpp
  src/world/Item.h
  src/world/Item.cpp
  src/world/Inventory.h
  src/world/Inventory.cpp
  src/world/World.h
  src/world/World.cpp
  src/world/WorldChunks.cpp
  src/world/internal/WorldTerrainGen.h
  src/world/internal/WorldTerrainGen.cpp
  src/world/WorldTerrain.cpp
  src/world/WorkSystem.h
  src/world/work/WorkUtils.h
  src/world/work/WorkUtils.cpp
  src/world/work/WorkPathfinding.h
  src/world/work/WorkPathfinding.cpp
  src/world/work/WorkMovement.h
  src/world/work/WorkMovement.cpp
  src/world/work/WorkOrders.cpp
  src/world/WorkSystem.cpp
  src/world/WorkExecution.cpp
  src/commands/Command.h
  src/commands/CommandRegistry.h
  src/commands/CommandRegistry.cpp
  src/commands/ChopTreeCommand.h
  src/commands/ChopTreeCommand.cpp
  src/commands/PickUpItemCommand.h
  src/commands/PickUpItemCommand.cpp
  src/input/CameraController.h
  src/input/CameraController.cpp
  src/input/GridPointer.h
  src/input/GridPointer.cpp
  src/input/WorldObjectPicker.h
  src/input/WorldObjectPicker.cpp
  src/input/WorldInteractionController.h
  src/input/WorldInteractionController.cpp
  src/ui/WorldContextMenu.h
  src/ui/WorldContextMenu.cpp
  src/ui/CharacterProfilePanel.h
  src/ui/CharacterProfilePanel.cpp
  src/ui/DebugOverlay.h
  src/ui/DebugOverlay.cpp
  src/rendering/WorldRenderer.h
  src/rendering/WorldRenderer.cpp
  src/rendering/WorldGeometryRenderer.h
  src/rendering/terrain/internal/TerrainMeshGeometry.h
  src/rendering/terrain/internal/TerrainMeshGeometry.cpp
  src/rendering/terrain/TerrainMeshBuilder.h
  src/rendering/terrain/TerrainMeshBuilder.cpp
  src/rendering/WorldTerrainRenderer.cpp
  src/rendering/WorldObjectRenderer.cpp
  src/rendering/WorldOverlayRenderer.h
  src/rendering/WorldOverlayRenderer.cpp
)

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

target_link_libraries(Infinitesia PRIVATE raylib)

if (WIN32)
  target_link_libraries(Infinitesia PRIVATE winmm)
endif()

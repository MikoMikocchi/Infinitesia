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
  src/app/Application.h
  src/app/ApplicationConfig.h
  src/app/Application.cpp
  src/core/Time.h
  src/core/Time.cpp
  src/core/Simulation.h
  src/core/Simulation.cpp
  src/world/Grid.h
  src/world/Grid.cpp
  src/world/GridPathfinder.h
  src/world/GridPathfinder.cpp
  src/world/World.h
  src/world/World.cpp
  src/world/WorkSystem.h
  src/world/WorkSystem.cpp
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
  src/ui/DebugOverlay.h
  src/ui/DebugOverlay.cpp
  src/rendering/WorldRenderer.h
  src/rendering/WorldRenderer.cpp
)

target_include_directories(Infinitesia PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(Infinitesia PRIVATE raylib)

if (WIN32)
  target_link_libraries(Infinitesia PRIVATE winmm)
endif()

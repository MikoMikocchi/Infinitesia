# Command-line tool: an isometric block diagram of the generated terrain (no window).
infinitesia_add_executable(TerrainSnapshot ASSET_DIR
  SOURCES
    TerrainSnapshot.cpp
    terrain_snapshot/BlockDiagram.cpp
    terrain_snapshot/GeologicalMap.cpp
    terrain_snapshot/Image.cpp
    terrain_snapshot/Options.cpp
    terrain_snapshot/Reports.cpp
    terrain_snapshot/SnapshotWorld.cpp
  LINK InfinitesiaWorld)
target_include_directories(TerrainSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Command-line tool: asks the geology what lies at a point of the planet and why (no window).
infinitesia_add_executable(GeologyInspector ASSET_DIR
  SOURCES GeologyInspector.cpp
  LINK InfinitesiaGeology)

# Command-line tool: a contact sheet of the generated plants, to tune assets/biology/species.json (no window).
infinitesia_add_executable(TreeSnapshot ASSET_DIR
  SOURCES
    TreeSnapshot.cpp
    terrain_snapshot/Image.cpp
  LINK InfinitesiaBiology)
target_include_directories(TreeSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

#pragma once

#include <cstddef>
#include <vector>

#include "world/Grid.h"
#include "world/VoxelWorld.h"

namespace Infinitesia {
// How far foliage reaches from the wood that holds it, in steps through cells
// of the tree (docs/07_BIOLOGY.md, section 6.2a); leaves farther from every
// wood cell fall.
inline constexpr int kLeafSupportSteps = 6;

// A tree is not bigger than this many cells of wood; a larger tangle (touching
// crowns of a thick forest) is cut off here.
inline constexpr std::size_t kMaxTreeWoodCells = 20000;

bool IsTreeWood(const VoxelWorld& terrain, GridCell cell);
bool IsTreeLeaves(const VoxelWorld& terrain, GridCell cell);

// The wood of the tree that has a cell at `start`: the cells with the tag
// tree_wood joined to it through the 26 neighbours. Foliage is not part of the
// connection, so crowns that overlap do not join two trees. Empty when `start`
// is not tree wood.
std::vector<GridCell> FindTreeWood(const VoxelWorld& terrain, GridCell start);

struct FelledTree {
  std::vector<GridCell> wood;  // the cells that were wood, now ground or air
  double woodKilograms = 0.0;  // the mass of that wood, by the density of each
                               // cell's material (1 m3 a cell)
  std::size_t leavesFallen =
      0;  // foliage of this and other trees left without wood
};

// Fells the tree at `start`: its wood becomes what the ground is there (roots
// give way to soil), and foliage out of reach of any wood falls with it (an
// oak's whole crown; the leaves of a neighbouring tree that its own wood still
// holds stay). Empty result when `start` is not tree wood.
FelledTree FellTree(VoxelWorld& terrain, GridCell start);
}  // namespace Infinitesia

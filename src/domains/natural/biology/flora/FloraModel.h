#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "domains/natural/biology/catalog/SpeciesCatalog.h"
#include "domains/natural/biology/community/VegetationSample.h"
#include "domains/natural/biology/flora/Tree.h"
#include "domains/natural/biology/niche/Suitability.h"

namespace Infinitesia {
// What the flora needs to know about one column of the ground, in the
// coordinates of the world grid.
struct GroundColumn {
  bool growable = false;  // soil that a plant can take root in, above the water
  int surfaceLevel = 0;   // level of the topmost ground block
  Conditions conditions;  // climate, soil and slope at the column
  // What the planet's vegetation layer says about the place. Without it the
  // flora works out the canopy and the kinds from `conditions` alone.
  std::optional<VegetationSample> vegetation;
};

// The ground as the flora sees it. Implemented by whoever owns the terrain; the
// flora never touches blocks, only asks about columns.
class GroundView {
 public:
  virtual ~GroundView() = default;
  virtual GroundColumn ColumnAt(int x, int z) const = 0;
};

// One cell of a plant: its level in the world and what it is.
struct FloraCell {
  int level = 0;
  MaterialId material = kUnspecifiedMaterial;
  TreePart part = TreePart::Trunk;
};

// The trees of a world (docs/07_BIOLOGY.md, section 7). Which tree stands where
// is a pure function of the seed and the ground: the world is cut into square
// "seed cells", each of which holds at most one tree at a place and of a kind
// chosen by hashes of the seed and the cell and by the ground there. A column
// is asked for the trees whose crowns can reach it, so neighbouring chunks
// agree without sharing anything. Trees are grown once and remembered.
//
// Not thread-safe: the caches fill on demand, like the columns of the terrain.
class FloraModel {
 public:
  static constexpr int kSeedCellMeters = 6;
  // The share of seed cells that hold a tree in the densest forest. Crowns are
  // wider than the cells, so even a share well below 1 closes the canopy; at 1
  // the forest floor is dark and hard to walk.
  static constexpr double kDensestStand = 0.55;

  // `ground` must outlive the model. Columns outside [0, width) x [0, depth)
  // have no flora.
  FloraModel(std::shared_ptr<const SpeciesCatalog> species, std::uint64_t seed,
             int width, int depth, const GroundView& ground);

  // The cells of the plants over the column (x, z), one per level, sorted by
  // level. Cells below the ground are roots only; whether they may replace a
  // block there is for the terrain to say.
  const std::vector<FloraCell>& Column(int x, int z) const;

  // How far from its trunk any plant of the catalog reaches, in cells.
  int ReachMeters() const noexcept { return m_reach; }

  // The tree of the seed cell (sx, sz) if it has one; its trunk base is in
  // `base`.
  struct Placement {
    const Species* species = nullptr;
    int baseX = 0;
    int baseY = 0;  // level of the first trunk cell
    int baseZ = 0;
  };
  std::optional<Placement> TreeOfSeedCell(int sx, int sz) const;

 private:
  struct ColumnCell {
    std::int16_t dy;
    TreePart part;
  };

  struct PlacedTree {
    Placement placement;
    int minX = 0;
    int minZ = 0;
    int width = 0;
    int depth = 0;
    std::vector<std::vector<ColumnCell>>
        columns;  // width * depth, row-major by z
  };

  const PlacedTree* Candidate(int sx, int sz) const;
  std::unique_ptr<PlacedTree> Grow(int sx, int sz) const;

  std::shared_ptr<const SpeciesCatalog> m_species;
  std::uint64_t m_seed;
  int m_width;
  int m_depth;
  const GroundView& m_ground;
  int m_reach = 0;
  mutable std::unordered_map<std::uint64_t, std::unique_ptr<PlacedTree>>
      m_trees;
  mutable std::vector<std::vector<FloraCell>> m_columns;
  mutable std::vector<std::uint8_t> m_known;
};
}  // namespace Infinitesia

#include "domains/natural/biology/flora/FloraModel.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "domains/natural/biology/community/Canopy.h"
#include "domains/natural/biology/flora/FloraRandom.h"

namespace Infinitesia {
namespace {
enum Stage : std::uint64_t { kPlace = 0x706c6163ull, kGrow = 0x67726f77ull };

int FloorDiv(int value, int divisor) noexcept {
  return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

std::uint64_t SeedCellKey(int sx, int sz) noexcept {
  return static_cast<std::uint64_t>(static_cast<std::uint32_t>(sx)) << 32 |
         static_cast<std::uint64_t>(static_cast<std::uint32_t>(sz));
}

const std::vector<FloraCell>& NoCells() {
  static const std::vector<FloraCell> none;
  return none;
}
}  // namespace

FloraModel::FloraModel(std::shared_ptr<const SpeciesCatalog> species,
                       std::uint64_t seed, int width, int depth,
                       const GroundView& ground)
    : m_species(std::move(species)),
      m_seed(seed),
      m_width(width),
      m_depth(depth),
      m_ground(ground) {
  if (m_species == nullptr || width <= 0 || depth <= 0)
    throw std::invalid_argument(
        "The flora needs a species catalog and a grid.");
  // A plant spreads over its crown, the foliage around it, and the lean of its
  // trunk.
  for (const Species& kind : m_species->All()) {
    const double reach = kind.crownRadiusMeters.max + 4.0 +
                         kind.heightMeters.max * 0.1 +
                         kind.trunkRadiusMeters.max;
    m_reach = std::max(m_reach, static_cast<int>(std::ceil(reach)));
  }
  const std::size_t columns =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(depth);
  m_columns.resize(columns);
  m_known.assign(columns, 0);
}

std::optional<FloraModel::Placement> FloraModel::TreeOfSeedCell(int sx,
                                                                int sz) const {
  const PlacedTree* tree = Candidate(sx, sz);
  if (tree == nullptr) return std::nullopt;
  return tree->placement;
}

const FloraModel::PlacedTree* FloraModel::Candidate(int sx, int sz) const {
  const std::uint64_t key = SeedCellKey(sx, sz);
  const auto found = m_trees.find(key);
  if (found != m_trees.end()) return found->second.get();
  return m_trees.emplace(key, Grow(sx, sz)).first->second.get();
}

std::unique_ptr<FloraModel::PlacedTree> FloraModel::Grow(int sx, int sz) const {
  const std::uint64_t cell = SeedCellKey(sx, sz);
  const auto roll = [&](std::uint64_t index) {
    return FloraRandom::Unit(FloraRandom::Stream(m_seed, kPlace, cell, index));
  };

  const int x =
      sx * kSeedCellMeters + static_cast<int>(roll(0) * kSeedCellMeters);
  const int z =
      sz * kSeedCellMeters + static_cast<int>(roll(1) * kSeedCellMeters);
  if (x < 0 || x >= m_width || z < 0 || z >= m_depth) return nullptr;
  const GroundColumn ground = m_ground.ColumnAt(x, z);
  if (!ground.growable) return nullptr;

  // The kinds that suit the place, each weighted by how well. The planet's
  // layer, when there is one, says which kinds make up the forest and how dense
  // it is; the column then adjusts it for its own elevation and slope (a steep
  // or high column fits a kind worse than the planet cell as a whole).
  const std::vector<Species>& kinds = m_species->All();
  std::vector<double> weight(kinds.size(), 0.0);
  double total = 0.0;
  double canopy = 0.0;
  if (ground.vegetation) {
    const VegetationSample& vegetation = *ground.vegetation;
    for (int i = 0; i < vegetation.count; ++i) {
      const VegetationSample::Entry& entry =
          vegetation.entries[static_cast<std::size_t>(i)];
      if (entry.species >= kinds.size()) continue;
      weight[entry.species] =
          entry.weight *
          Suitability(kinds[entry.species].niche, ground.conditions);
      total += weight[entry.species];
    }
    // The weights of the layer add up to 1, so `total` is the mean fitness of
    // the forest here.
    canopy = vegetation.canopyCover * total * kDensestStand;
  } else {
    double best = 0.0;
    for (std::size_t i = 0; i < kinds.size(); ++i) {
      weight[i] = Suitability(kinds[i].niche, ground.conditions);
      total += weight[i];
      best = std::max(best, weight[i]);
    }
    canopy = CanopyCover(ground.conditions) * best * kDensestStand;
  }
  if (total <= 0.0 || roll(2) >= canopy) return nullptr;

  double pick = roll(3) * total;
  std::size_t chosen = 0;
  for (std::size_t i = 0; i < kinds.size(); ++i) {
    if (weight[i] <= 0.0) continue;
    chosen = i;
    pick -= weight[i];
    if (pick < 0.0) break;
  }

  const Species& kind = kinds[chosen];
  const TreeVoxels voxels =
      GrowTree(kind, FloraRandom::Stream(m_seed, kGrow, cell, 0));
  auto tree = std::make_unique<PlacedTree>();
  tree->placement = {&kind, x, ground.surfaceLevel + 1, z};
  tree->minX = voxels.minX;
  tree->minZ = voxels.minZ;
  tree->width = voxels.maxX - voxels.minX + 1;
  tree->depth = voxels.maxZ - voxels.minZ + 1;
  tree->columns.resize(static_cast<std::size_t>(tree->width) *
                       static_cast<std::size_t>(tree->depth));
  for (const TreeVoxel& voxel : voxels.cells) {
    const std::size_t index =
        static_cast<std::size_t>(voxel.z - tree->minZ) * tree->width +
        static_cast<std::size_t>(voxel.x - tree->minX);
    tree->columns[index].push_back({voxel.y, voxel.part});
  }
  return tree;
}

const std::vector<FloraCell>& FloraModel::Column(int x, int z) const {
  if (x < 0 || x >= m_width || z < 0 || z >= m_depth) return NoCells();
  const std::size_t index =
      static_cast<std::size_t>(z) * m_width + static_cast<std::size_t>(x);
  if (m_known[index] != 0) return m_columns[index];
  m_known[index] = 1;

  std::vector<FloraCell>& cells = m_columns[index];
  const int firstX = FloorDiv(x - m_reach, kSeedCellMeters);
  const int lastX = FloorDiv(x + m_reach, kSeedCellMeters);
  const int firstZ = FloorDiv(z - m_reach, kSeedCellMeters);
  const int lastZ = FloorDiv(z + m_reach, kSeedCellMeters);
  for (int sz = firstZ; sz <= lastZ; ++sz) {
    for (int sx = firstX; sx <= lastX; ++sx) {
      const PlacedTree* tree = Candidate(sx, sz);
      if (tree == nullptr) continue;
      const int dx = x - tree->placement.baseX - tree->minX;
      const int dz = z - tree->placement.baseZ - tree->minZ;
      if (dx < 0 || dx >= tree->width || dz < 0 || dz >= tree->depth) continue;
      for (const ColumnCell& cell :
           tree->columns[static_cast<std::size_t>(dz) * tree->width + dx]) {
        cells.push_back(
            {tree->placement.baseY + cell.dy,
             PartMaterial(tree->placement.species->materials, cell.part),
             cell.part});
      }
    }
  }

  // One cell per level: the strongest part keeps it, trees met first win ties.
  std::stable_sort(cells.begin(), cells.end(),
                   [](const FloraCell& a, const FloraCell& b) {
                     if (a.level != b.level) return a.level < b.level;
                     return PartStrength(a.part) > PartStrength(b.part);
                   });
  cells.erase(std::unique(cells.begin(), cells.end(),
                          [](const FloraCell& a, const FloraCell& b) {
                            return a.level == b.level;
                          }),
              cells.end());
  cells.shrink_to_fit();
  return cells;
}
}  // namespace Infinitesia

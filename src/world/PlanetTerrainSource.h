#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "domains/natural/biology/community/VegetationLayer.h"
#include "domains/natural/biology/flora/FloraModel.h"
#include "domains/natural/geology/model/GeologyModel.h"
#include "domains/natural/planetary/region/SettlementTerrain.h"
#include "world/TerrainSource.h"

namespace Infinitesia {
// Fills the world grid from the planet. Cell (x, z) is the 1 m column that lies
// x - width / 2 metres east and depth / 2 - z metres north of the settlement
// origin (z grows southward, so looking north the map reads the right way
// round); the origin ground sits at `anchorLevel`, and one level is one metre
// of height.
//
// Each column is a surface block (turf, subsoil, sand or rock by biome), a few
// blocks of matching topsoil, and rock below. Ground below the sea or a lake is
// covered by the fluid up to its surface; FluidAt gives that layer in metres
// without looking at the blocks.
//
// With a geology model the rock is real: StateAt gives the rock of every rock
// block from the layers of the column's province. Without one every rock block
// is bedrock.
//
// With a species catalog the ground grows trees (docs/07_BIOLOGY.md): they
// stand in the air above the surface, their roots replace soil (never rock),
// and a clearing around the middle of the grid, where the settlers arrive,
// stays free of trunks.
class PlanetTerrainSource final : public TerrainSource {
 public:
  // The registry must know every rock the geology can hand out (checked here).
  PlanetTerrainSource(std::shared_ptr<const SettlementTerrain> terrain,
                      int gridWidth, int gridHeight, int gridDepth,
                      int anchorLevel,
                      std::shared_ptr<const BlockRegistry> registry,
                      std::shared_ptr<const GeologyModel> geology = nullptr,
                      std::shared_ptr<const SpeciesCatalog> flora = nullptr,
                      std::uint64_t floraSeed = 0);

  BlockState StateAt(GridCell cell) const override;
  BlockState GroundStateAt(GridCell cell) const override;
  FluidColumn FluidAt(int x, int z) const override;
  const BlockRegistry& Registry() const override { return *m_registry; }

  // What a layer of ground is, before a material is chosen for it.
  enum class Layer { Turf, Subsoil, Sand, Rock };

  // What lies `depthBelowSurface` blocks under the surface block of a column of
  // the given kind (0 is the surface block itself): topsoil, then rock.
  static Layer LayerAt(SurfaceKind kind, int depthBelowSurface);

  // Level of the topmost solid block of a column (clamped inside the grid).
  int SurfaceLevelAt(int x, int z) const;

  // Level of the top block of the sea, or -1 when the sea is not in the grid.
  int WaterTopLevel() const;

 private:
  struct Column {
    int surface = 0;
    SurfaceKind kind = SurfaceKind::Grass;
    ProvinceId province = kNoProvince;
    int pileTopMeters = 0;
    Conditions climate;  // slope is filled in when the flora asks
    // The fluid body over the column, if any, and the level of its top block
    // (-1 when the column is dry).
    FluidOrigin fluid = FluidOrigin::None;
    double fluidLevelMeters = 0.0;
    int fluidTopBlocks = -1;
    CartesianPoint3D direction{};
  };

  // The ground of a column as the flora sees it.
  class Ground final : public GroundView {
   public:
    explicit Ground(const PlanetTerrainSource& owner) : m_owner(owner) {}
    GroundColumn ColumnAt(int x, int z) const override;

   private:
    const PlanetTerrainSource& m_owner;
  };

  // Trunks start no nearer the middle of the grid than this many metres (a
  // trunk is up to 2 m thick, so the middle 5 m around it are free of trunks).
  static constexpr int kStartClearingMeters = 7;

  const Column& ColumnAt(int x, int z) const;
  BlockState StateOf(GridCell cell, bool withPlants) const;
  // The plant occupying the cell, if any.
  const FloraCell* FloraAt(GridCell cell) const;
  const GeologyModel::ColumnProfile& ProfileAt(int x, int z) const;
  // Top block level for a fluid surface this many metres above sea level. Only
  // call it for a column whose fluid is not None.
  int FluidTopBlocksFor(double fluidLevelMeters) const;

  std::shared_ptr<const SettlementTerrain> m_terrain;
  std::shared_ptr<const BlockRegistry> m_registry;
  SurfaceMatter m_matter;
  std::shared_ptr<const GeologyModel> m_geology;
  int m_width;
  int m_height;
  int m_depth;
  int m_anchorLevel;
  // Columns and rock profiles are worked out the first time a chunk touches
  // them, so they are stored sparsely: the world is far larger than what is
  // ever loaded at once.
  mutable std::unordered_map<std::size_t, Column> m_columns;
  mutable std::unordered_map<std::size_t, GeologyModel::ColumnProfile>
      m_profiles;
  std::unique_ptr<Ground> m_ground;
  std::unique_ptr<FloraModel> m_flora;
};
}  // namespace Infinitesia

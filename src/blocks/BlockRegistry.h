#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "blocks/BlockState.h"
#include "blocks/ShapeDef.h"
#include "blocks/Tag.h"
#include "materials/MaterialCatalog.h"

namespace Infinitesia {
// What a world cell can hold (docs/06_BLOCKS.md, section 3): the materials of
// the game (the MaterialCatalog) and the shapes (assets/blocks/shapes.json).
// The registry adds to the catalog the dense tables the hot paths read.
// Immutable after loading, so it can be shared freely between threads.
//
// The per-cell questions (Occupied, Opacity, ...) read dense tables and cost
// one array lookup; an id the registry does not know behaves like air, a shape
// it does not know like a full cube.
class BlockRegistry {
 public:
  // Reads the shapes from `shapesFile`. Throws CatalogError.
  static BlockRegistry Build(std::shared_ptr<const MaterialCatalog> materials,
                             const std::filesystem::path& shapesFile);

  // A minimum without files, for terrains and tests that have no catalog: air,
  // water, seawater, turf, subsoil, sand and bedrock, and the full cube.
  static BlockRegistry BuiltIn();

  const MaterialCatalog& Catalog() const noexcept { return *m_catalog; }
  std::shared_ptr<const MaterialCatalog> CatalogPointer() const noexcept {
    return m_catalog;
  }

  // Every material, sorted by id.
  std::span<const MaterialDef> Materials() const noexcept {
    return m_catalog->Materials();
  }
  std::span<const ShapeDef> Shapes() const noexcept { return m_shapes; }

  const MaterialDef* FindMaterial(std::string_view key) const noexcept {
    return m_catalog->Find(key);
  }
  // nullptr for an id that is not registered.
  const MaterialDef* GetMaterial(MaterialId id) const noexcept {
    return m_catalog->Get(id);
  }
  RgbColor ColorOf(MaterialId id) const noexcept {
    return m_catalog->ColorOf(id);
  }
  const ShapeDef* FindShape(std::string_view key) const noexcept;
  const ShapeDef* GetShape(ShapeId id) const noexcept;

  // The cell holds something (air is empty whatever its shape).
  bool Occupied(BlockState state) const noexcept {
    return Props(state.material).flags & kOccupied;
  }
  // A worker can stand on it: the material and the shape both allow it.
  bool Walkable(BlockState state) const noexcept {
    return (Props(state.material).flags & kWalkable) != 0 &&
           ShapeProps(state.shape).walkable;
  }
  bool Diggable(BlockState state) const noexcept {
    return Props(state.material).flags & kDiggable;
  }
  int DigClass(MaterialId material) const noexcept {
    return Props(material).digClass;
  }
  // How much the cell dims each channel of the light that passes through it.
  LightRgb Opacity(BlockState state) const noexcept {
    return ShapeProps(state.shape).passesLight ? LightRgb{}
                                               : Props(state.material).opacity;
  }
  // Some light gets through the cell: it stops a channel only when it dims it
  // fully.
  bool LetsLightThrough(BlockState state) const noexcept {
    const LightRgb dimming = Opacity(state);
    return dimming.r < kMaxLightLevel || dimming.g < kMaxLightLevel ||
           dimming.b < kMaxLightLevel;
  }
  LightRgb Emission(MaterialId material) const noexcept {
    return Props(material).emission;
  }
  // Sides of the cell the shape covers completely.
  std::uint8_t FaceMask(ShapeId shape) const noexcept {
    return ShapeProps(shape).faceMask;
  }

  bool Has(MaterialId material, Tag tag) const noexcept {
    return (Props(material).tags >> static_cast<unsigned>(tag)) & 1u;
  }
  // Any tag, including those only the data knows; slower than Has.
  bool HasTag(MaterialId material, std::string_view tag) const noexcept;

 private:
  static constexpr std::uint8_t kOccupied = 1 << 0;
  static constexpr std::uint8_t kWalkable = 1 << 1;
  static constexpr std::uint8_t kDiggable = 1 << 2;

  struct MaterialProps {
    std::uint8_t flags = 0;
    std::uint8_t digClass = 0;
    LightRgb opacity;
    LightRgb emission;
    std::uint32_t tags = 0;
  };

  struct ShapeProperties {
    std::uint8_t faceMask = kFaceAll;
    bool walkable = true;
    bool passesLight = false;
  };

  // Builds the dense tables and checks and indexes the shapes handed in.
  static BlockRegistry Assemble(
      std::shared_ptr<const MaterialCatalog> materials,
      std::vector<ShapeDef> shapes);

  const MaterialProps& Props(MaterialId material) const noexcept {
    return material < m_props.size() ? m_props[material]
                                     : m_props[kAirMaterial];
  }
  const ShapeProperties& ShapeProps(ShapeId shape) const noexcept {
    return m_shapeProps[m_shapeKnown[shape] ? shape : kShapeFull];
  }

  std::shared_ptr<const MaterialCatalog> m_catalog;
  std::vector<ShapeDef> m_shapes;
  std::vector<MaterialProps> m_props;  // by MaterialId
  std::array<ShapeProperties, kShapeCapacity> m_shapeProps{};
  std::array<bool, kShapeCapacity> m_shapeKnown{};
  std::unordered_map<std::string, int> m_shapeByKey;
};
}  // namespace Infinitesia

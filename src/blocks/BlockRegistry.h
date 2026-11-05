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
#include "matter/MatterCatalog.h"

namespace Infinitesia {
// What a world cell can hold (docs/06_BLOCKS.md, section 3): the materials of
// the game (the MatterCatalog) and the shapes (assets/blocks/shapes.json).
// The registry adds to the catalog the dense tables the hot paths read.
// Immutable after loading, so it can be shared freely between threads.
//
// The per-cell questions (Occupied, Opacity, ...) read dense tables and cost
// one array lookup; an id the registry does not know behaves like air, a shape
// it does not know like a full cube.
class BlockRegistry {
 public:
  // Reads the shapes from `shapesFile`. Throws CatalogError.
  static BlockRegistry Build(std::shared_ptr<const MatterCatalog> materials,
                             const std::filesystem::path& shapesFile);

  // A minimum without files, for terrains and tests that have no catalog: air,
  // water, turf, subsoil, sand and bedrock, and the full cube.
  static BlockRegistry BuiltIn();

  const MatterCatalog& Catalog() const noexcept { return *m_catalog; }
  std::shared_ptr<const MatterCatalog> CatalogPointer() const noexcept {
    return m_catalog;
  }

  // Every material, sorted by id.
  std::span<const MatterDef> Matter() const noexcept {
    return m_catalog->Matter();
  }
  std::span<const ShapeDef> Shapes() const noexcept { return m_shapes; }

  const MatterDef* FindMatter(std::string_view key) const noexcept {
    return m_catalog->Find(key);
  }
  // nullptr for an id that is not registered.
  const MatterDef* GetMatter(MatterId id) const noexcept {
    return m_catalog->Get(id);
  }
  RgbColor ColorOf(MatterId id) const noexcept {
    return m_catalog->ColorOf(id);
  }
  const ShapeDef* FindShape(std::string_view key) const noexcept;
  const ShapeDef* GetShape(ShapeId id) const noexcept;

  // The cell holds something (air is empty whatever its shape).
  bool Occupied(BlockState state) const noexcept {
    return Props(state.matter).flags & kOccupied;
  }
  // A worker can stand on it: the material and the shape both allow it.
  bool Walkable(BlockState state) const noexcept {
    return (Props(state.matter).flags & kWalkable) != 0 &&
           ShapeProps(state.shape).walkable;
  }
  bool Diggable(BlockState state) const noexcept {
    return Props(state.matter).flags & kDiggable;
  }
  int DigClass(MatterId material) const noexcept {
    return Props(material).digClass;
  }
  // How much the cell dims each channel of the light that passes through it.
  LightRgb Opacity(BlockState state) const noexcept {
    return ShapeProps(state.shape).passesLight ? LightRgb{}
                                               : Props(state.matter).opacity;
  }
  // Some light gets through the cell: it stops a channel only when it dims it
  // fully.
  bool LetsLightThrough(BlockState state) const noexcept {
    const LightRgb dimming = Opacity(state);
    return dimming.r < kMaxLightLevel || dimming.g < kMaxLightLevel ||
           dimming.b < kMaxLightLevel;
  }
  LightRgb Emission(MatterId material) const noexcept {
    return Props(material).emission;
  }
  // Sides of the cell the shape covers completely.
  std::uint8_t FaceMask(ShapeId shape) const noexcept {
    return ShapeProps(shape).faceMask;
  }

  bool Has(MatterId material, Tag tag) const noexcept {
    return (Props(material).tags >> static_cast<unsigned>(tag)) & 1u;
  }
  // Any tag, including those only the data knows; slower than Has.
  bool HasTag(MatterId material, std::string_view tag) const noexcept;

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
      std::shared_ptr<const MatterCatalog> materials,
      std::vector<ShapeDef> shapes);

  const MaterialProps& Props(MatterId material) const noexcept {
    return material < m_props.size() ? m_props[material]
                                     : m_props[kAirMatter];
  }
  const ShapeProperties& ShapeProps(ShapeId shape) const noexcept {
    return m_shapeProps[m_shapeKnown[shape] ? shape : kShapeFull];
  }

  std::shared_ptr<const MatterCatalog> m_catalog;
  std::vector<ShapeDef> m_shapes;
  std::vector<MaterialProps> m_props;  // by MatterId
  std::array<ShapeProperties, kShapeCapacity> m_shapeProps{};
  std::array<bool, kShapeCapacity> m_shapeKnown{};
  std::unordered_map<std::string, int> m_shapeByKey;
};
}  // namespace Infinitesia

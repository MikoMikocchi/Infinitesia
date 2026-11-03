#pragma once

#include "blocks/BlockState.h"
#include "blocks/ShapeDef.h"
#include "blocks/Tag.h"
#include "geology/catalog/GeologyCatalog.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Infinitesia
{
    // The one list of everything a world cell can hold (docs/06_BLOCKS.md, section 3): the
    // materials of the geology catalog (air, fluids, rocks, soils, ice) and the other materials of
    // the game (assets/blocks/built_materials.json), and the shapes (assets/blocks/shapes.json).
    // Immutable after loading, so it can be shared freely between threads.
    //
    // The per-cell questions (Occupied, Opacity, ...) read dense tables and cost one array lookup;
    // an id the registry does not know behaves like air, a shape it does not know like a full cube.
    class BlockRegistry
    {
    public:
        // Reads built_materials.json and shapes.json from `directory`. Throws CatalogError.
        static BlockRegistry Build(const GeologyCatalog& geology, const std::filesystem::path& directory);

        // A minimum without files, for terrains and tests that have no geology: air, water,
        // seawater, turf, subsoil, sand and bedrock, and the full cube.
        static BlockRegistry BuiltIn();

        // Every material, sorted by id.
        std::span<const MaterialDef> Materials() const noexcept { return m_materials; }
        std::span<const ShapeDef> Shapes() const noexcept { return m_shapes; }

        const MaterialDef* FindMaterial(std::string_view key) const noexcept;
        // nullptr for an id that is not registered.
        const MaterialDef* GetMaterial(MaterialId id) const noexcept;
        const ShapeDef* FindShape(std::string_view key) const noexcept;
        const ShapeDef* GetShape(ShapeId id) const noexcept;

        // The cell holds something (air is empty whatever its shape).
        bool Occupied(BlockState state) const noexcept { return Props(state.material).flags & kOccupied; }
        // A worker can stand on it: the material and the shape both allow it.
        bool Walkable(BlockState state) const noexcept
        {
            return (Props(state.material).flags & kWalkable) != 0 && ShapeProps(state.shape).walkable;
        }
        bool Diggable(BlockState state) const noexcept { return Props(state.material).flags & kDiggable; }
        int DigClass(MaterialId material) const noexcept { return Props(material).digClass; }
        // How much the cell dims each channel of the light that passes through it.
        LightRgb Opacity(BlockState state) const noexcept
        {
            return ShapeProps(state.shape).passesLight ? LightRgb{} : Props(state.material).opacity;
        }
        LightRgb Emission(MaterialId material) const noexcept { return Props(material).emission; }
        // Sides of the cell the shape covers completely.
        std::uint8_t FaceMask(ShapeId shape) const noexcept { return ShapeProps(shape).faceMask; }

        bool Has(MaterialId material, Tag tag) const noexcept
        {
            return (Props(material).tags >> static_cast<unsigned>(tag)) & 1u;
        }
        // Any tag, including those only the data knows; slower than Has.
        bool HasTag(MaterialId material, std::string_view tag) const noexcept;

    private:
        static constexpr std::uint8_t kOccupied = 1 << 0;
        static constexpr std::uint8_t kWalkable = 1 << 1;
        static constexpr std::uint8_t kDiggable = 1 << 2;

        struct MaterialProps
        {
            std::uint8_t flags = 0;
            std::uint8_t digClass = 0;
            LightRgb opacity;
            LightRgb emission;
            std::uint32_t tags = 0;
        };

        struct ShapeProperties
        {
            std::uint8_t faceMask = kFaceAll;
            bool walkable = true;
            bool passesLight = false;
        };

        // Sorts, checks and indexes the materials and shapes handed in.
        static BlockRegistry Assemble(std::vector<MaterialDef> materials, std::vector<ShapeDef> shapes);

        const MaterialProps& Props(MaterialId material) const noexcept
        {
            return material < m_props.size() ? m_props[material] : m_props[kAirMaterial];
        }
        const ShapeProperties& ShapeProps(ShapeId shape) const noexcept
        {
            return m_shapeProps[m_shapeKnown[shape] ? shape : kShapeFull];
        }

        std::vector<MaterialDef> m_materials;
        std::vector<ShapeDef> m_shapes;
        std::vector<MaterialProps> m_props;                     // by MaterialId
        std::array<ShapeProperties, kShapeCapacity> m_shapeProps{};
        std::array<bool, kShapeCapacity> m_shapeKnown{};
        std::vector<int> m_materialIndex;                       // by MaterialId, -1 when absent
        std::unordered_map<std::string, int> m_materialByKey;
        std::unordered_map<std::string, int> m_shapeByKey;
    };
}

#pragma once

#include "chemistry/Composition.h"
#include "materials/CatalogError.h"
#include "materials/MaterialDef.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Infinitesia
{
    // The one list of every material of the game, loaded from materials.json and validated as a
    // whole (docs/06_BLOCKS.md, section 3). Each material is one `MaterialDef`; the parts only some
    // have are side tables found by the same id: chemistry (minerals) and modal composition (rocks).
    // Immutable after loading, so it can be shared freely between threads.
    class MaterialCatalog
    {
    public:
        using ChemistryTable = std::unordered_map<MaterialId, ChemistryInfo>;
        using ModalTable = std::unordered_map<MaterialId, ModalComposition>;

        // Reads materials.json. Throws CatalogError.
        static MaterialCatalog LoadFromFile(const std::filesystem::path& path);

        // Checks and indexes materials already in memory: unique ids and keys, air at 0, a fluid at 1,
        // side tables that belong to listed materials, modal parts that name minerals.
        static MaterialCatalog Assemble(std::vector<MaterialDef> materials, ChemistryTable chemistry, ModalTable modal);

        // Every material, sorted by id.
        std::span<const MaterialDef> Materials() const noexcept { return m_materials; }

        const MaterialDef* Find(std::string_view key) const noexcept;
        // nullptr for an id that is not in the catalog (including kUnspecifiedMaterial).
        const MaterialDef* Get(MaterialId id) const noexcept;

        // nullptr when the material has no such part.
        const ChemistryInfo* ChemistryOf(MaterialId id) const noexcept;
        const ModalComposition* ModalOf(MaterialId id) const noexcept;

        // Colour of a material; magenta for an unknown id so mistakes are visible.
        RgbColor ColorOf(MaterialId id) const noexcept;

        // Element mass fractions of a mineral, or of a rock from its minerals (the modal composition
        // is by volume, so each mineral is weighted by its density). Empty for anything else, and for
        // a rock without a modal composition.
        Chemistry::ElementMassFractions ElementMassFractionsOf(MaterialId id) const;

    private:
        std::vector<MaterialDef> m_materials;
        ChemistryTable m_chemistry;
        ModalTable m_modal;
        std::unordered_map<std::string, int> m_byKey;
        std::vector<int> m_index; // MaterialId -> index in m_materials, -1 when absent
    };
}

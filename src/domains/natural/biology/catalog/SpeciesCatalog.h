#pragma once

#include "domains/natural/biology/catalog/Species.h"
#include "materials/MaterialCatalog.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace Infinitesia
{
    // Every plant species of the game, read from assets/biology/species.json. A species is found by its
    // stable key; the order of `Species()` is the order of the file, and saves store keys, never positions.
    class SpeciesCatalog
    {
    public:
        // The materials of a species must be plant materials of `materials`. Throws CatalogError,
        // naming the file and the record.
        static SpeciesCatalog Load(const std::filesystem::path& path, const MaterialCatalog& materials);

        const std::vector<Species>& All() const noexcept { return m_species; }
        const Species* Find(std::string_view key) const noexcept;

    private:
        std::vector<Species> m_species;
    };
}

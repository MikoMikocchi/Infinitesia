#pragma once

#include "domains/natural/planetary/geology/PlanetEpochs.h"
#include "domains/natural/planetary/geology/Province.h"

#include <concepts>
#include <cstdint>
#include <vector>

namespace Infinitesia
{
    // What the geology model needs to know about a planet: the seed, the provinces
    // (indexed by ProvinceId - 1) and the epoch table. Nothing else of the map.
    //
    // It only refers to the data, which must outlive the view. Anything that has
    // GetPlanet().GetSeed(), GetProvinces() and GetEpochs() converts to it implicitly
    // (PlanetMap does), so `GeologyModel(catalog, map)` keeps working without geology
    // including the map.
    class GeologyPlanetView
    {
    public:
        GeologyPlanetView(std::uint64_t seed, const std::vector<Province>& provinces,
                          const PlanetEpochTable& epochs) noexcept
            : m_seed(seed), m_provinces(&provinces), m_epochs(&epochs)
        {
        }

        template <typename Map>
            requires requires(const Map& map)
            {
                { map.GetPlanet().GetSeed() } -> std::convertible_to<std::uint64_t>;
                { map.GetProvinces() } -> std::convertible_to<const std::vector<Province>&>;
                { map.GetEpochs() } -> std::convertible_to<const PlanetEpochTable&>;
            }
        GeologyPlanetView(const Map& map) noexcept // NOLINT(google-explicit-constructor): the adapter
            : GeologyPlanetView(map.GetPlanet().GetSeed(), map.GetProvinces(), map.GetEpochs())
        {
        }

        std::uint64_t GetSeed() const noexcept { return m_seed; }
        const std::vector<Province>& GetProvinces() const noexcept { return *m_provinces; }
        const PlanetEpochTable& GetEpochs() const noexcept { return *m_epochs; }

    private:
        std::uint64_t m_seed;
        const std::vector<Province>* m_provinces;
        const PlanetEpochTable* m_epochs;
    };
}

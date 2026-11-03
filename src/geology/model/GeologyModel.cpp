#include "geology/model/GeologyModel.h"

#include "geology/util/StableRandom.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        constexpr std::uint64_t kPileStage = 0x7069'6c65'0001ull; // "pile"

        bool Satisfied(BiosphereRequirement requirement, const BiosphereState& state)
        {
            switch (requirement)
            {
            case BiosphereRequirement::None: return true;
            case BiosphereRequirement::FreeOxygen: return state.freeOxygen;
            case BiosphereRequirement::BandedIronFormations: return state.bandedIronFormations;
            case BiosphereRequirement::LandPlants: return state.landPlants;
            case BiosphereRequirement::OrganicRichMarineSediments: return state.organicRichMarineSediments;
            case BiosphereRequirement::CalcareousPlankton: return state.calcareousPlankton;
            }
            return true;
        }

        // How many formations the erosion of the province has stripped off the top.
        std::size_t StrippedLayers(ErosionLevel level, std::size_t layerCount)
        {
            switch (level)
            {
            case ErosionLevel::Surface: return 0;
            case ErosionLevel::Shallow: return std::min<std::size_t>(1, layerCount - 1);
            case ErosionLevel::Intermediate: return std::min<std::size_t>(2, layerCount - 1);
            case ErosionLevel::Deep: return layerCount - 1;
            }
            return 0;
        }
    }

    GeologyModel::GeologyModel(std::shared_ptr<const GeologyCatalog> catalog, const PlanetMap& map, GeologyConfig config)
        : m_catalog(std::move(catalog))
        , m_config(config)
    {
        if (m_catalog == nullptr || !(config.strataThicknessScale > 0.0))
        {
            throw std::invalid_argument("The geology model needs a catalog and a positive strata scale.");
        }
        const Rock* granite = m_catalog->FindRock("rock.granite");
        m_fallback = { { granite != nullptr ? granite->material : kFirstRockMaterial, 0 } };

        const std::uint64_t seed = map.GetPlanet().GetSeed();
        m_piles.reserve(map.GetProvinces().size());
        for (const Province& province : map.GetProvinces())
        {
            const ProvinceColumn& column = m_catalog->ColumnFor(province.type);
            const BiosphereState biosphere = map.GetEpochs().BiosphereAt(province.eventAgeMyr);

            // Top-down list of the layers the province can have at all.
            std::vector<ColumnLayer> layers;
            for (auto it = column.bottomUp.rbegin(); it != column.bottomUp.rend(); ++it)
            {
                if (Satisfied(it->needs, biosphere))
                {
                    layers.push_back(*it);
                }
            }
            if (layers.empty())
            {
                layers.push_back(column.bottomUp.front());
            }

            std::vector<Layer> pile;
            const std::size_t stripped = StrippedLayers(province.erosionLevel, layers.size());
            for (std::size_t i = stripped; i < layers.size(); ++i)
            {
                Layer layer;
                layer.rock = layers[i].rock;
                if (i + 1 < layers.size())
                {
                    // Formations vary in thickness from place to place; each province gets its own.
                    const double jitter = 0.8 + 0.45 * GeologyRandom::Unit(
                        GeologyRandom::Stream(seed, kPileStage, province.id, i));
                    layer.thicknessMeters = std::max(
                        1, static_cast<int>(std::lround(layers[i].thicknessMeters * m_config.strataThicknessScale * jitter)));
                }
                pile.push_back(layer);
            }
            m_piles.push_back(std::move(pile));
        }
    }

    const std::vector<GeologyModel::Layer>& GeologyModel::PileOf(ProvinceId province) const noexcept
    {
        if (province == kNoProvince || province > m_piles.size())
        {
            return m_fallback;
        }
        return m_piles[province - 1];
    }

    MaterialId GeologyModel::ExposedRock(ProvinceId province) const noexcept
    {
        return PileOf(province).front().rock;
    }

    MaterialId GeologyModel::BedrockAt(ProvinceId province, int elevationMeters, int pileTopMeters) const noexcept
    {
        const std::vector<Layer>& pile = PileOf(province);
        const int depth = pileTopMeters - elevationMeters;
        if (depth < 0)
        {
            return pile.front().rock;
        }
        int bottom = 0;
        for (std::size_t i = 0; i + 1 < pile.size(); ++i)
        {
            bottom += pile[i].thicknessMeters;
            if (depth < bottom)
            {
                return pile[i].rock;
            }
        }
        return pile.back().rock;
    }
}

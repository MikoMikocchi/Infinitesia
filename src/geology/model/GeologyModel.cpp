#include "geology/model/GeologyModel.h"

#include "geology/util/StableRandom.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        constexpr std::uint64_t kPileStage = 0x7069'6c65'0001ull;      // "pile"
        constexpr std::uint64_t kStructureStage = 0x7374'7275'0002ull; // "stru"
        constexpr std::uint64_t kLayerStage = 0x6c61'7972'0003ull;     // "layr"

        constexpr double kDegrees = std::numbers::pi / 180.0;

        // Fold flanks never get steeper than this (the first version has no overturned folds).
        constexpr double kMaxFoldSlope = 1.7320508075688772; // tan 60 degrees
        constexpr double kFoldWavelengthRatio = 0.45;

        // A share of the formations varies so much sideways that it thins to nothing in
        // places and survives as lenses (thickness * (1 + variability * noise) <= 0).
        constexpr double kLensShare = 0.2;
        constexpr double kLensMinVariability = 1.2;
        constexpr double kLensMaxVariability = 2.0;

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

        double Lerp(double first, double second, double amount)
        {
            return first + (second - first) * amount;
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
        m_fallbackPile = { { granite != nullptr ? granite->material : kFirstRockMaterial, 0 } };
        m_fallbackStructure.layers = { LayerShape{} };

        const std::uint64_t seed = map.GetPlanet().GetSeed();
        m_piles.reserve(map.GetProvinces().size());
        m_structures.reserve(map.GetProvinces().size());
        for (const Province& province : map.GetProvinces())
        {
            const ProvinceColumn& column = m_catalog->ColumnFor(province.type);
            const StructureStyle& style = column.style;
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

            Structure structure;
            std::vector<Layer> pile;
            const std::size_t stripped = StrippedLayers(province.erosionLevel, layers.size());
            for (std::size_t i = stripped; i < layers.size(); ++i)
            {
                Layer layer;
                layer.rock = layers[i].rock;
                LayerShape shape;
                if (i + 1 < layers.size())
                {
                    // Formations vary in thickness from place to place; each province gets its own.
                    const double jitter = 0.8 + 0.45 * GeologyRandom::Unit(
                        GeologyRandom::Stream(seed, kPileStage, province.id, i));
                    layer.thicknessMeters = std::max(
                        1, static_cast<int>(std::lround(layers[i].thicknessMeters * m_config.strataThicknessScale * jitter)));
                    shape.variability = Lerp(style.minVariability, style.maxVariability, GeologyRandom::Unit(
                        GeologyRandom::Stream(seed, kLayerStage, province.id, i * 3 + 0)));
                    if (GeologyRandom::Unit(GeologyRandom::Stream(seed, kLayerStage, province.id, i * 3 + 100)) < kLensShare)
                    {
                        shape.variability = Lerp(kLensMinVariability, kLensMaxVariability, GeologyRandom::Unit(
                            GeologyRandom::Stream(seed, kLayerStage, province.id, i * 3 + 200)));
                    }
                    shape.noiseWavelengthMeters = 300.0 + 900.0 * GeologyRandom::Unit(
                        GeologyRandom::Stream(seed, kLayerStage, province.id, i * 3 + 1));
                    shape.noiseSeed = GeologyRandom::Stream(seed, kLayerStage, province.id, i * 3 + 2);
                }
                pile.push_back(layer);
                structure.layers.push_back(shape);
            }

            // Dip and folds. Strongly deformed provinces are compressed along the axis of the
            // plate motion, so their layers fall along it and folds run across it; the others
            // lie in an arbitrary direction.
            const auto draw = [&](std::uint64_t index)
            {
                return GeologyRandom::Unit(GeologyRandom::Stream(seed, kStructureStage, province.id, index));
            };
            structure.seed = GeologyRandom::Stream(seed, kStructureStage, province.id, 100);
            structure.dipDegrees = Lerp(style.minDipDegrees, style.maxDipDegrees, draw(1));
            const bool compressed = style.maxDipDegrees >= 15.0;
            const double axisAzimuth = compressed ? province.compressionAzimuthDegrees : 180.0 * draw(2);
            structure.dipAzimuthDegrees = axisAzimuth + (draw(3) < 0.5 ? 0.0 : 180.0);
            const double slope = std::tan(structure.dipDegrees * kDegrees);
            const double dipAzimuth = structure.dipAzimuthDegrees * kDegrees;
            structure.slopeEast = -slope * std::sin(dipAzimuth);
            structure.slopeNorth = -slope * std::cos(dipAzimuth);

            const double longestWavelength = 1500.0 + 3500.0 * draw(4);
            for (int i = 0; i < style.foldCount; ++i)
            {
                Fold fold;
                fold.wavelengthMeters = longestWavelength * std::pow(kFoldWavelengthRatio, i);
                fold.amplitudeMeters = std::min(
                    style.foldAmplitude * std::pow(fold.wavelengthMeters, 0.8),
                    kMaxFoldSlope * fold.wavelengthMeters / (2.0 * std::numbers::pi));
                const double direction = (axisAzimuth + 30.0 * (draw(10 + static_cast<std::uint64_t>(i)) - 0.5)) * kDegrees;
                fold.directionEast = std::sin(direction);
                fold.directionNorth = std::cos(direction);
                fold.phase = 2.0 * std::numbers::pi * draw(20 + static_cast<std::uint64_t>(i));
                structure.folds.push_back(fold);
            }
            structure.warpWavelengthMeters = 400.0 + 600.0 * draw(5);

            m_piles.push_back(std::move(pile));
            m_structures.push_back(std::move(structure));
        }
    }

    const std::vector<GeologyModel::Layer>& GeologyModel::PileOf(ProvinceId province) const noexcept
    {
        if (province == kNoProvince || province > m_piles.size())
        {
            return m_fallbackPile;
        }
        return m_piles[province - 1];
    }

    const GeologyModel::Structure& GeologyModel::StructureOf(ProvinceId province) const noexcept
    {
        if (province == kNoProvince || province > m_structures.size())
        {
            return m_fallbackStructure;
        }
        return m_structures[province - 1];
    }

    MaterialId GeologyModel::ExposedRock(ProvinceId province) const noexcept
    {
        return PileOf(province).front().rock;
    }

    double GeologyModel::Displacement(const Structure& structure, double eastMeters, double northMeters) noexcept
    {
        double lift = structure.slopeEast * eastMeters + structure.slopeNorth * northMeters;
        if (!structure.folds.empty())
        {
            // The fold axes bend where the warp noise says, so folds are not straight cylinders.
            const double warp = structure.warpRadians * GeologyRandom::ValueNoise2(
                structure.seed, eastMeters / structure.warpWavelengthMeters, northMeters / structure.warpWavelengthMeters);
            for (const Fold& fold : structure.folds)
            {
                const double along = fold.directionEast * eastMeters + fold.directionNorth * northMeters;
                lift += fold.amplitudeMeters *
                        std::sin(2.0 * std::numbers::pi * along / fold.wavelengthMeters + fold.phase + warp);
            }
        }
        return lift;
    }

    double GeologyModel::Displacement(ProvinceId province, double eastMeters, double northMeters) const noexcept
    {
        return Displacement(StructureOf(province), eastMeters, northMeters);
    }

    GeologyModel::ColumnProfile GeologyModel::ProfileAt(
        ProvinceId province, double eastMeters, double northMeters, int pileTopMeters) const
    {
        const std::vector<Layer>& pile = PileOf(province);
        const Structure& structure = StructureOf(province);
        const double lift = Displacement(structure, eastMeters, northMeters);

        ColumnProfile profile;
        double height = pileTopMeters; // stratigraphic height of the top of the current layer
        const std::size_t count = std::min<std::size_t>(pile.size(), kMaxProfileLayers);
        for (std::size_t i = 0; i < count; ++i)
        {
            const bool basement = i + 1 == count;
            double bottomHeight = std::numeric_limits<double>::lowest();
            if (!basement)
            {
                const LayerShape& shape = structure.layers[i];
                const double noise = GeologyRandom::ValueNoise2(
                    shape.noiseSeed, eastMeters / shape.noiseWavelengthMeters, northMeters / shape.noiseWavelengthMeters);
                // A layer thins to nothing where the variation takes all of its thickness.
                const double thickness = std::max(0.0, pile[i].thicknessMeters * (1.0 + shape.variability * noise));
                if (thickness <= 0.0)
                {
                    continue; // pinched out here
                }
                bottomHeight = height - thickness;
            }

            const int slot = profile.count++;
            profile.rock[static_cast<std::size_t>(slot)] = pile[i].rock;
            profile.bottom[static_cast<std::size_t>(slot)] = basement
                ? std::numeric_limits<int>::min()
                : static_cast<int>(std::lround(bottomHeight + lift));
            height = bottomHeight;
        }
        if (profile.count == 0)
        {
            profile.count = 1;
            profile.rock[0] = pile.back().rock;
            profile.bottom[0] = std::numeric_limits<int>::min();
        }
        return profile;
    }

    MaterialId GeologyModel::RockAt(const ColumnProfile& profile, int elevationMeters) noexcept
    {
        for (int i = 0; i < profile.count; ++i)
        {
            if (elevationMeters >= profile.bottom[static_cast<std::size_t>(i)])
            {
                return profile.rock[static_cast<std::size_t>(i)];
            }
        }
        return profile.rock[static_cast<std::size_t>(profile.count - 1)];
    }

    MaterialId GeologyModel::BedrockAt(ProvinceId province, double eastMeters, double northMeters, int elevationMeters,
                                       int pileTopMeters) const
    {
        return RockAt(ProfileAt(province, eastMeters, northMeters, pileTopMeters), elevationMeters);
    }
}

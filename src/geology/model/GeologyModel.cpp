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
        constexpr std::uint64_t kFaultStage = 0x6661'756c'0004ull;     // "faul"

        // Faults of lattice cells up to this many cells away can reach a column.
        constexpr int kFaultCellReach = 2;
        constexpr double kFaultMinDip[3] = { 50.0, 20.0, 80.0 };  // normal, thrust, strike-slip
        constexpr double kFaultMaxDip[3] = { 70.0, 40.0, 88.0 };

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

    GeologyModel::GeologyModel(std::shared_ptr<const GeologyCatalog> catalog, const GeologyPlanetView& map,
                               GeologyConfig config)
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

        const std::uint64_t seed = map.GetSeed();
        m_piles.reserve(map.GetProvinces().size());
        m_structures.reserve(map.GetProvinces().size());
        for (const Province& province : map.GetProvinces())
        {
            const ProvinceColumn& column = m_catalog->ColumnFor(province.type);
            const StructureStyle& style = column.style;

            // The layers the province can have at all: a layer that needs a condition of the
            // biosphere (coal needs land plants) is left out if the time it would have been
            // laid down in comes before the condition. Dropping a layer shifts the times of the
            // others, so this repeats until nothing more goes.
            std::vector<ColumnLayer> bottomUp = column.bottomUp;
            for (int pass = 0; pass < 16; ++pass)
            {
                const std::vector<TimeSpan> timeline = CoverTimeline(
                    map.GetEpochs().PlanetAgeMyr(), province.eventAgeMyr, province.meanCrustAgeMyr, province.type,
                    bottomUp.size() - 1);
                bool removed = false;
                for (std::size_t k = 0; k < timeline.size(); ++k)
                {
                    const BiosphereState state = map.GetEpochs().BiosphereAt(0.5 * (timeline[k].startMyr + timeline[k].endMyr));
                    if (!Satisfied(bottomUp[k + 1].needs, state))
                    {
                        bottomUp.erase(bottomUp.begin() + static_cast<std::ptrdiff_t>(k + 1));
                        removed = true;
                        break;
                    }
                }
                if (!removed)
                {
                    break;
                }
            }
            // Top-down.
            const std::vector<ColumnLayer> layers(bottomUp.rbegin(), bottomUp.rend());

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
            structure.faults = style.faults;
            structure.compressionAzimuthDegrees = province.compressionAzimuthDegrees;

            // The history behind the pile: the formations of the whole column, bottom first, with
            // the ones erosion stripped off marked.
            HistoryInput input;
            input.seed = seed;
            input.province = province.id;
            input.type = province.type;
            input.planetAgeMyr = map.GetEpochs().PlanetAgeMyr();
            input.eventAgeMyr = province.eventAgeMyr;
            input.crustAgeMyr = province.meanCrustAgeMyr;
            input.dipDegrees = structure.dipDegrees;
            input.foldCount = style.foldCount;
            input.maxFaultSlipMeters = style.faults.maxSlipMeters;
            input.erosionDepthKm = province.erosionDepthKm;
            for (std::size_t b = 0; b < layers.size(); ++b)
            {
                const std::size_t topDown = layers.size() - 1 - b;
                HistoryFormation formation;
                formation.rock = layers[topDown].rock;
                const Rock* info = m_catalog->GetRock(formation.rock);
                formation.rockClass = info != nullptr ? info->rockClass : RockClass::SedimentaryClastic;
                formation.thicknessMeters = layers[topDown].thicknessMeters;
                formation.erased = topDown < stripped;
                input.formationsBottomUp.push_back(formation);
            }
            ProvinceHistory history = BuildProvinceHistory(input);
            std::vector<int> pileEvents;
            for (std::size_t p = 0; p < pile.size(); ++p)
            {
                const std::size_t bottomUp = layers.size() - 1 - (stripped + p);
                pileEvents.push_back(history.formationEvents[bottomUp]);
            }
            m_histories.push_back(std::move(history));
            m_pileEvents.push_back(std::move(pileEvents));

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

    GeologyModel::ColumnProfile GeologyModel::BaseProfileAt(
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

            const int bottom = basement ? std::numeric_limits<int>::min()
                                        : static_cast<int>(std::lround(bottomHeight + lift));
            if (profile.count > 0 && profile.rock[static_cast<std::size_t>(profile.count - 1)] == pile[i].rock)
            {
                // The layer between two pieces of one rock has pinched out: they are one layer.
                profile.bottom[static_cast<std::size_t>(profile.count - 1)] = bottom;
            }
            else
            {
                const int slot = profile.count++;
                profile.rock[static_cast<std::size_t>(slot)] = pile[i].rock;
                profile.bottom[static_cast<std::size_t>(slot)] = bottom;
                profile.formation[static_cast<std::size_t>(slot)] = static_cast<std::uint8_t>(i);
            }
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

    std::vector<GeologyModel::Fault> GeologyModel::FaultsNear(
        ProvinceId province, double eastMeters, double northMeters) const
    {
        const Structure& structure = StructureOf(province);
        const FaultStyle& style = structure.faults;
        std::vector<Fault> faults;
        if (!(style.probability > 0.0) || province == kNoProvince)
        {
            return faults;
        }

        const double spacing = style.spacingMeters;
        const auto cellEast = static_cast<std::int64_t>(std::floor(eastMeters / spacing));
        const auto cellNorth = static_cast<std::int64_t>(std::floor(northMeters / spacing));
        for (int di = -kFaultCellReach; di <= kFaultCellReach; ++di)
        {
            for (int dj = -kFaultCellReach; dj <= kFaultCellReach; ++dj)
            {
                const std::int64_t ci = cellEast + di;
                const std::int64_t cj = cellNorth + dj;
                const std::uint64_t key = GeologyRandom::Stream(
                    structure.seed, kFaultStage, static_cast<std::uint64_t>(ci), static_cast<std::uint64_t>(cj));
                const auto draw = [key](std::uint64_t index)
                {
                    return GeologyRandom::Unit(GeologyRandom::Mix(key + index * 0x9e3779b97f4a7c15ull));
                };
                if (draw(0) >= style.probability)
                {
                    continue;
                }

                Fault fault;
                fault.centerEast = (static_cast<double>(ci) + 0.15 + 0.7 * draw(1)) * spacing;
                fault.centerNorth = (static_cast<double>(cj) + 0.15 + 0.7 * draw(2)) * spacing;
                const double kindDraw = draw(3);
                fault.kind = kindDraw < style.normalShare ? FaultKind::Normal
                    : kindDraw < style.normalShare + style.thrustShare ? FaultKind::Thrust
                    : FaultKind::StrikeSlip;

                // Normal faults and thrusts strike across the axis of plate motion (which stretches
                // or compresses the crust along it); strike-slip faults lie at an angle to it.
                const double axis = structure.compressionAzimuthDegrees;
                const double jitter = 30.0 * (draw(4) - 0.5);
                const double side = draw(5) < 0.5 ? 1.0 : -1.0;
                fault.strikeAzimuthDegrees = fault.kind == FaultKind::StrikeSlip
                    ? axis + side * (25.0 + 20.0 * draw(6))
                    : axis + 90.0 + jitter;
                const auto kindIndex = static_cast<std::size_t>(fault.kind);
                fault.dipDegrees = Lerp(kFaultMinDip[kindIndex], kFaultMaxDip[kindIndex], draw(7));
                fault.dipAzimuthDegrees = fault.strikeAzimuthDegrees + (draw(8) < 0.5 ? 90.0 : -90.0);
                fault.strikeRadiusMeters = spacing * (0.7 + 0.6 * draw(9));
                fault.dipRadiusMeters = spacing * (0.5 + 0.5 * draw(10));
                fault.slipMeters = Lerp(style.minSlipMeters, style.maxSlipMeters, draw(11));
                fault.sense = draw(12) < 0.5 ? 1.0 : -1.0;
                fault.age = draw(13);
                fault.centerOffsetMeters = -(10.0 + 90.0 * draw(14));

                // Close enough to matter: the fault itself, or where a displaced block came from.
                const double reach = std::max(fault.strikeRadiusMeters, fault.dipRadiusMeters) + fault.slipMeters;
                if (std::hypot(fault.centerEast - eastMeters, fault.centerNorth - northMeters) <= reach)
                {
                    faults.push_back(fault);
                }
            }
        }

        if (faults.size() > static_cast<std::size_t>(kMaxFaultsPerColumn))
        {
            std::sort(faults.begin(), faults.end(), [](const Fault& a, const Fault& b) { return a.slipMeters > b.slipMeters; });
            faults.resize(static_cast<std::size_t>(kMaxFaultsPerColumn));
        }
        std::sort(faults.begin(), faults.end(), [](const Fault& a, const Fault& b)
        {
            return a.age != b.age ? a.age < b.age : a.centerEast < b.centerEast;
        });
        return faults;
    }

    GeologyModel::FaultEffect GeologyModel::EffectOf(
        const Fault& fault, double eastMeters, double northMeters, int pileTopMeters) noexcept
    {
        const double beta = fault.dipDegrees * kDegrees;
        const double strike = fault.strikeAzimuthDegrees * kDegrees;
        const double dipAzimuth = fault.dipAzimuthDegrees * kDegrees;
        const double strikeEast = std::sin(strike);
        const double strikeNorth = std::cos(strike);
        const double dipEast = std::sin(dipAzimuth);
        const double dipNorth = std::cos(dipAzimuth);

        const double dx = eastMeters - fault.centerEast;
        const double dy = northMeters - fault.centerNorth;
        const double along = dx * strikeEast + dy * strikeNorth;
        const double downDip = dx * dipEast + dy * dipNorth; // horizontal distance down the dip
        const double slant = downDip / std::cos(beta);       // the same, measured along the plane
        const double ellipse = (along / fault.strikeRadiusMeters) * (along / fault.strikeRadiusMeters) +
                               (slant / fault.dipRadiusMeters) * (slant / fault.dipRadiusMeters);

        FaultEffect effect;
        if (ellipse >= 1.0)
        {
            return effect;
        }
        // Slip dies out smoothly towards the edge of the fault.
        const double amplitude = fault.slipMeters * (1.0 - ellipse) * (1.0 - ellipse);
        effect.amplitude = amplitude;
        effect.cutElevation = static_cast<double>(pileTopMeters) + fault.centerOffsetMeters - std::tan(beta) * downDip;

        // Displacement of the block above the plane.
        switch (fault.kind)
        {
        case FaultKind::Normal: // the hanging wall slides down the plane
            effect.shiftEast = amplitude * std::cos(beta) * dipEast;
            effect.shiftNorth = amplitude * std::cos(beta) * dipNorth;
            effect.lift = -amplitude * std::sin(beta);
            break;
        case FaultKind::Thrust: // and is pushed up it
            effect.shiftEast = -amplitude * std::cos(beta) * dipEast;
            effect.shiftNorth = -amplitude * std::cos(beta) * dipNorth;
            effect.lift = amplitude * std::sin(beta);
            break;
        case FaultKind::StrikeSlip:
            effect.shiftEast = fault.sense * amplitude * strikeEast;
            effect.shiftNorth = fault.sense * amplitude * strikeNorth;
            break;
        }
        return effect;
    }

    namespace
    {
        using Profile = GeologyModel::ColumnProfile;

        constexpr int kBasementBottom = std::numeric_limits<int>::min();

        // Rocks above `cut` from `upper` and rocks below it from `lower`.
        Profile Compose(const Profile& upper, const Profile& lower, int cut)
        {
            struct Piece
            {
                MaterialId rock;
                int bottom;
                std::uint8_t formation;
            };
            std::vector<Piece> layers;
            for (int i = 0; i < upper.count; ++i)
            {
                const int bottom = upper.bottom[static_cast<std::size_t>(i)];
                const int top = i == 0 ? std::numeric_limits<int>::max() : upper.bottom[static_cast<std::size_t>(i - 1)];
                if (top <= cut)
                {
                    break;
                }
                layers.push_back({ upper.rock[static_cast<std::size_t>(i)], std::max(bottom, cut), upper.formation[static_cast<std::size_t>(i)] });
                if (bottom <= cut)
                {
                    break;
                }
            }
            for (int j = 0; j < lower.count; ++j)
            {
                const int bottom = lower.bottom[static_cast<std::size_t>(j)];
                if (bottom < cut)
                {
                    layers.push_back({ lower.rock[static_cast<std::size_t>(j)], bottom, lower.formation[static_cast<std::size_t>(j)] });
                }
            }

            // Neighbouring pieces of one rock are one layer.
            for (std::size_t i = 0; i + 1 < layers.size();)
            {
                if (layers[i].rock == layers[i + 1].rock)
                    layers.erase(layers.begin() + static_cast<std::ptrdiff_t>(i));
                else
                    ++i;
            }
            // Too many layers for a profile: drop the thinnest interior ones (never the top or basement).
            while (layers.size() > static_cast<std::size_t>(GeologyModel::kMaxProfileLayers))
            {
                std::size_t thinnest = 1;
                long long smallest = std::numeric_limits<long long>::max();
                for (std::size_t i = 1; i + 1 < layers.size(); ++i)
                {
                    const long long thickness = static_cast<long long>(layers[i - 1].bottom) - layers[i].bottom;
                    if (thickness < smallest)
                    {
                        smallest = thickness;
                        thinnest = i;
                    }
                }
                layers.erase(layers.begin() + static_cast<std::ptrdiff_t>(thinnest));
            }

            Profile result;
            for (const Piece& piece : layers)
            {
                result.rock[static_cast<std::size_t>(result.count)] = piece.rock;
                result.bottom[static_cast<std::size_t>(result.count)] = piece.bottom;
                result.formation[static_cast<std::size_t>(result.count)] = piece.formation;
                ++result.count;
            }
            result.bottom[static_cast<std::size_t>(result.count - 1)] = kBasementBottom;
            return result;
        }
    }

    GeologyModel::ColumnProfile GeologyModel::FaultedProfile(
        ProvinceId province, const std::vector<Fault>& faults, std::size_t count,
        double eastMeters, double northMeters, int pileTopMeters) const
    {
        if (count == 0)
        {
            return BaseProfileAt(province, eastMeters, northMeters, pileTopMeters);
        }
        // The youngest fault of the first `count` cuts everything the older ones made.
        const Fault& fault = faults[count - 1];
        const FaultEffect effect = EffectOf(fault, eastMeters, northMeters, pileTopMeters);
        if (!(effect.amplitude > 0.0))
        {
            return FaultedProfile(province, faults, count - 1, eastMeters, northMeters, pileTopMeters);
        }

        const ColumnProfile lower = FaultedProfile(province, faults, count - 1, eastMeters, northMeters, pileTopMeters);
        ColumnProfile upper = FaultedProfile(
            province, faults, count - 1, eastMeters - effect.shiftEast, northMeters - effect.shiftNorth, pileTopMeters);
        const int lift = static_cast<int>(std::lround(effect.lift));
        for (int i = 0; i + 1 < upper.count; ++i)
        {
            upper.bottom[static_cast<std::size_t>(i)] += lift;
        }
        return Compose(upper, lower, static_cast<int>(std::lround(effect.cutElevation)));
    }

    GeologyModel::ColumnProfile GeologyModel::ProfileAt(
        ProvinceId province, double eastMeters, double northMeters, int pileTopMeters) const
    {
        const std::vector<Fault> faults = FaultsNear(province, eastMeters, northMeters);
        return FaultedProfile(province, faults, faults.size(), eastMeters, northMeters, pileTopMeters);
    }

    const ProvinceHistory& GeologyModel::HistoryOf(ProvinceId province) const noexcept
    {
        if (province == kNoProvince || province > m_histories.size())
        {
            return m_fallbackHistory;
        }
        return m_histories[province - 1];
    }

    EventId GeologyModel::FormationEvent(ProvinceId province, std::size_t pileIndex) const noexcept
    {
        if (province == kNoProvince || province > m_histories.size() || pileIndex >= m_pileEvents[province - 1].size())
        {
            return 0;
        }
        const ProvinceHistory& history = m_histories[province - 1];
        return history.events[static_cast<std::size_t>(m_pileEvents[province - 1][pileIndex])].id;
    }

    std::vector<EventId> GeologyModel::ProvenanceAt(
        ProvinceId province, double eastMeters, double northMeters, int elevationMeters, int pileTopMeters) const
    {
        std::vector<EventId> chain;
        const ProvinceHistory& history = HistoryOf(province);
        if (history.events.empty())
        {
            return chain;
        }

        const ColumnProfile profile = ProfileAt(province, eastMeters, northMeters, pileTopMeters);
        chain.push_back(FormationEvent(province, FormationAt(profile, elevationMeters)));

        // Everything in the province was folded after it formed.
        if (history.foldingEvent >= 0)
            chain.push_back(history.events[static_cast<std::size_t>(history.foldingEvent)].id);

        // A fault displaced the rock if the point lies in the block above a fault plane.
        for (const Fault& fault : FaultsNear(province, eastMeters, northMeters))
        {
            const FaultEffect effect = EffectOf(fault, eastMeters, northMeters, pileTopMeters);
            if (effect.amplitude > 0.0 && elevationMeters > effect.cutElevation && history.faultingEvent >= 0)
            {
                chain.push_back(history.events[static_cast<std::size_t>(history.faultingEvent)].id);
                break;
            }
        }
        if (history.erosionEvent >= 0)
            chain.push_back(history.events[static_cast<std::size_t>(history.erosionEvent)].id);
        return chain;
    }

    std::size_t GeologyModel::FormationAt(const ColumnProfile& profile, int elevationMeters) noexcept
    {
        for (int i = 0; i < profile.count; ++i)
        {
            if (elevationMeters >= profile.bottom[static_cast<std::size_t>(i)])
            {
                return profile.formation[static_cast<std::size_t>(i)];
            }
        }
        return profile.formation[static_cast<std::size_t>(profile.count - 1)];
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

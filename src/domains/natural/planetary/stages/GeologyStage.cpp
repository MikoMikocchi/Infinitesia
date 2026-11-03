#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/stages/PlanetStages.h"
#include "domains/natural/planetary/internal/GridDistance.h"
#include "domains/natural/planetary/internal/GenerationMath.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace Infinitesia
{
    using namespace GenerationDetail;

    namespace
    {
        // Provenance of the numbers below: ocean heat flow and the half-space cooling
        // law follow Parsons & Sclater (1977) and Stein & Stein (1992); continental heat
        // flow by crust age follows the global means of Pollack et al. (1993). Sediment
        // thicknesses are gameplay calibrations to the ranges of Earth's basins, margins
        // and abyssal plains (docs/03_GEOLOGY.md, section 6.1).
        constexpr double kOceanicCrustAgeScaleMyr = 180.0;
        constexpr double kEarthAgeMyr = 4540.0;

        constexpr std::uint32_t kHotspotSalt = 0x6e07a001u;

        // 1 at distance 0, falling smoothly to 0 at `width`.
        double Near(double distanceKm, double widthKm)
        {
            return 1.0 - SmootherStep(distanceKm / widthKm);
        }

        bool IsSubduction(PlateBoundaryKind kind)
        {
            return kind == PlateBoundaryKind::OceanicSubduction ||
                   kind == PlateBoundaryKind::ContinentalSubduction;
        }

        struct Hotspot
        {
            Vec3d center{};
            double radiusKm = 0.0;
        };

        std::vector<Hotspot> MakeHotspots(std::uint32_t seed)
        {
            const int count = 5 + std::min(5, static_cast<int>(RandUnit(seed ^ kHotspotSalt, 0xfffeu) * 5.0));
            std::vector<Hotspot> hotspots(static_cast<std::size_t>(count));
            for (int i = 0; i < count; ++i)
            {
                const auto tag = static_cast<std::uint32_t>(i) * 4u;
                const double latitude =
                    std::asin(RandRange(seed ^ kHotspotSalt, tag + 1u, -1.0, 1.0)) * 180.0 / std::numbers::pi;
                const double longitude = RandRange(seed ^ kHotspotSalt, tag + 2u, 0.0, 360.0);
                hotspots[static_cast<std::size_t>(i)].center = LatLonToCart(latitude, longitude);
                hotspots[static_cast<std::size_t>(i)].radiusKm = RandRange(seed ^ kHotspotSalt, tag + 3u, 350.0, 800.0);
            }
            return hotspots;
        }

        double OceanicHeatFlow(double ageMyr)
        {
            // Half-space cooling, q = 473 / sqrt(t) mW/m2, with the old-plate floor.
            return std::clamp(473.0 / std::sqrt(std::max(ageMyr, 1.0)), 48.0, 300.0);
        }

        double ContinentalHeatFlow(double earthEquivalentAgeMyr)
        {
            // Archean cores ~41, Proterozoic ~55, Paleozoic ~61, Meso-Cenozoic ~66 mW/m2.
            const double age = std::max(earthEquivalentAgeMyr, 0.0);
            if (age >= 2500.0)
                return 41.0;
            if (age >= 1000.0)
                return Lerp(55.0, 41.0, (age - 1000.0) / 1500.0);
            if (age >= 250.0)
                return Lerp(61.0, 55.0, (age - 250.0) / 750.0);
            return Lerp(66.0, 61.0, age / 250.0);
        }
    }

    void RunGeology(PlanetBuild& build)
    {
        build.data.epochs = PlanetEpochTable(build.data.climate.planetAgeYears / 1.0e6);
        const int latB = build.LatBands();
        const int lonS = build.LonSegs();
        const std::size_t cellCount = static_cast<std::size_t>(latB) * lonS;
        const std::uint32_t seed = build.data.planet.GetSeed();
        const double radiusKm = build.data.planet.GetRadius() / 1000.0;
        const double planetAgeMyr = std::max(build.data.climate.planetAgeYears / 1.0e6, 1.0);

        // Same heat budget as the plate count in tectonics, normalized so that an
        // Earth-mass planet of Earth's age is 1.
        const double massFraction = build.data.planet.GetParameters().massKilograms / 5.972e24;
        const double ageFraction = std::clamp(build.data.climate.planetAgeYears / 4.5e9, 0.0, 1.0);
        const double thermalScale =
            std::sqrt(massFraction) * (1.0 - 0.4 * ageFraction) / 0.6;

        // ---- mantle plumes, away from plate boundaries
        const std::vector<Hotspot> hotspots = MakeHotspots(seed);
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                auto& c = build.Cell(la, lo);
                const Vec3d point = LatLonToCart(c.latitudeDegrees, c.longitudeDegrees);
                double influence = 0.0;
                for (const Hotspot& hotspot : hotspots)
                {
                    const double dot = std::clamp(
                        point.x * hotspot.center.x + point.y * hotspot.center.y + point.z * hotspot.center.z,
                        -1.0, 1.0);
                    const double distanceKm = std::acos(dot) * radiusKm;
                    influence = std::max(influence, SmootherStep(1.0 - distanceKm / hotspot.radiusKm));
                }
                // A plume under a plate boundary is the boundary's own magmatism.
                c.hotspotInfluence = influence * SmootherStep(c.distanceToBoundaryKm / 200.0);
                c.paleoLatitudeDegrees = c.latitudeDegrees;
            }
        }

        // ---- distance from the crust of the other kind, for margins and fans
        std::vector<bool> isContinentalCrust(cellCount);
        std::vector<bool> isOceanicCrust(cellCount);
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                const std::size_t index = static_cast<std::size_t>(la) * lonS + lo;
                const bool continental = build.Cell(la, lo).continentalCrustFraction > 0.5;
                isContinentalCrust[index] = continental;
                isOceanicCrust[index] = !continental;
            }
        }
        const std::vector<double> distanceToContinentKm =
            DistanceToSourcesKm(isContinentalCrust, latB, lonS, radiusKm);
        const std::vector<double> distanceToOceanKm =
            DistanceToSourcesKm(isOceanicCrust, latB, lonS, radiusKm);
        // Handed to the provinces stage through the layer, not by a call.
        Field<double>& distanceToOceanLayer =
            build.data.layers.Create<double>(kDistanceToOceanLayer, latB, lonS);
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                distanceToOceanLayer.At(la, lo) = distanceToOceanKm[static_cast<std::size_t>(la) * lonS + lo];
            }
        }

        // ---- sediment cover and heat flow
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                auto& c = build.Cell(la, lo);
                const std::size_t index = static_cast<std::size_t>(la) * lonS + lo;
                const double boundaryDistanceKm = c.distanceToBoundaryKm;
                const bool continental = isContinentalCrust[index];
                const PlateBoundaryKind kind = c.boundarySubtype;

                double sediment = 0.0;
                double heatFlow = 0.0;
                if (!continental)
                {
                    // Pelagic rain thickens with age; terrigenous fans fade with
                    // distance from land; trenches trap what the slab carries down.
                    const double ageMyr = std::clamp(c.crustAge, 0.0, 1.0) * kOceanicCrustAgeScaleMyr;
                    const double pelagic = 0.05 + 0.0025 * ageMyr;
                    const double fan = 3.5 * std::exp(-distanceToContinentKm[index] / 400.0) *
                        (0.4 + 0.6 * std::min(1.0, ageMyr / 60.0));
                    const double trenchFill =
                        (IsSubduction(kind) && c.isSubductingPlate) ? 1.2 * Near(boundaryDistanceKm, 150.0) : 0.0;
                    sediment = std::clamp(pelagic + fan + trenchFill, 0.02, 12.0);

                    heatFlow = OceanicHeatFlow(ageMyr);
                }
                else
                {
                    const double elevationKm = c.elevationNormalized * kMaxElevationKm;
                    // Low ground collects cover; young crust has not been scoured to the
                    // basement.
                    const double lowland = 1.0 - SmootherStep((elevationKm - 0.5) / 1.5);
                    const double youth = std::clamp((0.70 - c.crustAge) / 0.70, 0.0, 1.0);
                    double platform = 0.25 + 8.0 * lowland * (0.1 + 0.9 * youth);
                    if (c.surfaceRock == RockType::GraniticShield && c.crustAge > 0.65)
                    {
                        platform *= 0.15; // exposed shield
                    }

                    // Foreland basin: low ground beside a mountain belt (as in terrain).
                    double nearbyRelief = 0.0;
                    int neighbours = 0;
                    for (int dla = -2; dla <= 2; ++dla)
                    {
                        for (int dlo = -2; dlo <= 2; ++dlo)
                        {
                            const int nla = std::clamp(la + dla, 0, latB - 1);
                            const int nlo = (lo + dlo + lonS) % lonS;
                            nearbyRelief += std::max(0.0, build.Cell(nla, nlo).tectonicReliefKm);
                            ++neighbours;
                        }
                    }
                    nearbyRelief /= neighbours;
                    const double localRelief = std::max(0.0, c.tectonicReliefKm);
                    const double foreland = 2.6 * std::clamp(nearbyRelief - localRelief, 0.0, 1.8);

                    const double rift = kind == PlateBoundaryKind::ContinentalRift
                        ? 3.5 * Near(boundaryDistanceKm, 220.0) : 0.0;

                    // Passive margin wedge: only where no boundary is active.
                    const bool active = IsSubduction(kind) || kind == PlateBoundaryKind::ContinentalCollision;
                    const double margin = 5.0 * (1.0 - SmootherStep(distanceToOceanKm[index] / 300.0)) *
                        (active ? 1.0 - Near(boundaryDistanceKm, 300.0) : 1.0);

                    const double orogenSuppression = 1.0 - 0.9 * std::clamp(localRelief / 3.0, 0.0, 1.0);
                    sediment = std::clamp((platform + foreland + rift + margin) * orogenSuppression, 0.02, 14.0);

                    // Heat flow by crust age on the Earth timeline, plus magmatic belts.
                    double q = ContinentalHeatFlow(c.crustAgeMyr * kEarthAgeMyr / planetAgeMyr);
                    const bool overridingArc = kind == PlateBoundaryKind::ContinentalSubduction && !c.isSubductingPlate;
                    if (overridingArc)
                        q += 35.0 * Near(boundaryDistanceKm, 300.0);
                    if (kind == PlateBoundaryKind::ContinentalRift)
                        q += 30.0 * Near(boundaryDistanceKm, 300.0);
                    if (kind == PlateBoundaryKind::ContinentalCollision)
                        q += 10.0 * Near(boundaryDistanceKm, 300.0);
                    q += 40.0 * c.hotspotInfluence;
                    if (c.isVolcanicZone)
                        q += 25.0;
                    heatFlow = q;
                }

                if (!continental && c.isVolcanicZone)
                {
                    heatFlow += 25.0;
                }
                c.sedimentThicknessKm = sediment;
                c.heatFlowMwPerM2 = std::clamp(heatFlow * thermalScale, 5.0, 400.0);
            }
        }
    }

}

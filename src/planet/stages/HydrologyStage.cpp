#include "planet/PlanetMap.h"
#include "planet/stages/PlanetStages.h"
#include "planet/internal/PlanetMapGenerationHelpers.h"
#include "planet/internal/PlanetMapHydrologyHelpers.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace Infinitesia
{
    using namespace GenerationDetail;

    void RunHydrology(PlanetBuild& build)
    {
        const int latB = build.LatBands();
        const int lonS = build.LonSegs();

        // -----------------------------------------------------------------------
        // Precompute per-latitude cell area weights (spherical shell segment).
        //
        // Area of a latitude-longitude cell on a sphere of radius R:
        //   A_ij = R² · Δλ · (sin φ_N - sin φ_S)
        //
        // where Δλ is the longitude width in radians, φ_N and φ_S are the
        // northern and southern latitude boundaries of the cell.
        //
        // We normalise by the mean cell area so that the water-balance numbers
        // remain comparable to the mm/yr precipitation values.  The area ratio
        // ensures that equatorial cells (which are much larger than polar cells
        // on a lat-lon grid) contribute proportionally more runoff volume.
        // -----------------------------------------------------------------------
        const double radiusKm = build.data.planet.GetRadius() / 1000.0;
        const double deltaLonRad = (2.0 * std::numbers::pi) / lonS;
        const double deltaLatRad = std::numbers::pi / latB;
        std::vector<double> cellAreaKm2(static_cast<std::size_t>(latB));
        double meanCellAreaKm2 = 0.0;
        for (int la = 0; la < latB; ++la)
        {
            const double phiS = -std::numbers::pi / 2.0 + la * deltaLatRad;
            const double phiN = phiS + deltaLatRad;
            cellAreaKm2[static_cast<std::size_t>(la)] =
                radiusKm * radiusKm * deltaLonRad *
                (std::sin(phiN) - std::sin(phiS));
            meanCellAreaKm2 += cellAreaKm2[static_cast<std::size_t>(la)];
        }
        meanCellAreaKm2 /= latB;

        const std::size_t cellCount = static_cast<std::size_t>(latB) * lonS;
        std::vector<double> elevations(cellCount);
        std::vector<bool> oceanCells(cellCount);
        std::vector<double> localNormalizedRunoff(cellCount, 0.0);
        std::vector<double> localRunoffVolume(cellCount, 0.0);
        std::vector<double> etAnnualMmByCell(cellCount, 0.0);
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                const std::size_t index =
                    static_cast<std::size_t>(la) * lonS + lo;
                const auto& cell = build.Cell(la, lo);
                elevations[index] = cell.elevationNormalized;
                oceanCells[index] = cell.isOcean;
                if (cell.isOcean)
                    continue;

                const double etAnnualMm =
                    AnnualEvapotranspirationMm(cell.temperatureCelsius);
                const double runoffMm =
                    std::max(0.0, cell.precipitationMm - etAnnualMm);
                const double areaCellKm2 = cellAreaKm2[static_cast<std::size_t>(la)];
                const double areaRatio =
                    meanCellAreaKm2 > 0.0 ? areaCellKm2 / meanCellAreaKm2 : 1.0;

                localNormalizedRunoff[index] = runoffMm * areaRatio;
                localRunoffVolume[index] = runoffMm * areaCellKm2 * 1e-6;
                etAnnualMmByCell[index] = etAnnualMm;
            }
        }

        GenerationDetail::DrainageNetwork network =
            GenerationDetail::BuildDrainageNetwork(
                latB,
                lonS,
                elevations,
                oceanCells,
                localNormalizedRunoff,
                localRunoffVolume);
        const auto& filledElev = network.filledElevations;
        const auto& waterAccum = network.normalizedAccumulation;
        const double maxWater =
            *std::max_element(waterAccum.begin(), waterAccum.end());

        // -----------------------------------------------------------------------
        // Classify lakes and propagate runoff volume with lake losses.
        //
        // A cell is a lake candidate only when Priority-Flood raised it by more
        // than a noise epsilon (a genuine closed-basin sink) and it has
        // non-trivial upstream drainage.  Cells are visited from high to low
        // filled elevation, so all upstream outflow has already been added when
        // a cell is processed.  A lake keeps what it evaporates or infiltrates
        // and passes only its spill downstream; a sink whose losses exceed its
        // inflow is a dry endorheic basin and passes nothing downstream.
        // -----------------------------------------------------------------------
        constexpr double kRiverDischargeThreshold = 0.005;
        constexpr double kLakeFillThreshold = 0.001;
        constexpr double kUpstreamThreshold = 0.002;
        constexpr double kSecondsPerYear = 31557600.0;
        constexpr double kMaxLakeCellFraction = 0.6; // lakes never fill a cell
        constexpr double kMinLakeCellFraction = 0.01; // smaller = pond, ignored
        constexpr double kBowlScaleKm = 0.3; // depth scale of f_topo saturation

        std::vector<double> outflowKm3 = localRunoffVolume;
        PlanetWaterBalance balance;
        for (const double runoff : localRunoffVolume)
            balance.landRunoffKm3 += runoff;
        for (const int order : network.processingOrder)
        {
            const std::size_t idx = static_cast<std::size_t>(order);
            auto& c = build.Cell(order / lonS, order % lonS);
            if (c.isOcean)
                continue;

            c.drainageAccumulation = maxWater > 0.0
                ? waterAccum[idx] / maxWater
                : 0.0;

            const double fillDeltaKm =
                (filledElev[idx] - c.elevationNormalized) * kMaxElevationKm;
            const bool genuineSink =
                fillDeltaKm > kLakeFillThreshold * kMaxElevationKm;
            const bool hasUpstream = c.drainageAccumulation > kUpstreamThreshold;
            if (genuineSink && hasUpstream)
            {
                const double areaKm2 = cellAreaKm2[static_cast<std::size_t>(c.latitudeIndex)];
                const double catchmentInflowKm3 =
                    std::max(0.0, outflowKm3[idx] - localRunoffVolume[idx]);
                double infiltrationMm = 40.0;
                switch (c.surfaceRock)
                {
                case RockType::BasalticOceanic:   infiltrationMm = 45.0; break;
                case RockType::GraniticShield:    infiltrationMm = 20.0; break;
                case RockType::SedimentaryBasin:  infiltrationMm = 80.0; break;
                case RockType::VolcanicIgneous:   infiltrationMm = 55.0; break;
                case RockType::MetamorphicBelt:   infiltrationMm = 25.0; break;
                case RockType::FoldedSedimentary: infiltrationMm = 65.0; break;
                }

                // The lake covers only a fraction f of the cell; the rest stays
                // land and keeps producing ordinary runoff into the lake.
                //   surplus(f) = Q_up + R*A + f*A*(P - R - E - I)
                //              = Q_up + R*A - f*A*d
                // with d = I + max(0, E - P) > 0 the net loss per lake area.
                const double precipMm = c.precipitationMm;
                const double evapMm = etAnnualMmByCell[idx];
                const double runoffMm = std::max(0.0, precipMm - evapMm);
                const double netLossMm =
                    infiltrationMm + std::max(0.0, evapMm - precipMm);
                const double landSupplyKm3 =
                    catchmentInflowKm3 + runoffMm * areaKm2 * 1e-6;
                const double netLossKm3 = netLossMm * areaKm2 * 1e-6;

                // Equilibrium (closed-basin) fraction where inflow == losses.
                const double equilibriumFraction = netLossKm3 > 0.0
                    ? landSupplyKm3 / netLossKm3
                    : kMaxLakeCellFraction;
                // Largest fraction the depression's depth can hold at spill.
                const double topoFraction = std::min(
                    kMaxLakeCellFraction,
                    1.0 - std::exp(-fillDeltaKm / kBowlScaleKm));

                const bool overflows = equilibriumFraction > topoFraction;
                const double lakeFraction =
                    std::min(equilibriumFraction, topoFraction);

                double annualSpillKm3 = 0.0;
                const double sinkInflowKm3 = outflowKm3[idx];
                if (lakeFraction < kMinLakeCellFraction)
                {
                    balance.drySinkLossKm3 += sinkInflowKm3;
                }
                else
                {
                    const double lakeAreaKm2 = lakeFraction * areaKm2;
                    const double evapKm3 = evapMm * lakeAreaKm2 * 1e-6;
                    const double infilKm3 = infiltrationMm * lakeAreaKm2 * 1e-6;
                    const double inflowKm3 =
                        catchmentInflowKm3 +
                        runoffMm * (areaKm2 - lakeAreaKm2) * 1e-6 +
                        precipMm * lakeAreaKm2 * 1e-6;
                    annualSpillKm3 = overflows
                        ? std::max(0.0, inflowKm3 - evapKm3 - infilKm3)
                        : 0.0;

                    const double precipGainKm3 =
                        (precipMm - runoffMm) * lakeAreaKm2 * 1e-6;
                    balance.lakePrecipitationGainKm3 += precipGainKm3;
                    balance.lakeLossKm3 +=
                        sinkInflowKm3 + precipGainKm3 - annualSpillKm3;

                    // Paraboloid bowl: area grows linearly with depth, so a
                    // closed lake below its spill is shallower by the same
                    // ratio and V = 1/2 * A * depth.
                    const double depthKm = overflows
                        ? fillDeltaKm
                        : fillDeltaKm * (lakeFraction / topoFraction);
                    const double floorKm = c.elevationNormalized * kMaxElevationKm;

                    c.isLake = true;
                    c.lakeFraction = lakeFraction;
                    c.lakeSurfaceAreaKm2 = lakeAreaKm2;
                    c.lakeSpillHeightKm = fillDeltaKm;
                    c.lakeSpillElevationKm = floorKm + fillDeltaKm;
                    c.lakeWaterLevelKm = floorKm + depthKm;
                    c.lakeWaterVolumeKm3 = 0.5 * lakeAreaKm2 * depthKm;
                    c.lakeAnnualInflowKm3 = inflowKm3;
                    c.lakeAnnualEvaporationKm3 = evapKm3;
                    c.lakeAnnualInfiltrationKm3 = infilKm3;
                    c.lakeAnnualSpillKm3 = annualSpillKm3;
                }
                outflowKm3[idx] = annualSpillKm3;
            }

            const int target = network.flowDirections[idx];
            if (target >= 0)
                outflowKm3[static_cast<std::size_t>(target)] += outflowKm3[idx];
        }

        for (std::size_t idx = 0; idx < cellCount; ++idx)
        {
            if (oceanCells[idx])
                balance.oceanDischargeKm3 += outflowKm3[idx];
            else if (network.flowDirections[idx] < 0)
                balance.landSinkKm3 += outflowKm3[idx];
        }
        build.data.waterBalance = balance;

        for (std::size_t idx = 0; idx < cellCount; ++idx)
        {
            auto& c = build.data.grid[idx];
            if (c.isOcean)
                continue;
            c.hasRiver = c.drainageAccumulation > kRiverDischargeThreshold &&
                outflowKm3[idx] > 0.0;
            c.riverDischarge = c.hasRiver ? c.drainageAccumulation : 0.0;
            c.annualRunoffVolumeKm3 = c.hasRiver ? outflowKm3[idx] : 0.0;
            c.riverDischargeM3PerSecond =
                c.annualRunoffVolumeKm3 * 1e9 / kSecondsPerYear;
        }
    }
}

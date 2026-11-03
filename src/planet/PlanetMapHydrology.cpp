#include "planet/PlanetMap.h"
#include "planet/internal/PlanetMapGenerationHelpers.h"
#include "planet/internal/PlanetMapHydrologyHelpers.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace Infinitesia
{
    using namespace GenerationDetail;

    void PlanetMap::GenerateHydrology()
    {
        const int latB = LatBands();
        const int lonS = LonSegs();

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
        const double radiusKm = m_planet.GetRadius() / 1000.0;
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
                const auto& cell = Cell(la, lo);
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

        std::vector<double> outflowKm3 = localRunoffVolume;
        for (const int order : network.processingOrder)
        {
            const std::size_t idx = static_cast<std::size_t>(order);
            auto& c = Cell(order / lonS, order % lonS);
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
                const double directPrecipitationKm3 =
                    c.precipitationMm * areaKm2 * 1e-6;
                const double annualInflowKm3 =
                    catchmentInflowKm3 + directPrecipitationKm3;

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

                const double annualEvaporationKm3 =
                    etAnnualMmByCell[idx] * areaKm2 * 1e-6;
                const double annualInfiltrationKm3 =
                    infiltrationMm * areaKm2 * 1e-6;
                const double annualWaterSurplusKm3 =
                    annualInflowKm3 - annualEvaporationKm3 -
                    annualInfiltrationKm3;

                if (annualWaterSurplusKm3 > 0.0)
                {
                    // Treat the depression as a hypsometrically uniform bowl:
                    // its full storage is cell area times depth to the spill.
                    c.isLake = true;
                    c.lakeSurfaceAreaKm2 = areaKm2;
                    c.lakeSpillHeightKm = fillDeltaKm;
                    c.lakeSpillElevationKm =
                        c.elevationNormalized * kMaxElevationKm + fillDeltaKm;
                    c.lakeWaterLevelKm = c.lakeSpillElevationKm;
                    c.lakeWaterVolumeKm3 = areaKm2 * fillDeltaKm;
                    c.lakeAnnualInflowKm3 = annualInflowKm3;
                    c.lakeAnnualEvaporationKm3 = annualEvaporationKm3;
                    c.lakeAnnualInfiltrationKm3 = annualInfiltrationKm3;
                    c.lakeAnnualSpillKm3 = annualWaterSurplusKm3;
                }
                outflowKm3[idx] = std::max(0.0, annualWaterSurplusKm3);
            }

            const int target = network.flowDirections[idx];
            if (target >= 0)
                outflowKm3[static_cast<std::size_t>(target)] += outflowKm3[idx];
        }

        for (std::size_t idx = 0; idx < cellCount; ++idx)
        {
            auto& c = m_grid[idx];
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

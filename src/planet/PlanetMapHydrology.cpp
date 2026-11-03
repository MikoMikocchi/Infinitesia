#include "planet/PlanetMap.h"
#include "planet/internal/PlanetMapGenerationHelpers.h"
#include "planet/internal/PlanetMapHydrologyHelpers.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace Infinitesia
{
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
                    std::clamp(120.0 + cell.temperatureCelsius * 12.0, 0.0, 600.0);
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
        const auto& volumeAccum = network.volumeAccumulation;
        const double maxWater =
            *std::max_element(waterAccum.begin(), waterAccum.end());

        // -----------------------------------------------------------------------
        // Step 5: Classify rivers and lakes.
        //
        // Lakes: a cell is a genuine lake candidate only when the Priority-Flood
        // raised its elevation by more than a noise epsilon (fillDelta test).
        // This separates true closed-basin sinks from cells that were given only
        // a tiny upward nudge to maintain a drainage gradient across a flat area.
        // We additionally require non-trivial upstream drainage to exclude
        // isolated single-cell pits with no contributing catchment.
        // -----------------------------------------------------------------------
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                auto& c = Cell(la, lo);
                const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;

                c.drainageAccumulation = maxWater > 0.0
                    ? waterAccum[idx] / maxWater
                    : 0.0;

                constexpr double kRiverDischargeThreshold = 0.005;
                c.hasRiver = !c.isOcean &&
                    c.drainageAccumulation > kRiverDischargeThreshold;
c.riverDischarge = c.hasRiver ? c.drainageAccumulation : 0.0;
c.annualRunoffVolumeKm3 = c.hasRiver
    ? volumeAccum[idx]
    : 0.0;
c.riverDischargeM3PerSecond = c.hasRiver
    ? c.annualRunoffVolumeKm3 * 1e9 / 31557600.0
    : 0.0;

                constexpr double kLakeFillThreshold = 0.001;
                constexpr double kMaxElevationKm = 9.0;
                const double fillDeltaKm =
                    (filledElev[idx] - c.elevationNormalized) * kMaxElevationKm;
                const bool genuineSink = fillDeltaKm > kLakeFillThreshold * kMaxElevationKm;
                const bool hasUpstream = c.drainageAccumulation > 0.002;
                if (c.isOcean || !genuineSink || !hasUpstream)
                    continue;

                const double areaKm2 = cellAreaKm2[static_cast<std::size_t>(la)];
                const double localRunoffKm3 =
                    localRunoffVolume[idx];
                const double catchmentRunoffKm3 =
                    std::max(0.0, volumeAccum[idx] - localRunoffKm3);
                const double directPrecipitationKm3 =
                    c.precipitationMm * areaKm2 * 1e-6;
                const double annualInflowKm3 =
                    catchmentRunoffKm3 + directPrecipitationKm3;

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
                if (annualWaterSurplusKm3 <= 0.0)
                    continue;

                // Treat the cell depression as a hypsometrically uniform bowl:
                // its full storage is cell area times depth to the lowest spill.
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
        }
    }
}

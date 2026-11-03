#pragma once

#include "planet/PlanetMap.h"
#include "planet/internal/PlateKinematics.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// Steps of the tectonics stage. RunTectonics (TectonicsStage.cpp) calls them in order;
// each lives in its own file next to this header.
namespace Infinitesia::TectonicsDetail
{
    // Spatially continuous continental lithosphere, independent of plate boundaries.
    struct CrustField
    {
        std::vector<double> field;
        std::vector<bool> isContinental;
        double threshold = 0.5;
    };

    // The two plates nearest a cell, by great-circle distance in degrees.
    struct CellAssignment
    {
        int best = 0, second = 0;
        double bestDist = 1e18, secondDist = 1e18;
    };

    CrustField BuildCrustField(const PlanetBuild& build, std::uint32_t seed);

    std::vector<GenerationDetail::TectonicPlate> BuildPlates(
        std::uint32_t seed, int numPlates, double heatScale, const CrustField& crust);

    std::vector<CellAssignment> AssignCellsToPlates(
        const PlanetBuild& build, const std::vector<GenerationDetail::TectonicPlate>& plates);

    // Pass 1: plate ids, boundary classification and context, interior crust properties.
    // Returns the cells that are oceanic spreading boundaries.
    std::vector<bool> ClassifyBoundaries(
        PlanetBuild& build, std::uint32_t seed,
        const std::vector<GenerationDetail::TectonicPlate>& plates,
        const std::vector<CellAssignment>& assignments, const CrustField& crust);

    // Pass 2: oceanic crustAge from the distance to the nearest spreading boundary.
    void AssignOceanicCrustAge(
        PlanetBuild& build, const std::vector<CellAssignment>& assignments,
        const CrustField& crust, const std::vector<bool>& isDivergentBoundary);

    // Blurs the signed relief field into broad mountain belts and trenches.
    void SmoothTectonicRelief(PlanetBuild& build);

    // Absolute crustAgeMyr for every cell.
    void AssignAbsoluteCrustAge(PlanetBuild& build, const CrustField& crust);
}

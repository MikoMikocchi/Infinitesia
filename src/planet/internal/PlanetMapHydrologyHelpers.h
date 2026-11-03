#pragma once

#include <vector>

namespace Infinitesia::GenerationDetail
{
    struct DrainageNetwork
    {
        std::vector<double> filledElevations;
        std::vector<int> flowDirections;
        std::vector<double> normalizedAccumulation;
        std::vector<double> volumeAccumulation;
        bool usedLandFallback = false;
    };

    DrainageNetwork BuildDrainageNetwork(
        int latitudeBands,
        int longitudeSegments,
        const std::vector<double>& elevations,
        const std::vector<bool>& oceanCells,
        const std::vector<double>& localNormalizedRunoff,
        const std::vector<double>& localRunoffVolume);
}

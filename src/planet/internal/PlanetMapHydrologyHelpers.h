#pragma once

#include <vector>

namespace Infinitesia::GenerationDetail
{
    struct DrainageNetwork
    {
        std::vector<double> filledElevations;
        std::vector<int> flowDirections;
        // Cell indices sorted from highest to lowest filled elevation, so every
        // cell comes after all of its upstream contributors.
        std::vector<int> processingOrder;
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

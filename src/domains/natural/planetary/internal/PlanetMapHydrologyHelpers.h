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
        // Total runoff volume (same units as localRunoffVolume) delivered into
        // ocean cells, and total volume that terminates in land cells with no
        // downstream neighbour (only the global minimum of an ocean-free map).
        // Water is conserved: oceanInflowVolume + landSinkVolume equals the sum
        // of localRunoffVolume over all land cells.
        double oceanInflowVolume = 0.0;
        double landSinkVolume = 0.0;
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

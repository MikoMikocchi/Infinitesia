#pragma once

#include <vector>

namespace Infinitesia::GenerationDetail
{
    // Distance in km from every cell of a latitude/longitude grid to the nearest source
    // cell, travelling cell to cell over the 8-neighbourhood (longitude wraps) with each
    // step as the great-circle distance between cell centres. O(N log N), deterministic.
    // Cells that no source can reach (there are no sources) get infinity.
    std::vector<double> DistanceToSourcesKm(
        const std::vector<bool>& isSource,
        int latitudeBands,
        int longitudeSegments,
        double radiusKm);
}

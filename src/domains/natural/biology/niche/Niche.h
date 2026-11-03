#pragma once

namespace Infinitesia
{
    // A tolerance along one environmental axis (docs/07_BIOLOGY.md, section 5.1): zero below `lowest`
    // and above `highest`, one between `optimumFrom` and `optimumTo`, linear in between.
    struct Trapezoid
    {
        double lowest = 0.0;
        double optimumFrom = 0.0;
        double optimumTo = 0.0;
        double highest = 0.0;
    };

    // The conditions a species grows in. Values come from the planet's climate and soil fields and from
    // the local relief; `maxDrySeasonMonths` and `maxSlope` are plain limits, the rest are tolerances.
    struct Niche
    {
        Trapezoid temperatureCelsius;
        Trapezoid precipitationMm;
        Trapezoid elevationMeters;
        double maxDrySeasonMonths = 12.0;
        double minSoilFertility = 0.0;  // 0..1
        double minSoilDepth = 0.0;      // 0..1, as PlanetRegion::soilDepthNorm
        double maxSlope = 1.0;          // metres of rise per metre
    };
}

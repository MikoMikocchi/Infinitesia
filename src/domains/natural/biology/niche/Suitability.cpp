#include "domains/natural/biology/niche/Suitability.h"

#include <algorithm>

namespace Infinitesia
{
    namespace
    {
        // 1 at `full` and beyond, 0 at `none` and beyond, linear between (either order).
        double Ramp(double value, double none, double full) noexcept
        {
            return std::clamp((value - none) / (full - none), 0.0, 1.0);
        }
    }

    double Tolerance(const Trapezoid& trapezoid, double value) noexcept
    {
        if (value <= trapezoid.lowest || value >= trapezoid.highest)
            return 0.0;
        if (value < trapezoid.optimumFrom)
            return Ramp(value, trapezoid.lowest, trapezoid.optimumFrom);
        if (value > trapezoid.optimumTo)
            return Ramp(value, trapezoid.highest, trapezoid.optimumTo);
        return 1.0;
    }

    double Suitability(const Niche& niche, const Conditions& conditions) noexcept
    {
        double result = Tolerance(niche.temperatureCelsius, conditions.temperatureCelsius);
        if (result == 0.0)
            return 0.0;
        result *= Tolerance(niche.precipitationMm, conditions.precipitationMm);
        result *= Tolerance(niche.elevationMeters, conditions.elevationMeters);
        result *= Ramp(conditions.drySeasonMonths, niche.maxDrySeasonMonths + 1.0, niche.maxDrySeasonMonths);
        if (niche.minSoilFertility > 0.0)
            result *= Ramp(conditions.soilFertility, 0.0, niche.minSoilFertility);
        if (niche.minSoilDepth > 0.0)
            result *= Ramp(conditions.soilDepth, 0.0, niche.minSoilDepth);
        result *= Ramp(conditions.slope, niche.maxSlope, niche.maxSlope * 0.6);
        return result;
    }
}

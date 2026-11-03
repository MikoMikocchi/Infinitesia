#pragma once

#include "domains/natural/biology/niche/Niche.h"

namespace Infinitesia
{
    // The environment at one point, in the units of the Niche axes.
    struct Conditions
    {
        double temperatureCelsius = 0.0;
        double precipitationMm = 0.0;
        double elevationMeters = 0.0;
        double drySeasonMonths = 0.0;
        double soilFertility = 0.0; // 0..1
        double soilDepth = 0.0;     // 0..1
        double slope = 0.0;         // metres of rise per metre
    };

    // How well one Trapezoid tolerates `value`: 0 outside, 1 on the optimum, linear between.
    double Tolerance(const Trapezoid& trapezoid, double value) noexcept;

    // How well a niche suits the conditions, 0..1 (docs/07_BIOLOGY.md, section 5.1): the product of the
    // tolerances of every axis, so one axis at zero rules the species out. Pure and free of randomness.
    //   temperature, precipitation, elevation  Tolerance of their trapezoids
    //   dry season     1 up to the maximum, falling to 0 one month beyond it
    //   soil fertility and depth  1 from the minimum up, falling to 0 as the value goes to 0
    //   slope          1 up to 60% of the maximum, falling to 0 at the maximum
    double Suitability(const Niche& niche, const Conditions& conditions) noexcept;
}

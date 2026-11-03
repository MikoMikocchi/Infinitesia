#pragma once

#include "domains/natural/biology/niche/Suitability.h"

namespace Infinitesia
{
    // The share of the ground a forest could shade under the conditions, 0..1 (docs/07_BIOLOGY.md,
    // section 5.2): 0 on bare ground (desert, tundra, ice), 1 in a closed forest.
    //
    // It follows the Miami model of net primary production (the lesser of a temperature and a
    // precipitation limit, grams of dry matter per square metre and year) scaled so that 1500 g
    // is a closed canopy, then cut by a long dry season and by thin or poor soil. Pure and repeatable.
    double CanopyCover(const Conditions& conditions) noexcept;
}

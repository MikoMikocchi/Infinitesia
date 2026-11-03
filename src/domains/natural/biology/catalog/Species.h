#pragma once

#include "domains/natural/biology/niche/Niche.h"

#include <string>

namespace Infinitesia
{
    // The generator that grows a plant of the species (docs/07_BIOLOGY.md, section 4).
    enum class PlantForm : unsigned char { Broadleaf, Conifer, Palm, Shrub };

    enum class CrownShape : unsigned char { Sphere, Cone, Umbrella, Column };

    struct IntRange
    {
        int min = 0;
        int max = 0;
    };

    struct DoubleRange
    {
        double min = 0.0;
        double max = 0.0;
    };

    // Materials of the parts of a plant, as catalog keys. They are resolved against the material
    // catalog when the plant materials exist (docs/07_BIOLOGY.md, step B1).
    struct PlantMaterialKeys
    {
        std::string trunk;
        std::string branch;
        std::string twig;
        std::string leaves;
        std::string root;
    };

    struct Species
    {
        std::string key;   // "species.oak"
        std::string name;  // English only
        PlantForm form = PlantForm::Broadleaf;
        Niche niche;

        IntRange heightMeters;           // of a grown plant
        DoubleRange trunkRadiusMeters;   // at the base
        CrownShape crownShape = CrownShape::Sphere;
        DoubleRange crownRadiusMeters;
        double crownStartFraction = 0.5; // share of the height where the crown begins
        IntRange branchCount;
        double branchAngleDegrees = 45.0;
        int rootDepthMeters = 1;
        double shadeTolerance = 0.5;     // 0..1: can it grow under a canopy

        PlantMaterialKeys materials;
        double logsPerCubicMeter = 1.0;
    };
}

#pragma once

#include "materials/MaterialId.h"
#include "domains/natural/planetary/geology/Province.h"

#include <cstdint>
#include <vector>

namespace Infinitesia
{
    // Biosphere conditions a layer of a stratigraphic column depends on.
    enum class BiosphereRequirement : std::uint8_t
    {
        None,
        FreeOxygen,
        BandedIronFormations,
        LandPlants,
        OrganicRichMarineSediments,
        CalcareousPlankton,
    };

    struct ColumnLayer
    {
        MaterialId rock = 0;
        double thicknessMeters = 0.0; // real-world thickness before the game scale
        BiosphereRequirement needs = BiosphereRequirement::None;
    };

    // Faults of a province type: a candidate fault per lattice cell of `spacingMeters`, present
    // with the given probability; the rest of the kinds up to 1 are strike-slip.
    struct FaultStyle
    {
        double spacingMeters = 1000.0;
        double probability = 0.0;
        double minSlipMeters = 0.0; // displacement along the fault plane at the centre
        double maxSlipMeters = 0.0;
        double normalShare = 0.0;
        double thrustShare = 0.0;
    };

    // How the rocks of a province type are deformed (docs/03_GEOLOGY.md, section 8.1).
    struct StructureStyle
    {
        FaultStyle faults;
        double minDipDegrees = 0.0;
        double maxDipDegrees = 0.0;
        // Fold amplitude is foldAmplitude * wavelength^0.8, limited to a 60 degree flank.
        double foldAmplitude = 0.0;
        int foldCount = 0;
        // Lateral change of a layer's thickness: thickness * (1 + variability * noise), noise in
        // [-1, 1]. Above 1 the layer pinches out in places and forms lenses.
        double minVariability = 0.0;
        double maxVariability = 0.0;
    };

    // The stratigraphic pile typical for a province type, basement first, and how it is deformed.
    struct ProvinceColumn
    {
        ProvinceType type = ProvinceType::AbyssalPlain;
        StructureStyle style;
        std::vector<ColumnLayer> bottomUp;
    };
}

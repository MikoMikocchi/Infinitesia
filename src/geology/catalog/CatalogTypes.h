#pragma once

#include "chemistry/Composition.h"
#include "geology/catalog/MaterialId.h"
#include "planet/geology/Province.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Infinitesia
{
    struct RgbColor
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;

        friend bool operator==(const RgbColor&, const RgbColor&) = default;
    };

    // Where a value comes from (docs/03_GEOLOGY.md, section 0): a reference work, a
    // derivation from other data, or a game assumption.
    enum class Provenance : std::uint8_t { Reference, Derived, Gameplay };

    // What a block of this material is for gameplay and rendering.
    enum class BlockClass : std::uint8_t { Air, Fluid, Loose, Rock, Ice };

    enum class RockClass : std::uint8_t
    {
        IgneousIntrusive,
        IgneousExtrusive,
        Volcaniclastic,
        SedimentaryClastic,
        SedimentaryChemical,
        SedimentaryBiogenic,
        Metamorphic,
    };

    struct Mineral
    {
        std::string id;          // "mineral.quartz"
        int numericId = 0;       // stable, never reused
        std::string name;
        std::string formula;     // as written in the catalog
        Chemistry::Composition composition;
        double molarMass = 0.0;  // g/mol, from the composition
        double density = 0.0;    // g/cm3
        double mohs = 0.0;
        RgbColor color;
        Provenance provenance = Provenance::Gameplay;
        bool solidSolution = false; // the formula is a typical member of a series
    };

    // Share of one mineral in a rock, by volume.
    struct ModalEntry
    {
        int mineralIndex = 0;
        double fraction = 0.0;
    };

    struct Rock
    {
        std::string id;          // "rock.granite"
        int numericId = 0;       // also its MaterialId
        std::string name;
        std::string nameRu;
        RockClass rockClass = RockClass::IgneousIntrusive;
        double density = 0.0;    // kg/m3, typical bulk
        int digClass = 3;        // 1 soft ground .. 5 very hard
        RgbColor color;
        Provenance provenance = Provenance::Gameplay;
        std::vector<ModalEntry> modal; // empty when the composition is not filled in yet
        MaterialId material = 0;
    };

    // Everything a block can be made of: air, fluids, rocks and loose ground.
    struct MaterialDef
    {
        MaterialId id = 0;
        std::string key;         // "rock.granite", "soil.sand", "fluid.water"
        std::string name;
        std::string nameRu;
        BlockClass blockClass = BlockClass::Air;
        double density = 0.0;    // kg/m3
        int digClass = 0;
        RgbColor color;
        int rockIndex = -1;      // index into the rock list for rocks, else -1
    };

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

    // The stratigraphic pile typical for a province type, basement first.
    struct ProvinceColumn
    {
        ProvinceType type = ProvinceType::AbyssalPlain;
        std::vector<ColumnLayer> bottomUp;
    };
}

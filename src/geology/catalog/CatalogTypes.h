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

    // Block light is coloured: one 0-15 level per channel. The same shape holds how much a material
    // dims each channel of the light that passes through it (docs/06_BLOCKS.md, section 3.2).
    struct LightRgb
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;

        friend bool operator==(const LightRgb&, const LightRgb&) = default;
    };

    inline constexpr std::uint8_t kMaxLightLevel = 15;

    // How much a material of the class dims light when the data does not say: air passes it,
    // water and ice dim it a little, ground and rock stop it.
    inline constexpr LightRgb DefaultOpacity(BlockClass blockClass)
    {
        switch (blockClass)
        {
        case BlockClass::Air:   return { 0, 0, 0 };
        case BlockClass::Fluid: return { 2, 2, 2 };
        case BlockClass::Ice:   return { 3, 3, 3 };
        case BlockClass::Loose:
        case BlockClass::Rock:  break;
        }
        return { kMaxLightLevel, kMaxLightLevel, kMaxLightLevel };
    }

    // Tag every material of the class carries besides the ones written in the data.
    inline constexpr const char* ClassTag(BlockClass blockClass)
    {
        switch (blockClass)
        {
        case BlockClass::Air:   return "air";
        case BlockClass::Fluid: return "fluid";
        case BlockClass::Loose: return "loose";
        case BlockClass::Rock:  return "rock";
        case BlockClass::Ice:   return "ice";
        }
        return "";
    }

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
        // Tags group materials so rules ask "is it water" instead of naming ids; the class tag
        // (ClassTag) is always present.
        std::vector<std::string> tags;
        LightRgb opacity{ kMaxLightLevel, kMaxLightLevel, kMaxLightLevel }; // dimming per channel
        LightRgb emission;       // light the material gives off, per channel
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

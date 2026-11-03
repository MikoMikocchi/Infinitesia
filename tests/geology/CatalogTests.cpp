#include <doctest/doctest.h>

#include "support/GeologyFixtures.h"
#include "support/Require.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <utility>

using namespace Infinitesia;
using namespace InfinitesiaTests;

TEST_CASE("CatalogLoadsWithStableIds")
{
    const GeologyCatalog& catalog = SharedCatalog();
    CHECK(catalog.Minerals().size() >= 160);
    CHECK(catalog.Rocks().size() >= 95);
    CHECK(catalog.Materials().size() == catalog.Rocks().size() + 21);

    // Numeric IDs are saved with worlds: these never change.
    CHECK(catalog.FindMineral("mineral.quartz")->numericId == 1);
    CHECK(RockNamed("rock.granite").numericId == 32);
    CHECK(RockNamed("rock.granite").material == 32);
    CHECK(catalog.FindMaterial("fluid.water")->id == kWaterMaterial);
    CHECK(catalog.FindMaterial("air")->id == kAirMaterial);
    CHECK(catalog.FindMaterial("soil.humus")->id == kFirstLooseMaterial);

    std::set<std::string> ids;
    std::set<int> mineralNumbers;
    for (const Mineral& mineral : catalog.Minerals())
    {
        Require(ids.insert(mineral.id).second, "duplicate mineral id");
        Require(mineralNumbers.insert(mineral.numericId).second, "duplicate mineral numeric id");
    }
    std::set<MaterialId> materialIds;
    for (const MaterialDef& material : catalog.Materials())
    {
        Require(materialIds.insert(material.id).second, "duplicate material id");
        Require(catalog.GetMaterial(material.id) == &material, "material lookup by id is wrong");
        Require(catalog.FindMaterial(material.key) == &material, "material lookup by key is wrong");
        switch (material.blockClass)
        {
        case BlockClass::Air:
        case BlockClass::Fluid: Require(material.id < kFirstRockMaterial, "air and fluids live below 32"); break;
        case BlockClass::Rock: Require(material.id >= kFirstRockMaterial && material.id < kFirstLooseMaterial, "rocks live in 32-511"); break;
        case BlockClass::Loose:
        case BlockClass::Ice: Require(material.id >= kFirstLooseMaterial && material.id < kFirstReservedMaterial, "loose ground lives in 512-1023"); break;
        }
    }
    CHECK(catalog.GetMaterial(kUnspecifiedMaterial) == nullptr);
    CHECK(catalog.GetMaterial(31) == nullptr);
    CHECK(catalog.ColorOf(kUnspecifiedMaterial) == RgbColor{ 255, 0, 255 });
}

TEST_CASE("MaterialsCarryTagsAndLightProperties")
{
    const GeologyCatalog& catalog = SharedCatalog();
    const auto hasTag = [](const MaterialDef& material, const char* tag)
    {
        return std::find(material.tags.begin(), material.tags.end(), tag) != material.tags.end();
    };
    for (const MaterialDef& material : catalog.Materials())
    {
        Require(hasTag(material, ClassTag(material.blockClass)), "a material lacks the tag of its class");
        for (const std::uint8_t level : { material.opacity.r, material.opacity.g, material.opacity.b,
                                          material.emission.r, material.emission.g, material.emission.b })
        {
            Require(level <= kMaxLightLevel, "a light value is above 15");
        }
    }

    // Defaults follow the class: air passes light, water and ice dim it a little, ground and rock stop it.
    CHECK(catalog.FindMaterial("air")->opacity == LightRgb{ 0, 0, 0 });
    CHECK(catalog.FindMaterial("fluid.water")->opacity == LightRgb{ 2, 2, 2 });
    CHECK(catalog.FindMaterial("ice.glacier")->opacity == LightRgb{ 3, 3, 3 });
    CHECK(catalog.FindMaterial("soil.sand")->opacity == LightRgb{ 15, 15, 15 });
    CHECK(catalog.FindMaterial("rock.granite")->opacity == LightRgb{ 15, 15, 15 });

    // Magma gives off orange light and blocks it like rock; nothing else glows yet.
    CHECK(catalog.FindMaterial("fluid.magma")->emission == LightRgb{ 14, 6, 1 });
    CHECK(catalog.FindMaterial("fluid.magma")->opacity == LightRgb{ 15, 15, 15 });
    CHECK(catalog.FindMaterial("fluid.oil")->emission == LightRgb{});

    // The sea is the same as fresh water for now (docs/06_BLOCKS.md): one tag, equal properties.
    const MaterialDef& water = *catalog.FindMaterial("fluid.water");
    const MaterialDef& seawater = *catalog.FindMaterial("fluid.seawater");
    CHECK(hasTag(water, "water"));
    CHECK(hasTag(seawater, "water"));
    CHECK(water.density == seawater.density);
    CHECK(water.color == seawater.color);
    CHECK(water.opacity == seawater.opacity);
}

TEST_CASE("MaterialLightFieldsAreValidated")
{
    {
        const BrokenCatalog broken("infinitesia_catalog_opacity");
        broken.Replace("materials.json", "\"opacity\": 15", "\"opacity\": 16");
        const std::string error = broken.LoadError();
        CHECK(error.find("opacity") != std::string::npos);
        CHECK(error.find("fluid.magma") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_emission");
        broken.Replace("materials.json", "\"emission\": [14, 6, 1]", "\"emission\": [14, 6]");
        CHECK(broken.LoadError().find("emission") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_tags");
        broken.Replace("materials.json", "\"tags\": [\"water\"]", "\"tags\": [7]");
        CHECK(broken.LoadError().find("tags") != std::string::npos);
    }
    {
        // A tint is three numbers: red glass dims green and blue more than red.
        const BrokenCatalog broken("infinitesia_catalog_tint");
        broken.Replace("materials.json", "\"opacity\": 15", "\"opacity\": [1, 8, 8]");
        CHECK(broken.LoadError().empty());
    }
}

TEST_CASE("MineralMolarMassesMatchTheReference")
{
    // Reference molar masses (g/mol) of well known minerals; the catalog computes its own
    // from the formula and must agree within 0.1% (docs/03_GEOLOGY.md, section 16).
    const std::pair<const char*, double> reference[] = {
        { "mineral.quartz", 60.084 },       { "mineral.calcite", 100.087 },    { "mineral.hematite", 159.688 },
        { "mineral.magnetite", 231.533 },   { "mineral.pyrite", 119.98 },      { "mineral.halite", 58.443 },
        { "mineral.gypsum", 172.168 },      { "mineral.galena", 239.27 },      { "mineral.chalcopyrite", 183.54 },
        { "mineral.corundum", 101.961 },    { "mineral.fluorite", 78.075 },    { "mineral.graphite", 12.011 },
        { "mineral.native_gold", 196.967 }, { "mineral.kaolinite", 258.16 },   { "mineral.muscovite", 398.31 },
        { "mineral.orthoclase", 278.33 },   { "mineral.albite", 262.22 },      { "mineral.anorthite", 278.21 },
        { "mineral.sphalerite", 97.47 },    { "mineral.cassiterite", 150.71 }, { "mineral.malachite", 221.12 },
        { "mineral.dolomite", 184.40 },     { "mineral.barite", 233.39 },      { "mineral.rutile", 79.866 },
        { "mineral.diopside", 216.55 },     { "mineral.spodumene", 186.09 },   { "mineral.zircon", 183.31 },
    };
    for (const auto& [id, expected] : reference)
    {
        const Mineral* mineral = SharedCatalog().FindMineral(id);
        REQUIRE_MESSAGE(mineral != nullptr, id);
        CHECK_MESSAGE(mineral->molarMass == doctest::Approx(expected).epsilon(0.001), id);
    }
}

TEST_CASE("EveryMineralIsAConsistentCompositionWithPlausibleProperties")
{
    for (const Mineral& mineral : SharedCatalog().Minerals())
    {
        Require(!mineral.composition.Empty(), "mineral has an empty composition");
        Require(mineral.molarMass > 1.0, "mineral molar mass is implausible");
        Require(std::abs(mineral.composition.MassFractions().Sum() - 1.0) < 1e-9, "mineral element fractions do not sum to 1");
        Require(mineral.density >= 0.9 && mineral.density <= 22.0, "mineral density out of range");
        Require(mineral.mohs >= 1.0 && mineral.mohs <= 10.0, "mineral hardness out of range");
        // Atoms of every element are accounted for: no element beyond uranium in a mineral.
        for (const Chemistry::Composition::Entry& entry : mineral.composition.Entries())
        {
            Require(entry.atomicNumber <= 92, "mineral contains a transuranic element");
        }
    }
}

TEST_CASE("RocksAreMixturesOfMinerals")
{
    const GeologyCatalog& catalog = SharedCatalog();
    int filled = 0;
    for (const Rock& rock : catalog.Rocks())
    {
        Require(rock.density >= 700.0 && rock.density <= 4000.0, "rock density out of range");
        if (rock.modal.empty())
            continue;
        ++filled;
        double volume = 0.0;
        double crystallineDensity = 0.0;
        for (const ModalEntry& entry : rock.modal)
        {
            volume += entry.fraction;
            crystallineDensity += entry.fraction * catalog.Minerals()[static_cast<std::size_t>(entry.mineralIndex)].density * 1000.0;
        }
        Require(std::abs(volume - 1.0) < 1e-3, "modal fractions do not sum to 1");

        const Chemistry::ElementMassFractions elements = catalog.RockElementMassFractions(rock);
        Require(std::abs(elements.Sum() - 1.0) < 1e-9, "rock element fractions do not sum to 1");

        // A crystalline rock is as dense as its minerals; sedimentary rocks are porous, so
        // their bulk density can only be lower.
        const bool crystalline = rock.rockClass == RockClass::IgneousIntrusive ||
                                 rock.rockClass == RockClass::IgneousExtrusive ||
                                 rock.rockClass == RockClass::Metamorphic;
        if (crystalline)
        {
            const bool close = std::abs(crystallineDensity - rock.density) / rock.density < 0.12;
            CHECK_MESSAGE(close, rock.id, ": stated ", rock.density, ", minerals give ", crystallineDensity);
        }
        else
        {
            Require(crystallineDensity >= rock.density * 0.95, "a porous rock is denser than its minerals");
        }
    }
    CHECK(filled >= 70);
}

TEST_CASE("RockChemistryFollowsFromItsMinerals")
{
    const GeologyCatalog& catalog = SharedCatalog();
    const auto elements = [&](const char* id) { return catalog.RockElementMassFractions(RockNamed(id)); };

    const auto granite = elements("rock.granite");
    const auto basalt = elements("rock.basalt");
    const auto peridotite = elements("rock.peridotite");
    const auto limestone = elements("rock.limestone");
    const auto quartzite = elements("rock.quartzite");
    const auto rockSalt = elements("rock.rock_salt");

    // Silica-rich to silica-poor, and the mafic elements the other way round.
    CHECK(Fraction(granite, "Si") > Fraction(basalt, "Si"));
    CHECK(Fraction(basalt, "Si") > Fraction(peridotite, "Si"));
    CHECK(Fraction(peridotite, "Mg") > Fraction(basalt, "Mg"));
    CHECK(Fraction(basalt, "Mg") > Fraction(granite, "Mg"));
    CHECK(Fraction(basalt, "Fe") > Fraction(granite, "Fe"));
    CHECK(Fraction(granite, "K") > Fraction(basalt, "K"));

    // Typical absolute values.
    CHECK(Fraction(granite, "Si") == doctest::Approx(0.33).epsilon(0.12));
    CHECK(Fraction(granite, "O") == doctest::Approx(0.48).epsilon(0.08));
    CHECK(Fraction(limestone, "Ca") == doctest::Approx(0.38).epsilon(0.05));
    CHECK(Fraction(quartzite, "Si") == doctest::Approx(0.467).epsilon(0.03));
    CHECK(Fraction(rockSalt, "Na") == doctest::Approx(0.37).epsilon(0.05));
    CHECK(Fraction(rockSalt, "Cl") == doctest::Approx(0.58).epsilon(0.05));

    // A rock without a filled modal composition has no element fractions yet.
    CHECK(catalog.RockElementMassFractions(RockNamed("rock.anthracite")).Empty());
}

TEST_CASE("ProvinceColumnsCoverEveryProvinceTypeWithKnownRocks")
{
    const GeologyCatalog& catalog = SharedCatalog();
    for (int type = 0; type < kProvinceTypeCount; ++type)
    {
        const ProvinceColumn& column = catalog.ColumnFor(static_cast<ProvinceType>(type));
        CHECK_MESSAGE(column.type == static_cast<ProvinceType>(type), ProvinceTypeName(static_cast<ProvinceType>(type)));
        REQUIRE(!column.bottomUp.empty());
        CHECK(column.bottomUp.front().needs == BiosphereRequirement::None);
        for (const ColumnLayer& layer : column.bottomUp)
        {
            Require(catalog.GetRock(layer.rock) != nullptr, "a column layer is not a rock");
            Require(layer.thicknessMeters > 0.0, "a column layer has no thickness");
        }
    }
}

TEST_CASE("LoaderRejectsBrokenCatalogsWithAMessageNamingTheRecord")
{
    {
        const BrokenCatalog broken("infinitesia_catalog_formula");
        broken.Replace("minerals.json", "\"formula\": \"SiO2\"", "\"formula\": \"Si(O2\"");
        const std::string error = broken.LoadError();
        CHECK(error.find("minerals.json") != std::string::npos);
        CHECK(error.find("mineral.quartz") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_duplicate");
        broken.Replace("minerals.json", "\"id\": \"mineral.calcite\"", "\"id\": \"mineral.quartz\"");
        CHECK(broken.LoadError().find("duplicate id") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_numeric");
        broken.Replace("rocks.json", "\"numericId\": 33", "\"numericId\": 32");
        CHECK(broken.LoadError().find("numericId") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_modal");
        broken.Replace("rocks.json", "\"mineral\": \"mineral.orthoclase\"", "\"mineral\": \"mineral.nothing\"");
        const std::string error = broken.LoadError();
        CHECK(error.find("unknown mineral") != std::string::npos);
        CHECK(error.find("rock.granite") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_sum");
        broken.Replace("rocks.json", "\"fraction\": 0.28", "\"fraction\": 0.5");
        CHECK(broken.LoadError().find("sum") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_color");
        broken.Replace("rocks.json", "\"color\": \"#b8a89a\"", "\"color\": \"granite\"");
        CHECK(broken.LoadError().find("colour") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_column");
        broken.Replace("province_columns.json", "\"rock\": \"rock.chert\"", "\"rock\": \"rock.cheese\"");
        CHECK(broken.LoadError().find("unknown rock") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_structure");
        broken.Replace("province_columns.json", "\"foldCount\": 2", "\"foldCount\": 9");
        CHECK(broken.LoadError().find("foldCount") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_json");
        std::ofstream(broken.directory / "materials.json", std::ios::trunc) << "{ \"schemaVersion\": 1, ";
        CHECK(broken.LoadError().find("invalid JSON") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_missing");
        std::filesystem::remove(broken.directory / "rocks.json");
        CHECK(broken.LoadError().find("cannot open") != std::string::npos);
    }
    {
        const BrokenCatalog broken("infinitesia_catalog_version");
        broken.Replace("minerals.json", "\"schemaVersion\": 1", "\"schemaVersion\": 2");
        CHECK(broken.LoadError().find("schemaVersion") != std::string::npos);
    }
}

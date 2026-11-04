#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <set>
#include <string>
#include <utility>

#include "support/BrokenFile.h"
#include "support/MaterialFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

namespace {
bool HasTag(const MaterialDef& material, const char* tag) {
  return std::find(material.tags.begin(), material.tags.end(), tag) !=
         material.tags.end();
}

// The message of the error loading a broken copy of materials.json; empty when
// it loads.
std::string LoadError(const BrokenFile& broken) {
  try {
    MaterialCatalog::LoadFromFile(broken.Path());
  } catch (const CatalogError& error) {
    return error.what();
  }
  return {};
}

std::vector<const MaterialDef*> OfClass(const MaterialCatalog& catalog,
                                        BlockClass blockClass) {
  std::vector<const MaterialDef*> found;
  for (const MaterialDef& material : catalog.Materials()) {
    if (material.blockClass == blockClass) found.push_back(&material);
  }
  return found;
}
}  // namespace

TEST_CASE("CatalogLoadsWithStableIds") {
  const MaterialCatalog& catalog = *SharedMaterials();
  CHECK(OfClass(catalog, BlockClass::Mineral).size() >= 160);
  CHECK(OfClass(catalog, BlockClass::Rock).size() >= 95);
  CHECK(catalog.Materials().size() >= 290);

  // Numeric IDs are saved with worlds: these never change.
  CHECK(catalog.Find("air")->id == kAirMaterial);
  CHECK(catalog.Find("fluid.water")->id == kWaterMaterial);
  CHECK(catalog.Find("rock.granite")->id == 32);
  CHECK(catalog.Find("rock.bedrock")->id == 130);
  CHECK(catalog.Find("soil.humus")->id == kFirstLooseMaterial);
  CHECK(catalog.Find("soil.turf")->id == 527);
  CHECK(catalog.Find("built.planks")->id == kFirstBuiltMaterial);
  CHECK(catalog.Find("mineral.quartz")->id == kFirstMineralMaterial);

  std::set<MaterialId> ids;
  std::set<std::string> keys;
  for (const MaterialDef& material : catalog.Materials()) {
    Require(ids.insert(material.id).second, "duplicate material id");
    Require(keys.insert(material.key).second, "duplicate material key");
    Require(catalog.Get(material.id) == &material,
            "material lookup by id is wrong");
    Require(catalog.Find(material.key) == &material,
            "material lookup by key is wrong");
    switch (material.blockClass) {
      case BlockClass::Air:
      case BlockClass::Fluid:
        Require(material.id < kFirstRockMaterial,
                "air and fluids live below 32");
        break;
      case BlockClass::Rock:
        Require(material.id >= kFirstRockMaterial &&
                    material.id < kFirstLooseMaterial,
                "rocks live in 32-511");
        break;
      case BlockClass::Loose:
      case BlockClass::Ice:
        Require(material.id >= kFirstLooseMaterial &&
                    material.id < kFirstBuiltMaterial,
                "loose ground lives in 512-1023");
        break;
      case BlockClass::Built:
        Require(material.id >= kFirstBuiltMaterial &&
                    material.id < kFirstMineralMaterial,
                "built materials live in 1024-2047");
        break;
      case BlockClass::Mineral:
        Require(material.id >= kFirstMineralMaterial &&
                    material.id < kFirstPlantMaterial,
                "minerals live in 2048-4095");
        break;
      case BlockClass::Plant:
        Require(material.id >= kFirstPlantMaterial &&
                    material.id < kMaterialCapacity,
                "plants live in 4096-5119");
        break;
    }
  }
  CHECK(catalog.Get(kUnspecifiedMaterial) == nullptr);
  CHECK(catalog.Get(31) == nullptr);
  CHECK(catalog.ColorOf(kUnspecifiedMaterial) == RgbColor{255, 0, 255});
}

TEST_CASE("AMineralIsAMaterialLikeAnyOther") {
  const MaterialCatalog& catalog = *SharedMaterials();
  const MaterialDef& quartz = *catalog.Find("mineral.quartz");

  // The same record as granite or planks: density, colour, tags, light and
  // rules.
  CHECK(quartz.blockClass == BlockClass::Mineral);
  CHECK(HasTag(quartz, "mineral"));
  CHECK(quartz.density == doctest::Approx(2650.0));
  CHECK(quartz.opacity == LightRgb{15, 15, 15});
  CHECK(quartz.walkable);
  CHECK(quartz.diggable);
  CHECK(quartz.digClass >= 1);

  // What only some materials have lives in side tables, found by the same id.
  CHECK(catalog.ChemistryOf(quartz.id) != nullptr);
  CHECK(catalog.ModalOf(quartz.id) == nullptr);
  CHECK(catalog.ModalOf(catalog.Find("rock.granite")->id) != nullptr);
  CHECK(catalog.ChemistryOf(catalog.Find("rock.granite")->id) == nullptr);
  for (const char* plain :
       {"air", "fluid.water", "soil.sand", "ice.glacier", "built.planks"}) {
    CHECK_MESSAGE(catalog.ChemistryOf(catalog.Find(plain)->id) == nullptr,
                  plain);
    CHECK_MESSAGE(catalog.ModalOf(catalog.Find(plain)->id) == nullptr, plain);
  }

  // A mineral has the element fractions of its formula; a fluid has none.
  const auto quartzElements = catalog.ElementMassFractionsOf(quartz.id);
  CHECK(Fraction(quartzElements, "Si") ==
        doctest::Approx(0.4674).epsilon(0.002));
  CHECK(Fraction(quartzElements, "O") ==
        doctest::Approx(0.5326).epsilon(0.002));
  CHECK(
      catalog.ElementMassFractionsOf(catalog.Find("fluid.water")->id).Empty());
}

TEST_CASE("MaterialsCarryTagsAndLightProperties") {
  const MaterialCatalog& catalog = *SharedMaterials();
  for (const MaterialDef& material : catalog.Materials()) {
    Require(HasTag(material, ClassTag(material.blockClass)),
            "a material lacks the tag of its class");
    for (const std::uint8_t level :
         {material.opacity.r, material.opacity.g, material.opacity.b,
          material.emission.r, material.emission.g, material.emission.b}) {
      Require(level <= kMaxLightLevel, "a light value is above 15");
    }
  }

  // Defaults follow the class: air passes light, water and ice dim it a little,
  // ground and rock stop it.
  CHECK(catalog.Find("air")->opacity == LightRgb{0, 0, 0});
  CHECK(catalog.Find("fluid.water")->opacity == LightRgb{2, 2, 2});
  CHECK(catalog.Find("ice.glacier")->opacity == LightRgb{3, 3, 3});
  CHECK(catalog.Find("soil.sand")->opacity == LightRgb{15, 15, 15});
  CHECK(catalog.Find("rock.granite")->opacity == LightRgb{15, 15, 15});

  // Magma gives off orange light and blocks it like rock; the lamp glows
  // white-yellow.
  CHECK(catalog.Find("fluid.magma")->emission == LightRgb{14, 6, 1});
  CHECK(catalog.Find("fluid.magma")->opacity == LightRgb{15, 15, 15});
  CHECK(catalog.Find("fluid.oil")->emission == LightRgb{});
  CHECK(catalog.Find("built.lamp")->emission != LightRgb{});
  CHECK(catalog.Find("built.glass")->opacity == LightRgb{0, 0, 0});
  CHECK(catalog.Find("built.glass_red")->opacity.r <
        catalog.Find("built.glass_red")->opacity.g);

  // Grass-covered ground and the stand-in rock for terrains without geology
  // (docs/06_BLOCKS.md, section 4).
  const MaterialDef& turf = *catalog.Find("soil.turf");
  CHECK(turf.blockClass == BlockClass::Loose);
  CHECK(HasTag(turf, "soil"));
  CHECK(turf.digClass == 1);
  const MaterialDef& bedrock = *catalog.Find("rock.bedrock");
  CHECK(bedrock.blockClass == BlockClass::Rock);
  CHECK(bedrock.digClass == 4);
  CHECK(catalog.ModalOf(bedrock.id)->parts.empty());
  CHECK(catalog.Find("soil.sand")->tags.size() == 2);

  // The sea is the same as fresh water for now (docs/06_BLOCKS.md): one tag,
  // equal properties.
  const MaterialDef& water = *catalog.Find("fluid.water");
  const MaterialDef& seawater = *catalog.Find("fluid.seawater");
  CHECK(HasTag(water, "water"));
  CHECK(HasTag(seawater, "water"));
  CHECK(water.density == seawater.density);
  CHECK(water.color == seawater.color);
  CHECK(water.opacity == seawater.opacity);
}

TEST_CASE("MaterialLightFieldsAreValidated") {
  {
    const BrokenFile broken(kMaterialsFile, "infinitesia_materials_opacity");
    broken.Edit([](Json& root) {
      BrokenFile::Record(root, "materials", "fluid.magma")["opacity"] = 16;
    });
    const std::string error = LoadError(broken);
    CHECK(error.find("opacity") != std::string::npos);
    CHECK(error.find("fluid.magma") != std::string::npos);
  }
  {
    const BrokenFile broken(kMaterialsFile, "infinitesia_materials_emission");
    broken.Edit([](Json& root) {
      BrokenFile::Record(root, "materials", "fluid.magma")["emission"] =
          Json::array({14, 6});
    });
    CHECK(LoadError(broken).find("emission") != std::string::npos);
  }
  {
    const BrokenFile broken(kMaterialsFile, "infinitesia_materials_tags");
    broken.Edit([](Json& root) {
      BrokenFile::Record(root, "materials", "fluid.water")["tags"] =
          Json::array({7});
    });
    CHECK(LoadError(broken).find("tags") != std::string::npos);
  }
  {
    // A tint is three numbers: red glass dims green and blue more than red.
    const BrokenFile broken(kMaterialsFile, "infinitesia_materials_tint");
    broken.Edit([](Json& root) {
      BrokenFile::Record(root, "materials", "fluid.magma")["opacity"] =
          Json::array({1, 8, 8});
    });
    CHECK(LoadError(broken).empty());
  }
}

TEST_CASE("MineralMolarMassesMatchTheReference") {
  // Reference molar masses (g/mol) of well known minerals; the catalog computes
  // its own from the formula and must agree within 0.1% (docs/03_GEOLOGY.md,
  // section 16).
  const std::pair<const char*, double> reference[] = {
      {"mineral.quartz", 60.084},       {"mineral.calcite", 100.087},
      {"mineral.hematite", 159.688},    {"mineral.magnetite", 231.533},
      {"mineral.pyrite", 119.98},       {"mineral.halite", 58.443},
      {"mineral.gypsum", 172.168},      {"mineral.galena", 239.27},
      {"mineral.chalcopyrite", 183.54}, {"mineral.corundum", 101.961},
      {"mineral.fluorite", 78.075},     {"mineral.graphite", 12.011},
      {"mineral.native_gold", 196.967}, {"mineral.kaolinite", 258.16},
      {"mineral.muscovite", 398.31},    {"mineral.orthoclase", 278.33},
      {"mineral.albite", 262.22},       {"mineral.anorthite", 278.21},
      {"mineral.sphalerite", 97.47},    {"mineral.cassiterite", 150.71},
      {"mineral.malachite", 221.12},    {"mineral.dolomite", 184.40},
      {"mineral.barite", 233.39},       {"mineral.rutile", 79.866},
      {"mineral.diopside", 216.55},     {"mineral.spodumene", 186.09},
      {"mineral.zircon", 183.31},
  };
  for (const auto& [key, expected] : reference) {
    const MaterialDef& mineral = MaterialNamed(key);
    const ChemistryInfo* chemistry = SharedMaterials()->ChemistryOf(mineral.id);
    REQUIRE_MESSAGE(chemistry != nullptr, key);
    CHECK_MESSAGE(
        chemistry->molarMass == doctest::Approx(expected).epsilon(0.001), key);
  }
}

TEST_CASE("EveryMineralIsAConsistentCompositionWithPlausibleProperties") {
  const MaterialCatalog& catalog = *SharedMaterials();
  for (const MaterialDef* mineral : OfClass(catalog, BlockClass::Mineral)) {
    const ChemistryInfo* chemistry = catalog.ChemistryOf(mineral->id);
    RequireFatal(chemistry != nullptr, "a mineral has no chemistry");
    Require(!chemistry->composition.Empty(),
            "mineral has an empty composition");
    Require(chemistry->molarMass > 1.0, "mineral molar mass is implausible");
    Require(std::abs(chemistry->composition.MassFractions().Sum() - 1.0) < 1e-9,
            "mineral element fractions do not sum to 1");
    Require(mineral->density >= 900.0 && mineral->density <= 22000.0,
            "mineral density out of range");
    Require(chemistry->mohs >= 1.0 && chemistry->mohs <= 10.0,
            "mineral hardness out of range");
    Require(mineral->digClass >= 1 && mineral->digClass <= 5,
            "mineral dig class out of range");
    // Atoms of every element are accounted for: no element beyond uranium in a
    // mineral.
    for (const Chemistry::Composition::Entry& entry :
         chemistry->composition.Entries()) {
      Require(entry.atomicNumber <= 92,
              "mineral contains a transuranic element");
    }
  }
}

TEST_CASE("RocksAreMixturesOfMinerals") {
  const MaterialCatalog& catalog = *SharedMaterials();
  int filled = 0;
  for (const MaterialDef* rock : OfClass(catalog, BlockClass::Rock)) {
    Require(rock->density >= 700.0 && rock->density <= 4000.0,
            "rock density out of range");
    const ModalComposition* modal = catalog.ModalOf(rock->id);
    RequireFatal(modal != nullptr, "a rock has no modal composition record");
    if (modal->parts.empty()) continue;
    ++filled;
    double volume = 0.0;
    double crystallineDensity = 0.0;
    for (const ModalEntry& entry : modal->parts) {
      volume += entry.fraction;
      crystallineDensity +=
          entry.fraction * catalog.Get(entry.mineral)->density;
    }
    Require(std::abs(volume - 1.0) < 1e-3, "modal fractions do not sum to 1");

    const Chemistry::ElementMassFractions elements =
        catalog.ElementMassFractionsOf(rock->id);
    Require(std::abs(elements.Sum() - 1.0) < 1e-9,
            "rock element fractions do not sum to 1");

    // A crystalline rock is as dense as its minerals; sedimentary rocks are
    // porous, so their bulk density can only be lower.
    const bool crystalline = modal->rockClass == RockClass::IgneousIntrusive ||
                             modal->rockClass == RockClass::IgneousExtrusive ||
                             modal->rockClass == RockClass::Metamorphic;
    if (crystalline) {
      const bool close =
          std::abs(crystallineDensity - rock->density) / rock->density < 0.12;
      CHECK_MESSAGE(close, rock->key, ": stated ", rock->density,
                    ", minerals give ", crystallineDensity);
    } else {
      Require(crystallineDensity >= rock->density * 0.95,
              "a porous rock is denser than its minerals");
    }
  }
  CHECK(filled >= 70);
}

TEST_CASE("RockChemistryFollowsFromItsMinerals") {
  const MaterialCatalog& catalog = *SharedMaterials();
  const auto elements = [&](const char* key) {
    return catalog.ElementMassFractionsOf(MaterialNamed(key).id);
  };

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
  CHECK(elements("rock.anthracite").Empty());
}

TEST_CASE("LoaderRejectsBrokenFilesWithAMessageNamingTheRecord") {
  const auto check = [](const char* name,
                        const std::function<void(Json&)>& change,
                        const char* inError, const char* inRecord) {
    const BrokenFile broken(kMaterialsFile, name);
    broken.Edit(change);
    const std::string error = LoadError(broken);
    CHECK_MESSAGE(error.find(inError) != std::string::npos, name, ": ", error);
    if (inRecord != nullptr)
      CHECK_MESSAGE(error.find(inRecord) != std::string::npos, name, ": ",
                    error);
  };
  const auto record = [](Json& root, const char* key) -> Json& {
    return BrokenFile::Record(root, "materials", key);
  };

  check(
      "infinitesia_mat_formula",
      [&](Json& r) {
        record(r, "mineral.quartz")["chemistry"]["formula"] = "Si(O2";
      },
      "materials.json", "mineral.quartz");
  check(
      "infinitesia_mat_duplicate",
      [&](Json& r) { record(r, "mineral.calcite")["id"] = "mineral.quartz"; },
      "duplicate id", nullptr);
  check(
      "infinitesia_mat_numeric",
      [&](Json& r) { record(r, "rock.granodiorite")["numericId"] = 32; },
      "numeric id", "rock.granite");
  check(
      "infinitesia_mat_range",
      [&](Json& r) { record(r, "rock.granite")["numericId"] = 600; }, "range",
      "rock.granite");
  check(
      "infinitesia_mat_prefix",
      [&](Json& r) { record(r, "mineral.quartz")["id"] = "rock.quartz"; },
      "must start with", nullptr);
  check(
      "infinitesia_mat_modal",
      [&](Json& r) {
        record(r, "rock.granite")["rock"]["modal"][1]["mineral"] =
            "mineral.nothing";
      },
      "unknown mineral", "rock.granite");
  check(
      "infinitesia_mat_modal_rock",
      [&](Json& r) {
        record(r, "rock.granite")["rock"]["modal"][1]["mineral"] =
            "rock.basalt";
      },
      "not a mineral", "rock.granite");
  check(
      "infinitesia_mat_sum",
      [&](Json& r) {
        record(r, "rock.granite")["rock"]["modal"][0]["fraction"] = 0.5;
      },
      "sum", "rock.granite");
  check(
      "infinitesia_mat_color",
      [&](Json& r) { record(r, "rock.granite")["color"] = "granite"; },
      "colour", nullptr);
  check(
      "infinitesia_mat_nochem",
      [&](Json& r) { record(r, "mineral.quartz").erase("chemistry"); },
      "chemistry", "mineral.quartz");
  check(
      "infinitesia_mat_norock",
      [&](Json& r) { record(r, "rock.granite").erase("rock"); }, "rock",
      "rock.granite");
  check(
      "infinitesia_mat_rockpart",
      [&](Json& r) {
        record(r, "mineral.quartz")["rock"] =
            Json::object({{"class", "metamorphic"}});
      },
      "rock", "mineral.quartz");
  check(
      "infinitesia_mat_class",
      [&](Json& r) { record(r, "rock.granite")["class"] = "plasma"; },
      "unknown material class", nullptr);
  check(
      "infinitesia_mat_digclass",
      [&](Json& r) { record(r, "rock.granite")["digClass"] = 9; }, "digClass",
      nullptr);
  {
    const BrokenFile broken(kMaterialsFile, "infinitesia_mat_nowater");
    broken.Edit([&](Json& r) { record(r, "fluid.water")["class"] = "loose"; });
    CHECK(!LoadError(broken).empty());
  }
  {
    const BrokenFile broken(kMaterialsFile, "infinitesia_mat_json");
    broken.WriteText("{ \"materials\": [ ");
    CHECK(LoadError(broken).find("invalid JSON") != std::string::npos);
  }
  {
    const BrokenFile broken(kMaterialsFile, "infinitesia_mat_missing");
    broken.Remove();
    CHECK(LoadError(broken).find("cannot open") != std::string::npos);
  }
}

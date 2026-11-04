#include <doctest/doctest.h>

#include <algorithm>
#include <set>
#include <string>

#include "blocks/BlockRegistry.h"
#include "support/BrokenFile.h"
#include "support/MaterialFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

namespace {
const BlockRegistry& SharedRegistry() {
  static const BlockRegistry registry =
      BlockRegistry::Build(SharedMaterials(), kShapesFile);
  return registry;
}

MaterialId IdOf(const char* key) {
  const MaterialDef* material = SharedRegistry().FindMaterial(key);
  REQUIRE_MESSAGE(material != nullptr, "no material ", key);
  return material->id;
}

BlockState Full(const char* key) { return {IdOf(key), kShapeFull}; }

// The message of the error loading a broken copy of shapes.json; empty when it
// loads.
std::string LoadError(const BrokenFile& broken) {
  try {
    BlockRegistry::Build(SharedMaterials(), broken.Path());
  } catch (const CatalogError& error) {
    return error.what();
  }
  return {};
}
}  // namespace

TEST_CASE("RegistryHoldsEveryMaterialOfTheCatalog") {
  const BlockRegistry& registry = SharedRegistry();
  const MaterialCatalog& catalog = *SharedMaterials();
  REQUIRE(registry.Materials().size() == catalog.Materials().size());

  std::set<MaterialId> ids;
  for (const MaterialDef& material : registry.Materials()) {
    Require(ids.insert(material.id).second, "duplicate material id");
    Require(registry.GetMaterial(material.id) == &material,
            "lookup by id is wrong");
    Require(registry.FindMaterial(material.key) == &material,
            "lookup by key is wrong");
  }
  CHECK(registry.FindMaterial("built.lamp")->id == 1030);
  CHECK(registry.FindMaterial("rock.granite")->id == 32);
  CHECK(registry.FindMaterial("mineral.quartz")->id == kFirstMineralMaterial);
  CHECK(registry.GetMaterial(4000) == nullptr);
  CHECK(&registry.Catalog() == SharedMaterials().get());

  REQUIRE(registry.Shapes().size() == 1);
  CHECK(registry.FindShape("shape.full")->id == kShapeFull);
  CHECK(registry.GetShape(kShapeFull) == registry.FindShape("shape.full"));
  CHECK(registry.GetShape(9) == nullptr);
}

TEST_CASE("CellQueriesFollowTheMaterial") {
  const BlockRegistry& registry = SharedRegistry();

  const BlockState air{kAirMaterial, kShapeFull};
  CHECK_FALSE(registry.Occupied(air));
  CHECK_FALSE(registry.Walkable(air));
  CHECK_FALSE(registry.Diggable(air));
  CHECK(registry.Opacity(air) == LightRgb{0, 0, 0});

  const BlockState granite = Full("rock.granite");
  CHECK(registry.Occupied(granite));
  CHECK(registry.Walkable(granite));
  CHECK(registry.Diggable(granite));
  CHECK(registry.Opacity(granite) == LightRgb{15, 15, 15});
  CHECK(registry.DigClass(granite.material) == 4);

  // Water is still ground for workers (docs/06_BLOCKS.md, section 0), but dims
  // light a little.
  const BlockState water = Full("fluid.water");
  CHECK(registry.Occupied(water));
  CHECK(registry.Walkable(water));
  CHECK(registry.Opacity(water) == LightRgb{2, 2, 2});

  CHECK(registry.Opacity(Full("built.glass")) == LightRgb{0, 0, 0});
  // Red glass lets red through and stops green and blue.
  CHECK(registry.Opacity(Full("built.glass_red")) == LightRgb{1, 8, 8});
  CHECK(registry.Opacity(Full("ice.glacier")) == LightRgb{3, 3, 3});

  CHECK(registry.Emission(IdOf("built.lamp")) == LightRgb{15, 14, 10});
  CHECK(registry.Emission(IdOf("fluid.magma")) == LightRgb{14, 6, 1});
  CHECK(registry.Emission(IdOf("rock.granite")) == LightRgb{});
  CHECK(registry.Opacity(Full("fluid.magma")) == LightRgb{15, 15, 15});
}

TEST_CASE("UnknownIdsBehaveLikeAirAndLikeAFullCube") {
  const BlockRegistry& registry = SharedRegistry();
  const BlockState unknownMaterial{4000, kShapeFull};
  CHECK_FALSE(registry.Occupied(unknownMaterial));
  CHECK(registry.Opacity(unknownMaterial) == LightRgb{});
  CHECK_FALSE(registry.Has(4000, Tag::Water));
  CHECK_FALSE(registry.Occupied({65535, kShapeFull}));

  const BlockState unknownShape{IdOf("rock.granite"), 77};
  CHECK(registry.Occupied(unknownShape));
  CHECK(registry.Walkable(unknownShape));
  CHECK(registry.FaceMask(77) == kFaceAll);
}

TEST_CASE("TagsLetRulesAskWhatAMaterialIsInsteadOfWhichId") {
  const BlockRegistry& registry = SharedRegistry();
  CHECK(registry.Has(IdOf("fluid.water"), Tag::Water));
  CHECK(registry.Has(IdOf("fluid.seawater"), Tag::Water));
  CHECK(registry.Has(IdOf("fluid.seawater"), Tag::Fluid));
  CHECK_FALSE(registry.Has(IdOf("fluid.brine"), Tag::Water));
  CHECK(registry.Has(IdOf("soil.sand"), Tag::Soil));
  CHECK(registry.Has(IdOf("rock.granite"), Tag::Rock));
  CHECK(registry.Has(IdOf("ice.glacier"), Tag::Ice));
  CHECK(registry.Has(kAirMaterial, Tag::Air));
  CHECK(registry.Has(IdOf("built.glass_blue"), Tag::Glass));
  CHECK(registry.Has(IdOf("built.glass_blue"), Tag::Built));
  // Anything that gives off light is a light source, whatever the data says.
  CHECK(registry.Has(IdOf("built.lamp"), Tag::LightSource));
  CHECK(registry.Has(IdOf("fluid.magma"), Tag::LightSource));
  CHECK_FALSE(registry.Has(IdOf("rock.granite"), Tag::LightSource));

  CHECK(registry.HasTag(IdOf("built.planks"), "wood"));
  CHECK_FALSE(registry.HasTag(IdOf("built.brick"), "wood"));
  CHECK(TagFromName("light_source") == Tag::LightSource);
  CHECK_FALSE(TagFromName("wood").has_value());
  for (int i = 0; i < kEngineTagCount; ++i)
    CHECK(TagFromName(TagName(static_cast<Tag>(i))) == static_cast<Tag>(i));
}

TEST_CASE("BuiltInRegistryAgreesWithTheFiles") {
  const BlockRegistry builtIn = BlockRegistry::BuiltIn();
  const BlockRegistry& files = SharedRegistry();
  CHECK(builtIn.Materials().size() == 7);
  for (const MaterialDef& material : builtIn.Materials()) {
    const MaterialDef* other = files.FindMaterial(material.key);
    REQUIRE_MESSAGE(other != nullptr, material.key, " is not in the files");
    CHECK_MESSAGE(material.id == other->id, material.key);
    CHECK_MESSAGE(material.blockClass == other->blockClass, material.key);
    CHECK_MESSAGE(material.color == other->color, material.key);
    CHECK_MESSAGE(material.digClass == other->digClass, material.key);
    CHECK_MESSAGE(material.opacity == other->opacity, material.key);
    CHECK_MESSAGE(material.emission == other->emission, material.key);
    CHECK_MESSAGE(material.walkable == other->walkable, material.key);
    CHECK_MESSAGE(material.diggable == other->diggable, material.key);
    CHECK_MESSAGE(
        std::set<std::string>(material.tags.begin(), material.tags.end()) ==
            std::set<std::string>(other->tags.begin(), other->tags.end()),
        material.key);
    CHECK_MESSAGE(builtIn.Occupied({material.id, kShapeFull}) ==
                      files.Occupied({other->id, kShapeFull}),
                  material.key);
  }
  CHECK(builtIn.FindShape("shape.full") != nullptr);
}

TEST_CASE("AShapeNarrowsWhatTheMaterialAllows") {
  const BrokenFile blocks(kShapesFile, "infinitesia_blocks_shape");
  blocks.WriteText(R"({
        "shapes": [
            { "id": "shape.full", "numericId": 0, "faces": "all", "walkable": true, "passesLight": false, "mesh": "cube" },
            { "id": "shape.grate", "numericId": 1, "faces": ["top", "bottom"], "walkable": false, "passesLight": true, "mesh": "cube" }
        ] })");
  const BlockRegistry registry =
      BlockRegistry::Build(SharedMaterials(), blocks.Path());
  const MaterialId granite = registry.FindMaterial("rock.granite")->id;
  const ShapeId grate = registry.FindShape("shape.grate")->id;

  CHECK(registry.Occupied({granite, grate}));
  CHECK_FALSE(registry.Walkable({granite, grate}));
  CHECK(registry.Opacity({granite, grate}) == LightRgb{});
  CHECK(registry.Opacity({granite, kShapeFull}) == LightRgb{15, 15, 15});
  CHECK(registry.FaceMask(grate) == (kFaceTop | kFaceBottom));
  CHECK(registry.FaceMask(kShapeFull) == kFaceAll);
  // Air stays empty whatever shape it is given.
  CHECK_FALSE(registry.Occupied({kAirMaterial, grate}));
}

TEST_CASE("LoaderRejectsBrokenShapeFilesWithAMessageNamingTheRecord") {
  const auto shape = [](Json& root) -> Json& {
    return BrokenFile::Record(root, "shapes", "shape.full");
  };
  {
    const BrokenFile broken(kShapesFile, "infinitesia_shapes_fullshape");
    broken.Edit([&](Json& root) { shape(root)["numericId"] = 1; });
    CHECK(LoadError(broken).find("shape 0") != std::string::npos);
  }
  {
    const BrokenFile broken(kShapesFile, "infinitesia_shapes_face");
    broken.Edit(
        [&](Json& root) { shape(root)["faces"] = Json::array({"up"}); });
    const std::string error = LoadError(broken);
    CHECK(error.find("shapes.json") != std::string::npos);
    CHECK(error.find("shape.full") != std::string::npos);
    CHECK(error.find("face") != std::string::npos);
  }
  {
    const BrokenFile broken(kShapesFile, "infinitesia_shapes_mesh");
    broken.Edit([&](Json& root) { shape(root)["mesh"] = "sphere"; });
    CHECK(LoadError(broken).find("mesh") != std::string::npos);
  }
  {
    const BrokenFile broken(kShapesFile, "infinitesia_shapes_prefix");
    broken.Edit([&](Json& root) { shape(root)["id"] = "full"; });
    CHECK(LoadError(broken).find("shape.") != std::string::npos);
  }
  {
    const BrokenFile broken(kShapesFile, "infinitesia_shapes_duplicate");
    broken.Edit([&](Json& root) {
      Json second = shape(root);
      second["numericId"] = 1;
      root.at("shapes").push_back(second);
    });
    CHECK(LoadError(broken).find("duplicate shape id") != std::string::npos);
  }
  {
    const BrokenFile broken(kShapesFile, "infinitesia_shapes_missing");
    broken.Remove();
    CHECK(LoadError(broken).find("cannot open") != std::string::npos);
  }
}

TEST_CASE("LightPassesACellThatDoesNotStopEveryChannel") {
  const BlockRegistry& registry = SharedRegistry();
  CHECK(registry.LetsLightThrough({kAirMaterial, kShapeFull}));
  CHECK(registry.LetsLightThrough(Full("fluid.water")));
  CHECK(registry.LetsLightThrough(Full("ice.glacier")));
  CHECK(registry.LetsLightThrough(Full("built.glass")));
  CHECK(registry.LetsLightThrough(Full("built.glass_red")));
  CHECK_FALSE(registry.LetsLightThrough(Full("rock.granite")));
  CHECK_FALSE(registry.LetsLightThrough(Full("soil.sand")));
  CHECK_FALSE(registry.LetsLightThrough(Full("fluid.magma")));
}

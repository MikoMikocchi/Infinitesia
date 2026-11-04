#include <doctest/doctest.h>

#include <string>

#include "support/BrokenFile.h"
#include "support/GeologyFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

namespace {
// The message of the error loading a broken copy of province_columns.json;
// empty when it loads.
std::string LoadError(const BrokenFile& broken) {
  try {
    GeologyCatalog::Load(SharedMatter(), broken.Path());
  } catch (const CatalogError& error) {
    return error.what();
  }
  return {};
}
}  // namespace

TEST_CASE("ProvinceColumnsCoverEveryProvinceTypeWithKnownRocks") {
  const GeologyCatalog& catalog = SharedCatalog();
  for (int type = 0; type < kProvinceTypeCount; ++type) {
    const ProvinceColumn& column =
        catalog.ColumnFor(static_cast<ProvinceType>(type));
    CHECK_MESSAGE(column.type == static_cast<ProvinceType>(type),
                  ProvinceTypeName(static_cast<ProvinceType>(type)));
    REQUIRE(!column.bottomUp.empty());
    CHECK(column.bottomUp.front().needs == BiosphereRequirement::None);
    for (const ColumnLayer& layer : column.bottomUp) {
      const MatterDef* material = catalog.Matter().Get(layer.rock);
      Require(material != nullptr && material->blockClass == BlockClass::Rock,
              "a column layer is not a rock");
      Require(layer.thicknessMeters > 0.0, "a column layer has no thickness");
    }
  }
}

TEST_CASE("ColumnLoaderRejectsBrokenFilesWithAMessageNamingTheRecord") {
  const auto firstLayer = [](Json& root) -> Json& {
    return root.at("columns").begin().value().at("layers").at(0);
  };
  {
    const BrokenFile broken(kColumnsFile, "infinitesia_columns_unknown");
    broken.Edit([&](Json& root) { firstLayer(root)["rock"] = "cheese"; });
    const std::string error = LoadError(broken);
    CHECK(error.find("unknown rock") != std::string::npos);
    CHECK(error.find("province_columns.json") != std::string::npos);
  }
  {
    // A layer must be a rock: a mineral, though a material, is not.
    const BrokenFile broken(kColumnsFile, "infinitesia_columns_mineral");
    broken.Edit(
        [&](Json& root) { firstLayer(root)["rock"] = "quartz"; });
    CHECK(LoadError(broken).find("unknown rock") != std::string::npos);
  }
  {
    const BrokenFile broken(kColumnsFile, "infinitesia_columns_structure");
    broken.Edit([](Json& root) {
      root.at("columns").begin().value().at("structure")["foldCount"] = 9;
    });
    CHECK(LoadError(broken).find("foldCount") != std::string::npos);
  }
  {
    const BrokenFile broken(kColumnsFile, "infinitesia_columns_missing");
    broken.Remove();
    CHECK(LoadError(broken).find("cannot open") != std::string::npos);
  }
}

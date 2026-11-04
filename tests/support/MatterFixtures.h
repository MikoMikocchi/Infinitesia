#pragma once

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string_view>

#include "domains/natural/chemistry/Element.h"
#include "matter/MatterCatalog.h"

// Shared by the tests that read the real material catalog.
namespace InfinitesiaTests {
using namespace Infinitesia;

inline const std::filesystem::path kAssets = INFINITESIA_ASSET_DIR;
inline const std::filesystem::path kMatterFile =
    kAssets / "matter" / "matter.json";
inline const std::filesystem::path kColumnsFile =
    kAssets / "geology" / "province_columns.json";
inline const std::filesystem::path kShapesFile =
    kAssets / "blocks" / "shapes.json";

inline std::shared_ptr<const MatterCatalog> SharedMatter() {
  static const auto materials = std::make_shared<const MatterCatalog>(
      MatterCatalog::LoadFromFile(kMatterFile));
  return materials;
}

inline const MatterDef& MatterNamed(std::string_view key) {
  const MatterDef* material = SharedMatter()->Find(key);
  REQUIRE_MESSAGE(material != nullptr, key);
  return *material;
}

inline int Z(std::string_view symbol) {
  return Chemistry::FindElement(symbol)->atomicNumber;
}

inline double Fraction(const Chemistry::ElementMassFractions& fractions,
                       std::string_view symbol) {
  return fractions.Fraction(Z(symbol));
}
}  // namespace InfinitesiaTests

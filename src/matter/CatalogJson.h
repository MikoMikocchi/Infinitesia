#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "materials/CatalogError.h"
#include "materials/MaterialTypes.h"

// JSON helpers shared by the catalog loaders (materials, geology columns, block
// shapes): reading a file, required fields, and errors that name the file and
// the record.
namespace Infinitesia::CatalogJson {
using Json = nlohmann::json;

// Context of an error message: the file and the record being read.
struct Where {
  std::string file;
  std::string record;
};

[[noreturn]] void Fail(const Where& where, const std::string& what);

Json ReadJson(const std::filesystem::path& path);

const Json& Field(const Json& object, const char* key, const Where& where);
std::string StringField(const Json& object, const char* key,
                        const Where& where);
double NumberField(const Json& object, const char* key, const Where& where);
int IntField(const Json& object, const char* key, const Where& where);
const Json& ArrayField(const Json& object, const char* key, const Where& where);

void ReadRange(const Json& object, const char* key, const Where& where,
               double lowest, double highest, double& first, double& second);
bool StartsWith(const std::string& text, const char* prefix);

// Light values (docs/06_BLOCKS.md): a level 0-15, or [r, g, b]. Absent gives
// `fallback`. `allowNumber` lets a single number stand for all three channels
// (opacity, not emission).
LightRgb LightField(const Json& object, const char* key, const Where& where,
                    LightRgb fallback, bool allowNumber);
// The tags of a material: the tag of its class plus the optional "tags" array.
std::vector<std::string> TagsField(const Json& object, BlockClass blockClass,
                                   const Where& where);
// An optional boolean field.
bool BoolField(const Json& object, const char* key, const Where& where,
               bool fallback);

RgbColor ColorField(const Json& object, const Where& where);
Provenance ProvenanceField(const Json& object, const Where& where);
}  // namespace Infinitesia::CatalogJson

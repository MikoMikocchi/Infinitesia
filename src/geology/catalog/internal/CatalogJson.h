#pragma once

#include "geology/catalog/CatalogTypes.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

// JSON helpers shared by the catalog loaders: reading a file, required fields, and errors
// that name the file and the record.
namespace Infinitesia::CatalogInternal
{
    using Json = nlohmann::json;

    // Context of an error message: the file and the record being read.
    struct Where
    {
        std::string file;
        std::string record;
    };

    [[noreturn]] void Fail(const Where& where, const std::string& what);

    Json ReadJson(const std::filesystem::path& path);

    const Json& Field(const Json& object, const char* key, const Where& where);
    std::string StringField(const Json& object, const char* key, const Where& where);
    double NumberField(const Json& object, const char* key, const Where& where);
    int IntField(const Json& object, const char* key, const Where& where);
    const Json& ArrayField(const Json& object, const char* key, const Where& where);

    void CheckSchemaVersion(const Json& root, const Where& where, int expected = 1);
    void ReadRange(const Json& object, const char* key, const Where& where, double lowest, double highest,
                   double& first, double& second);
    bool StartsWith(const std::string& text, const char* prefix);

    RgbColor ColorField(const Json& object, const Where& where);
    Provenance ProvenanceField(const Json& object, const Where& where);
}

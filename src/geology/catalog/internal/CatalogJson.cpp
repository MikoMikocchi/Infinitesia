#include "geology/catalog/internal/CatalogJson.h"

#include "geology/catalog/GeologyCatalog.h"

#include <algorithm>
#include <fstream>

namespace Infinitesia::CatalogInternal
{
    [[noreturn]] void Fail(const Where& where, const std::string& what)
    {
        throw CatalogError(where.file + (where.record.empty() ? "" : ": " + where.record) + ": " + what);
    }

    Json ReadJson(const std::filesystem::path& path)
    {
        std::ifstream stream(path);
        if (!stream)
        {
            throw CatalogError(path.string() + ": cannot open the file");
        }
        try
        {
            return Json::parse(stream);
        }
        catch (const Json::parse_error& error)
        {
            throw CatalogError(path.string() + ": invalid JSON: " + error.what());
        }
    }

    const Json& Field(const Json& object, const char* key, const Where& where)
    {
        if (!object.is_object() || !object.contains(key))
        {
            Fail(where, std::string("missing field '") + key + "'");
        }
        return object.at(key);
    }

    std::string StringField(const Json& object, const char* key, const Where& where)
    {
        const Json& value = Field(object, key, where);
        if (!value.is_string())
        {
            Fail(where, std::string("field '") + key + "' must be a string");
        }
        return value.get<std::string>();
    }

    double NumberField(const Json& object, const char* key, const Where& where)
    {
        const Json& value = Field(object, key, where);
        if (!value.is_number())
        {
            Fail(where, std::string("field '") + key + "' must be a number");
        }
        return value.get<double>();
    }

    int IntField(const Json& object, const char* key, const Where& where)
    {
        const Json& value = Field(object, key, where);
        if (!value.is_number_integer())
        {
            Fail(where, std::string("field '") + key + "' must be an integer");
        }
        return value.get<int>();
    }

    const Json& ArrayField(const Json& object, const char* key, const Where& where)
    {
        const Json& value = Field(object, key, where);
        if (!value.is_array())
        {
            Fail(where, std::string("field '") + key + "' must be an array");
        }
        return value;
    }

    void ReadRange(const Json& object, const char* key, const Where& where, double lowest, double highest,
                   double& first, double& second)
    {
        const Json& value = Field(object, key, where);
        if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number())
            Fail(where, std::string("field '") + key + "' must be [min, max]");
        first = value[0].get<double>();
        second = value[1].get<double>();
        if (!(first >= lowest && first <= second && second <= highest))
            Fail(where, std::string("field '") + key + "' must satisfy " + std::to_string(lowest) +
                        " <= min <= max <= " + std::to_string(highest));
    }

    bool StartsWith(const std::string& text, const char* prefix)
    {
        return text.rfind(prefix, 0) == 0;
    }

    static int HexDigit(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static std::uint8_t LightLevel(const Json& value, const char* key, const Where& where)
    {
        if (!value.is_number_integer())
            Fail(where, std::string("field '") + key + "' must hold whole numbers");
        const int level = value.get<int>();
        if (level < 0 || level > kMaxLightLevel)
            Fail(where, std::string("field '") + key + "' must be 0-15");
        return static_cast<std::uint8_t>(level);
    }

    LightRgb LightField(const Json& object, const char* key, const Where& where, LightRgb fallback, bool allowNumber)
    {
        if (!object.contains(key))
            return fallback;
        const Json& value = object.at(key);
        if (allowNumber && value.is_number())
        {
            const std::uint8_t level = LightLevel(value, key, where);
            return { level, level, level };
        }
        if (!value.is_array() || value.size() != 3)
            Fail(where, std::string("field '") + key + "' must be " + (allowNumber ? "a number or " : "") + "[r, g, b]");
        return { LightLevel(value[0], key, where), LightLevel(value[1], key, where), LightLevel(value[2], key, where) };
    }

    std::vector<std::string> TagsField(const Json& object, BlockClass blockClass, const Where& where)
    {
        std::vector<std::string> tags{ ClassTag(blockClass) };
        if (!object.contains("tags"))
            return tags;
        const Json& list = object.at("tags");
        if (!list.is_array())
            Fail(where, "field 'tags' must be an array of strings");
        for (const Json& tag : list)
        {
            if (!tag.is_string() || tag.get<std::string>().empty())
                Fail(where, "field 'tags' must be an array of non-empty strings");
            const std::string text = tag.get<std::string>();
            if (std::find(tags.begin(), tags.end(), text) == tags.end())
                tags.push_back(text);
        }
        return tags;
    }

    bool BoolField(const Json& object, const char* key, const Where& where, bool fallback)
    {
        if (!object.contains(key))
            return fallback;
        const Json& value = object.at(key);
        if (!value.is_boolean())
            Fail(where, std::string("field '") + key + "' must be true or false");
        return value.get<bool>();
    }

    RgbColor ColorField(const Json& object, const Where& where)
    {
        const std::string text = StringField(object, "color", where);
        if (text.size() != 7 || text[0] != '#')
        {
            Fail(where, "colour must look like #rrggbb, got '" + text + "'");
        }
        int channel[3];
        for (int i = 0; i < 3; ++i)
        {
            const int high = HexDigit(text[static_cast<std::size_t>(1 + 2 * i)]);
            const int low = HexDigit(text[static_cast<std::size_t>(2 + 2 * i)]);
            if (high < 0 || low < 0)
            {
                Fail(where, "colour must look like #rrggbb, got '" + text + "'");
            }
            channel[i] = high * 16 + low;
        }
        return { static_cast<std::uint8_t>(channel[0]), static_cast<std::uint8_t>(channel[1]),
                 static_cast<std::uint8_t>(channel[2]) };
    }

    Provenance ProvenanceField(const Json& object, const Where& where)
    {
        const std::string text = StringField(object, "provenance", where);
        if (text == "reference") return Provenance::Reference;
        if (text == "derived") return Provenance::Derived;
        if (text == "gameplay") return Provenance::Gameplay;
        Fail(where, "provenance must be reference, derived or gameplay, got '" + text + "'");
    }
}

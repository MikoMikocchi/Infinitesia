#pragma once

#include "chemistry/Element.h"
#include "geology/catalog/GeologyCatalog.h"
#include "planet/PlanetMap.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

// Shared by the geology test files: the catalog and planet every test reads, plus the
// tools to break a copy of the catalog on purpose.
namespace InfinitesiaTests
{
    using namespace Infinitesia;

    inline const std::filesystem::path kAssets = INFINITESIA_GEOLOGY_ASSET_DIR;

    inline const GeologyCatalog& SharedCatalog()
    {
        static const GeologyCatalog catalog = GeologyCatalog::LoadFromDirectory(kAssets);
        return catalog;
    }

    inline int Z(std::string_view symbol)
    {
        return Chemistry::FindElement(symbol)->atomicNumber;
    }

    inline double Fraction(const Chemistry::ElementMassFractions& fractions, std::string_view symbol)
    {
        return fractions.Fraction(Z(symbol));
    }

    inline const Rock& RockNamed(std::string_view id)
    {
        const Rock* rock = SharedCatalog().FindRock(id);
        REQUIRE(rock != nullptr);
        return *rock;
    }

    // A copy of the catalog files in a fresh directory, to be broken on purpose.
    struct BrokenCatalog
    {
        std::filesystem::path directory;

        explicit BrokenCatalog(const char* name)
            : directory(std::filesystem::temp_directory_path() / name)
        {
            std::filesystem::remove_all(directory);
            std::filesystem::create_directories(directory);
            for (const char* file : { "minerals.json", "rocks.json", "materials.json", "province_columns.json" })
            {
                std::filesystem::copy_file(kAssets / file, directory / file);
            }
        }
        ~BrokenCatalog() { std::filesystem::remove_all(directory); }

        std::string Read(const char* file) const
        {
            std::ifstream stream(directory / file);
            std::stringstream buffer;
            buffer << stream.rdbuf();
            return buffer.str();
        }

        void Replace(const char* file, const std::string& from, const std::string& to) const
        {
            std::string text = Read(file);
            const std::size_t at = text.find(from);
            REQUIRE_MESSAGE(at != std::string::npos, "test text not found: ", from);
            text.replace(at, from.size(), to);
            std::ofstream(directory / file, std::ios::trunc) << text;
        }

        std::string LoadError() const
        {
            try
            {
                GeologyCatalog::LoadFromDirectory(directory);
            }
            catch (const CatalogError& error)
            {
                return error.what();
            }
            return {};
        }
    };

    inline const PlanetMap& SharedMap()
    {
        static const PlanetMap map = []
        {
            PlanetParameters parameters;
            parameters.latitudeBands = 90;
            parameters.longitudeSegments = 180;
            return PlanetMap(parameters);
        }();
        return map;
    }
}

#include <doctest/doctest.h>

#include "blocks/BlockRegistry.h"
#include "support/GeologyFixtures.h"
#include "support/Require.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

using namespace Infinitesia;
using namespace InfinitesiaTests;

namespace
{
    const std::filesystem::path kBlocksAssets = INFINITESIA_BLOCKS_ASSET_DIR;

    const BlockRegistry& SharedRegistry()
    {
        static const BlockRegistry registry = BlockRegistry::Build(SharedCatalog(), kBlocksAssets);
        return registry;
    }

    MaterialId IdOf(const char* key)
    {
        const MaterialDef* material = SharedRegistry().FindMaterial(key);
        REQUIRE_MESSAGE(material != nullptr, "no material ", key);
        return material->id;
    }

    BlockState Full(const char* key)
    {
        return { IdOf(key), kShapeFull };
    }

    // A copy of the blocks files in a fresh directory, to be broken on purpose.
    struct BrokenBlocks
    {
        std::filesystem::path directory;

        explicit BrokenBlocks(const char* name)
            : directory(std::filesystem::temp_directory_path() / name)
        {
            std::filesystem::remove_all(directory);
            std::filesystem::create_directories(directory);
            for (const char* file : { "built_materials.json", "shapes.json" })
                std::filesystem::copy_file(kBlocksAssets / file, directory / file);
        }
        ~BrokenBlocks() { std::filesystem::remove_all(directory); }

        void Replace(const char* file, const std::string& from, const std::string& to) const
        {
            std::ifstream in(directory / file);
            std::stringstream buffer;
            buffer << in.rdbuf();
            std::string text = buffer.str();
            const std::size_t at = text.find(from);
            REQUIRE_MESSAGE(at != std::string::npos, "test text not found: ", from);
            text.replace(at, from.size(), to);
            std::ofstream(directory / file, std::ios::trunc) << text;
        }

        void Write(const char* file, const std::string& text) const
        {
            std::ofstream(directory / file, std::ios::trunc) << text;
        }

        std::string LoadError() const
        {
            try
            {
                BlockRegistry::Build(SharedCatalog(), directory);
            }
            catch (const CatalogError& error)
            {
                return error.what();
            }
            return {};
        }
    };
}

TEST_CASE("RegistryHoldsGeologyAndBuiltMaterials")
{
    const BlockRegistry& registry = SharedRegistry();
    const GeologyCatalog& geology = SharedCatalog();
    CHECK(registry.Materials().size() == geology.Materials().size() + 7);

    std::set<MaterialId> ids;
    for (const MaterialDef& material : registry.Materials())
    {
        Require(ids.insert(material.id).second, "duplicate material id");
        Require(registry.GetMaterial(material.id) == &material, "lookup by id is wrong");
        Require(registry.FindMaterial(material.key) == &material, "lookup by key is wrong");
        if (material.blockClass == BlockClass::Built)
            Require(material.id >= kFirstBuiltMaterial, "a built material lives in 1024 and up");
        else
            Require(geology.FindMaterial(material.key) != nullptr, "a geology material is missing from the catalog");
    }
    CHECK(registry.FindMaterial("built.lamp")->id == 1030);
    CHECK(registry.FindMaterial("rock.granite")->id == 32);
    CHECK(registry.GetMaterial(4000) == nullptr);

    REQUIRE(registry.Shapes().size() == 1);
    CHECK(registry.FindShape("shape.full")->id == kShapeFull);
    CHECK(registry.GetShape(kShapeFull) == registry.FindShape("shape.full"));
    CHECK(registry.GetShape(9) == nullptr);
}

TEST_CASE("CellQueriesFollowTheMaterial")
{
    const BlockRegistry& registry = SharedRegistry();

    const BlockState air{ kAirMaterial, kShapeFull };
    CHECK_FALSE(registry.Occupied(air));
    CHECK_FALSE(registry.Walkable(air));
    CHECK_FALSE(registry.Diggable(air));
    CHECK(registry.Opacity(air) == LightRgb{ 0, 0, 0 });

    const BlockState granite = Full("rock.granite");
    CHECK(registry.Occupied(granite));
    CHECK(registry.Walkable(granite));
    CHECK(registry.Diggable(granite));
    CHECK(registry.Opacity(granite) == LightRgb{ 15, 15, 15 });
    CHECK(registry.DigClass(granite.material) == 4);

    // Water is still ground for workers (docs/06_BLOCKS.md, section 0), but dims light a little.
    const BlockState water = Full("fluid.water");
    CHECK(registry.Occupied(water));
    CHECK(registry.Walkable(water));
    CHECK(registry.Opacity(water) == LightRgb{ 2, 2, 2 });

    CHECK(registry.Opacity(Full("built.glass")) == LightRgb{ 0, 0, 0 });
    // Red glass lets red through and stops green and blue.
    CHECK(registry.Opacity(Full("built.glass_red")) == LightRgb{ 1, 8, 8 });
    CHECK(registry.Opacity(Full("ice.glacier")) == LightRgb{ 3, 3, 3 });

    CHECK(registry.Emission(IdOf("built.lamp")) == LightRgb{ 15, 14, 10 });
    CHECK(registry.Emission(IdOf("fluid.magma")) == LightRgb{ 14, 6, 1 });
    CHECK(registry.Emission(IdOf("rock.granite")) == LightRgb{});
    CHECK(registry.Opacity(Full("fluid.magma")) == LightRgb{ 15, 15, 15 });
}

TEST_CASE("UnknownIdsBehaveLikeAirAndLikeAFullCube")
{
    const BlockRegistry& registry = SharedRegistry();
    const BlockState unknownMaterial{ 4000, kShapeFull };
    CHECK_FALSE(registry.Occupied(unknownMaterial));
    CHECK(registry.Opacity(unknownMaterial) == LightRgb{});
    CHECK_FALSE(registry.Has(4000, Tag::Water));
    CHECK_FALSE(registry.Occupied({ 65535, kShapeFull }));

    const BlockState unknownShape{ IdOf("rock.granite"), 77 };
    CHECK(registry.Occupied(unknownShape));
    CHECK(registry.Walkable(unknownShape));
    CHECK(registry.FaceMask(77) == kFaceAll);
}

TEST_CASE("TagsLetRulesAskWhatAMaterialIsInsteadOfWhichId")
{
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

TEST_CASE("BuiltInRegistryAgreesWithTheFiles")
{
    const BlockRegistry builtIn = BlockRegistry::BuiltIn();
    const BlockRegistry& files = SharedRegistry();
    CHECK(builtIn.Materials().size() == 7);
    for (const MaterialDef& material : builtIn.Materials())
    {
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
        CHECK_MESSAGE(std::set<std::string>(material.tags.begin(), material.tags.end()) ==
                          std::set<std::string>(other->tags.begin(), other->tags.end()),
                      material.key);
        CHECK_MESSAGE(builtIn.Occupied({ material.id, kShapeFull }) == files.Occupied({ other->id, kShapeFull }), material.key);
    }
    CHECK(builtIn.FindShape("shape.full") != nullptr);
}

TEST_CASE("AShapeNarrowsWhatTheMaterialAllows")
{
    const BrokenBlocks blocks("infinitesia_blocks_shape");
    blocks.Write("shapes.json", R"({
        "shapes": [
            { "id": "shape.full", "numericId": 0, "faces": "all", "walkable": true, "passesLight": false, "mesh": "cube" },
            { "id": "shape.grate", "numericId": 1, "faces": ["top", "bottom"], "walkable": false, "passesLight": true, "mesh": "cube" }
        ] })");
    const BlockRegistry registry = BlockRegistry::Build(SharedCatalog(), blocks.directory);
    const MaterialId granite = registry.FindMaterial("rock.granite")->id;
    const ShapeId grate = registry.FindShape("shape.grate")->id;

    CHECK(registry.Occupied({ granite, grate }));
    CHECK_FALSE(registry.Walkable({ granite, grate }));
    CHECK(registry.Opacity({ granite, grate }) == LightRgb{});
    CHECK(registry.Opacity({ granite, kShapeFull }) == LightRgb{ 15, 15, 15 });
    CHECK(registry.FaceMask(grate) == (kFaceTop | kFaceBottom));
    CHECK(registry.FaceMask(kShapeFull) == kFaceAll);
    // Air stays empty whatever shape it is given.
    CHECK_FALSE(registry.Occupied({ kAirMaterial, grate }));
}

TEST_CASE("LoaderRejectsBrokenBlockFilesWithAMessageNamingTheRecord")
{
    {
        const BrokenBlocks broken("infinitesia_blocks_numeric");
        broken.Replace("built_materials.json", "\"numericId\": 1025", "\"numericId\": 1024");
        CHECK(broken.LoadError().find("numeric id 1024") != std::string::npos);
    }
    {
        const BrokenBlocks broken("infinitesia_blocks_range");
        broken.Replace("built_materials.json", "\"numericId\": 1025", "\"numericId\": 1023");
        const std::string error = broken.LoadError();
        CHECK(error.find("built_materials.json") != std::string::npos);
        CHECK(error.find("built.brick") != std::string::npos);
        CHECK(error.find("1024-4095") != std::string::npos);
    }
    {
        const BrokenBlocks broken("infinitesia_blocks_prefix");
        broken.Replace("built_materials.json", "\"id\": \"built.brick\"", "\"id\": \"brick\"");
        CHECK(broken.LoadError().find("built.") != std::string::npos);
    }
    {
        const BrokenBlocks broken("infinitesia_blocks_duplicate");
        broken.Replace("built_materials.json", "\"id\": \"built.brick\"", "\"id\": \"built.planks\"");
        CHECK(broken.LoadError().find("duplicate material id") != std::string::npos);
    }
    {
        const BrokenBlocks broken("infinitesia_blocks_opacity");
        broken.Replace("built_materials.json", "\"opacity\": [1, 8, 8]", "\"opacity\": [1, 8, 16]");
        CHECK(broken.LoadError().find("opacity") != std::string::npos);
    }
    {
        const BrokenBlocks broken("infinitesia_blocks_fullshape");
        broken.Replace("shapes.json", "\"numericId\": 0", "\"numericId\": 1");
        CHECK(broken.LoadError().find("shape 0") != std::string::npos);
    }
    {
        const BrokenBlocks broken("infinitesia_blocks_face");
        broken.Replace("shapes.json", "\"faces\": \"all\"", "\"faces\": [\"up\"]");
        const std::string error = broken.LoadError();
        CHECK(error.find("shapes.json") != std::string::npos);
        CHECK(error.find("shape.full") != std::string::npos);
        CHECK(error.find("face") != std::string::npos);
    }
    {
        const BrokenBlocks broken("infinitesia_blocks_mesh");
        broken.Replace("shapes.json", "\"mesh\": \"cube\"", "\"mesh\": \"sphere\"");
        CHECK(broken.LoadError().find("mesh") != std::string::npos);
    }
    {
        const BrokenBlocks broken("infinitesia_blocks_missing");
        std::filesystem::remove(broken.directory / "shapes.json");
        CHECK(broken.LoadError().find("cannot open") != std::string::npos);
    }
}

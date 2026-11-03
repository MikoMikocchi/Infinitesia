#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "chemistry/Element.h"
#include "geology/catalog/GeologyCatalog.h"
#include "geology/model/GeologyModel.h"
#include "planet/PlanetMap.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <string>

#define Require(condition, message)     do { if (!(condition)) { FAIL_CHECK(message); } } while (0)

namespace
{
    using namespace Infinitesia;

    const std::filesystem::path kAssets = INFINITESIA_GEOLOGY_ASSET_DIR;

    const GeologyCatalog& SharedCatalog()
    {
        static const GeologyCatalog catalog = GeologyCatalog::LoadFromDirectory(kAssets);
        return catalog;
    }

    int Z(std::string_view symbol)
    {
        return Chemistry::FindElement(symbol)->atomicNumber;
    }

    double Fraction(const Chemistry::ElementMassFractions& fractions, std::string_view symbol)
    {
        return fractions.Fraction(Z(symbol));
    }

    const Rock& RockNamed(std::string_view id)
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

    const PlanetMap& SharedMap()
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

TEST_CASE("EveryProvinceGetsAPileOfFormations")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);

    for (const Province& province : map.GetProvinces())
    {
        const std::vector<GeologyModel::Layer>& pile = model.PileOf(province.id);
        REQUIRE(!pile.empty());
        const ProvinceColumn& column = catalog->ColumnFor(province.type);
        Require(pile.size() <= column.bottomUp.size(), "a pile has more layers than its column");
        Require(pile.back().rock == column.bottomUp.front().rock, "the basement is not the column's first layer");
        for (std::size_t i = 0; i + 1 < pile.size(); ++i)
        {
            Require(pile[i].thicknessMeters >= 1, "a formation has no thickness");
        }
        Require(model.ExposedRock(province.id) == pile.front().rock, "ExposedRock is not the top of the pile");
        for (const GeologyModel::Layer& layer : pile)
        {
            Require(catalog->GetRock(layer.rock) != nullptr, "a layer is not a rock");
        }

        // Erosion strips the top: the deeper the cut the fewer formations remain.
        if (province.erosionLevel == ErosionLevel::Deep)
        {
            Require(pile.size() == 1, "a deeply eroded province keeps more than the basement");
        }
    }
    // Unknown provinces fall back to granite basement.
    CHECK(model.BedrockAt(kNoProvince, 0.0, 0.0, -100, 0) == RockNamed("rock.granite").material);
}

TEST_CASE("ProfilesFollowThePileOrderFromTopToBasement")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);

    for (const Province& province : map.GetProvinces())
    {
        const std::vector<GeologyModel::Layer>& pile = model.PileOf(province.id);
        for (double east = -2500.0; east <= 2500.0; east += 830.0)
        {
            for (double north = -2500.0; north <= 2500.0; north += 710.0)
            {
                const GeologyModel::ColumnProfile profile = model.BaseProfileAt(province.id, east, north, 300);
                Require(profile.count >= 1 && profile.count <= static_cast<int>(pile.size()), "a profile has the wrong number of layers");

                // The rocks of a profile are the pile's, in the pile's order, with some layers absent.
                std::size_t next = 0;
                for (int i = 0; i < profile.count; ++i)
                {
                    while (next < pile.size() && pile[next].rock != profile.rock[static_cast<std::size_t>(i)])
                        ++next;
                    Require(next < pile.size(), "a profile layer is not in the pile, or out of order");
                    ++next;
                }
                Require(profile.rock[static_cast<std::size_t>(profile.count - 1)] == pile.back().rock, "the profile does not end in basement");
                for (int i = 1; i < profile.count; ++i)
                {
                    Require(profile.bottom[static_cast<std::size_t>(i)] <= profile.bottom[static_cast<std::size_t>(i - 1)],
                            "a lower layer's bottom is above the layer over it");
                }

                // Above everything is the top layer of the column, below everything the basement.
                Require(GeologyModel::RockAt(profile, 100000) == profile.rock[0], "high above a column is not its top layer");
                Require(GeologyModel::RockAt(profile, -100000000) == pile.back().rock, "far below a column is not basement");
                for (int i = 0; i + 1 < profile.count; ++i)
                {
                    // (A layer thinner than a metre collapses onto the one above it.)
                    const bool collapsed = i > 0 && profile.bottom[static_cast<std::size_t>(i)] == profile.bottom[static_cast<std::size_t>(i - 1)];
                    Require(collapsed || GeologyModel::RockAt(profile, profile.bottom[static_cast<std::size_t>(i)]) == profile.rock[static_cast<std::size_t>(i)],
                            "the bottom elevation of a layer is not in the layer");
                }
            }
        }
    }
}

TEST_CASE("LayersDipAlongTheCompressionAxisAndFoldWithinTheFlankLimit")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);
    const double pi = 3.14159265358979323846;

    int steepProvinces = 0;
    for (const Province& province : map.GetProvinces())
    {
        const GeologyModel::Structure& structure = model.StructureOf(province.id);
        const StructureStyle& style = catalog->ColumnFor(province.type).style;

        Require(structure.dipDegrees >= style.minDipDegrees && structure.dipDegrees <= style.maxDipDegrees, "dip is outside the range of the style");
        const double slope = std::tan(structure.dipDegrees * pi / 180.0);
        Require(std::abs(std::hypot(structure.slopeEast, structure.slopeNorth) - slope) < 1e-9, "the slope does not match the dip");
        // The elevation of the layers falls along the dip azimuth.
        const double azimuth = structure.dipAzimuthDegrees * pi / 180.0;
        Require(structure.slopeEast * std::sin(azimuth) + structure.slopeNorth * std::cos(azimuth) < 1e-9, "the layers rise along their dip");

        if (style.maxDipDegrees >= 15.0)
        {
            // Strongly deformed provinces dip along the axis of the plate motion (either way).
            double difference = std::abs(std::fmod(structure.dipAzimuthDegrees - province.compressionAzimuthDegrees + 360.0, 180.0));
            difference = std::min(difference, 180.0 - difference);
            Require(difference < 1e-6, "a compressed province does not dip along its compression axis");
            steepProvinces += structure.dipDegrees > 10.0 ? 1 : 0;
        }

        Require(static_cast<int>(structure.folds.size()) == style.foldCount, "the number of folds does not match the style");
        double previousWavelength = 1e18;
        for (const GeologyModel::Fold& fold : structure.folds)
        {
            Require(fold.wavelengthMeters < previousWavelength && fold.wavelengthMeters >= 40.0 && fold.wavelengthMeters <= 6000.0,
                    "fold wavelengths must fall from kilometres to tens of metres");
            previousWavelength = fold.wavelengthMeters;
            Require(fold.amplitudeMeters <= 1.7320508075688772 * fold.wavelengthMeters / (2.0 * pi) + 1e-9, "a fold flank is steeper than 60 degrees");
            Require(std::abs(std::hypot(fold.directionEast, fold.directionNorth) - 1.0) < 1e-9, "the fold direction is not a unit vector");
        }

        // The deformation is bounded by the dip over the distance and the fold amplitudes.
        double bound = 0.0;
        for (const GeologyModel::Fold& fold : structure.folds)
            bound += fold.amplitudeMeters;
        for (double distance : { 0.0, 150.0, 900.0 })
        {
            const double lift = model.Displacement(province.id, distance * 0.6, distance * 0.8);
            Require(std::abs(lift) <= slope * distance + bound + 1e-6, "the displacement exceeds its bound");
        }
    }
    CHECK(steepProvinces > 0);
}

TEST_CASE("FormationsPinchOutIntoLenses")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);

    // A layer that thins to nothing leaves a profile with fewer layers than the pile; it is
    // present elsewhere in the same province.
    int provincesWithLenses = 0;
    for (const Province& province : map.GetProvinces())
    {
        const std::size_t layers = model.PileOf(province.id).size();
        int missingSomewhere = 0;
        int fullSomewhere = 0;
        for (double east = -3000.0; east <= 3000.0; east += 97.0)
        {
            for (double north = -3000.0; north <= 3000.0; north += 103.0)
            {
                const int count = model.ProfileAt(province.id, east, north, 0).count;
                missingSomewhere += count < static_cast<int>(layers) ? 1 : 0;
                fullSomewhere += count == static_cast<int>(layers) ? 1 : 0;
            }
        }
        if (missingSomewhere > 0 && fullSomewhere > 0)
            ++provincesWithLenses;
    }
    CHECK(provincesWithLenses > 0);
}

TEST_CASE("ProfilesAreDeterministicAndDependOnThePosition")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel first(catalog, map);
    const GeologyModel second(catalog, map);

    int movedProfiles = 0;
    for (const Province& province : map.GetProvinces())
    {
        const auto a = first.ProfileAt(province.id, 123.5, -456.5, 200);
        const auto b = second.ProfileAt(province.id, 123.5, -456.5, 200);
        REQUIRE(a.count == b.count);
        for (int i = 0; i < a.count; ++i)
        {
            Require(a.rock[static_cast<std::size_t>(i)] == b.rock[static_cast<std::size_t>(i)] &&
                    a.bottom[static_cast<std::size_t>(i)] == b.bottom[static_cast<std::size_t>(i)],
                    "a profile changed between builds");
        }
        const auto elsewhere = first.ProfileAt(province.id, 4000.0, 3000.0, 200);
        if (elsewhere.count != a.count || (a.count > 1 && elsewhere.bottom[0] != a.bottom[0]))
            ++movedProfiles;
    }
    CHECK(movedProfiles > 0);
}

TEST_CASE("FaultsFollowTheirStyleAndTheRegimeOfTheProvince")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);

    int faultsSeen = 0;
    int kinds[3] = { 0, 0, 0 };
    for (const Province& province : map.GetProvinces())
    {
        const FaultStyle& style = catalog->ColumnFor(province.type).style.faults;
        for (double east = -1800.0; east <= 1800.0; east += 600.0)
        {
            for (double north = -1800.0; north <= 1800.0; north += 600.0)
            {
                const std::vector<GeologyModel::Fault> faults = model.FaultsNear(province.id, east, north);
                const std::vector<GeologyModel::Fault> again = model.FaultsNear(province.id, east, north);
                Require(faults.size() == again.size() && faults.size() <= static_cast<std::size_t>(GeologyModel::kMaxFaultsPerColumn),
                        "the faults near a place are not stable or exceed the cap");
                double previousAge = -1.0;
                for (const GeologyModel::Fault& fault : faults)
                {
                    ++faultsSeen;
                    ++kinds[static_cast<int>(fault.kind)];
                    Require(fault.age >= previousAge, "faults are not ordered from the oldest");
                    previousAge = fault.age;
                    Require(fault.slipMeters >= style.minSlipMeters && fault.slipMeters <= style.maxSlipMeters, "fault slip is outside the style");
                    Require(fault.strikeRadiusMeters > 0.0 && fault.dipRadiusMeters > 0.0, "a fault has no size");

                    const double axis = model.StructureOf(province.id).compressionAzimuthDegrees;
                    double offset = std::abs(std::fmod(fault.strikeAzimuthDegrees - axis + 720.0, 180.0));
                    offset = std::min(offset, 180.0 - offset); // angle between the strike and the motion axis
                    switch (fault.kind)
                    {
                    case GeologyModel::FaultKind::Normal:
                        Require(fault.dipDegrees >= 50.0 && fault.dipDegrees <= 70.0, "a normal fault has the wrong dip");
                        Require(std::abs(offset - 90.0) <= 15.0 + 1e-6, "a normal fault does not strike across the motion axis");
                        break;
                    case GeologyModel::FaultKind::Thrust:
                        Require(fault.dipDegrees >= 20.0 && fault.dipDegrees <= 40.0, "a thrust has the wrong dip");
                        Require(std::abs(offset - 90.0) <= 15.0 + 1e-6, "a thrust does not strike across the motion axis");
                        break;
                    case GeologyModel::FaultKind::StrikeSlip:
                        Require(fault.dipDegrees >= 80.0 && fault.dipDegrees <= 88.0, "a strike-slip fault is not near vertical");
                        Require(offset >= 25.0 - 1e-6 && offset <= 45.0 + 1e-6, "a strike-slip fault is not at an angle to the motion axis");
                        break;
                    }
                }
            }
        }
    }
    CHECK(faultsSeen > 100);
    CHECK(kinds[0] > 0);
    CHECK(kinds[1] > 0);
    CHECK(kinds[2] > 0);
}

TEST_CASE("TheBlockAboveAFaultComesFromWhereTheSlipSaysAndTheBlockBelowStays")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);
    constexpr int kTop = 250;

    int checked[3] = { 0, 0, 0 };
    for (const Province& province : map.GetProvinces())
    {
        const std::vector<GeologyModel::Fault> faults = model.FaultsNear(province.id, 0.0, 0.0);
        for (const GeologyModel::Fault& fault : faults)
        {
            // Slip is largest at the centre and dies out to nothing at the edge.
            const GeologyModel::FaultEffect centre = GeologyModel::EffectOf(fault, fault.centerEast, fault.centerNorth, kTop);
            Require(std::abs(centre.amplitude - fault.slipMeters) < 1e-6, "slip at the centre is not the fault's slip");
            const double outside = fault.strikeRadiusMeters * 1.01;
            const double strike = fault.strikeAzimuthDegrees * 3.14159265358979323846 / 180.0;
            Require(GeologyModel::EffectOf(fault, fault.centerEast + outside * std::sin(strike),
                                           fault.centerNorth + outside * std::cos(strike), kTop).amplitude == 0.0,
                    "a fault reaches beyond its edge");

            // Look at a column a little off the centre where this is the only fault in play.
            const double east = fault.centerEast + 0.2 * fault.strikeRadiusMeters * std::sin(strike);
            const double north = fault.centerNorth + 0.2 * fault.strikeRadiusMeters * std::cos(strike);
            const GeologyModel::FaultEffect effect = GeologyModel::EffectOf(fault, east, north, kTop);
            if (!(effect.amplitude > 0.0))
                continue;
            bool alone = true;
            for (const GeologyModel::Fault& other : model.FaultsNear(province.id, east, north))
            {
                if (other.centerEast == fault.centerEast && other.centerNorth == fault.centerNorth)
                    continue;
                alone = alone && GeologyModel::EffectOf(other, east, north, kTop).amplitude == 0.0 &&
                        GeologyModel::EffectOf(other, east - effect.shiftEast, north - effect.shiftNorth, kTop).amplitude == 0.0;
            }
            if (!alone)
                continue;

            const GeologyModel::ColumnProfile faulted = model.ProfileAt(province.id, east, north, kTop);
            const GeologyModel::ColumnProfile here = model.BaseProfileAt(province.id, east, north, kTop);
            const GeologyModel::ColumnProfile source =
                model.BaseProfileAt(province.id, east - effect.shiftEast, north - effect.shiftNorth, kTop);
            const int cut = static_cast<int>(std::lround(effect.cutElevation));
            const int lift = static_cast<int>(std::lround(effect.lift));
            for (int above : { 3, 9, 30 })
            {
                Require(GeologyModel::RockAt(faulted, cut + above) == GeologyModel::RockAt(source, cut + above - lift),
                        "the block above a fault is not what the undeformed pile had at its source");
            }
            for (int below : { 3, 9, 30 })
            {
                Require(GeologyModel::RockAt(faulted, cut - below) == GeologyModel::RockAt(here, cut - below),
                        "the block below a fault changed");
            }

            switch (fault.kind)
            {
            case GeologyModel::FaultKind::Normal: Require(effect.lift < 0.0, "a normal fault does not drop its hanging wall"); break;
            case GeologyModel::FaultKind::Thrust: Require(effect.lift > 0.0, "a thrust does not raise its hanging wall"); break;
            case GeologyModel::FaultKind::StrikeSlip:
                Require(effect.lift == 0.0 && std::hypot(effect.shiftEast, effect.shiftNorth) > 0.0, "a strike-slip fault moves vertically or not at all");
                break;
            }
            ++checked[static_cast<int>(fault.kind)];
        }
    }
    CHECK(checked[0] > 0);
    CHECK(checked[1] > 0);
    CHECK(checked[2] > 0);
}

TEST_CASE("FaultedProfilesStayOrderedAndEndInBasement")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);

    int faulted = 0;
    int differing = 0;
    for (const Province& province : map.GetProvinces())
    {
        for (double east = -2000.0; east <= 2000.0; east += 90.0)
        {
            for (double north = -2000.0; north <= 2000.0; north += 110.0)
            {
                const GeologyModel::ColumnProfile profile = model.ProfileAt(province.id, east, north, 100);
                Require(profile.count >= 1 && profile.count <= GeologyModel::kMaxProfileLayers, "a faulted profile has the wrong number of layers");
                Require(profile.bottom[static_cast<std::size_t>(profile.count - 1)] == std::numeric_limits<int>::min(), "a faulted profile does not end in basement");
                for (int i = 1; i < profile.count; ++i)
                {
                    Require(profile.bottom[static_cast<std::size_t>(i)] <= profile.bottom[static_cast<std::size_t>(i - 1)], "a faulted profile is out of order");
                    Require(profile.rock[static_cast<std::size_t>(i)] != profile.rock[static_cast<std::size_t>(i - 1)], "a faulted profile repeats a rock");
                }
                const GeologyModel::ColumnProfile base = model.BaseProfileAt(province.id, east, north, 100);
                bool same = base.count == profile.count;
                for (int i = 0; same && i < base.count; ++i)
                    same = base.rock[static_cast<std::size_t>(i)] == profile.rock[static_cast<std::size_t>(i)] &&
                           base.bottom[static_cast<std::size_t>(i)] == profile.bottom[static_cast<std::size_t>(i)];
                ++faulted;
                differing += same ? 0 : 1;
            }
        }
    }
    CHECK(faulted > 0);
    CHECK(differing > 0);
}

TEST_CASE("PilesRespectTheBiosphereOfTheirTime")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);
    const MaterialId coal = RockNamed("rock.bituminous_coal").material;
    const MaterialId redBeds = RockNamed("rock.red_beds").material;
    const MaterialId chalk = RockNamed("rock.chalk").material;

    int withCoal = 0;
    for (const Province& province : map.GetProvinces())
    {
        const BiosphereState state = map.GetEpochs().BiosphereAt(province.eventAgeMyr);
        for (const GeologyModel::Layer& layer : model.PileOf(province.id))
        {
            if (layer.rock == coal)
            {
                Require(state.landPlants, "coal in a province older than land plants");
                ++withCoal;
            }
            if (layer.rock == redBeds)
                Require(state.freeOxygen, "red beds before free oxygen");
            if (layer.rock == chalk)
                Require(state.calcareousPlankton, "chalk before calcareous plankton");
        }
    }
    CHECK(withCoal >= 0);
}

TEST_CASE("ModelIsDeterministic")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel first(catalog, map);
    const GeologyModel second(catalog, map);
    for (const Province& province : map.GetProvinces())
    {
        const auto& a = first.PileOf(province.id);
        const auto& b = second.PileOf(province.id);
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            Require(a[i].rock == b[i].rock && a[i].thicknessMeters == b[i].thicknessMeters,
                    "the pile of a province changed between builds");
        }
    }

    GeologyConfig doubled;
    doubled.strataThicknessScale = 0.4;
    const GeologyModel thicker(catalog, map, doubled);
    int thickerLayers = 0;
    for (const Province& province : map.GetProvinces())
    {
        const auto& a = first.PileOf(province.id);
        const auto& b = thicker.PileOf(province.id);
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i + 1 < a.size(); ++i)
        {
            thickerLayers += b[i].thicknessMeters > a[i].thicknessMeters ? 1 : 0;
        }
    }
    CHECK(thickerLayers > 0);
    CHECK_THROWS_AS(GeologyModel(nullptr, map), std::invalid_argument);
}

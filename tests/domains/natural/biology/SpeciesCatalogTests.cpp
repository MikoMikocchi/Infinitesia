#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "domains/natural/biology/catalog/SpeciesCatalog.h"
#include "matter/CatalogError.h"
#include "support/BrokenFile.h"
#include "support/MatterFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

namespace
{
const std::filesystem::path kSpeciesFile = kAssets / "biology" / "species.json";

// The message of the error loading a broken copy of species.json; empty when it
// loads.
std::string LoadError(const BrokenFile &broken)
{
	try
	{
		SpeciesCatalog::Load(broken.Path(), *SharedMatter());
	}
	catch (const CatalogError &error)
	{
		return error.what();
	}
	return {};
}

// Breaks the first species with `change` and returns the load error.
std::string ErrorAfter(const char *name, const std::function<void(Json &)> &change)
{
	BrokenFile broken(kSpeciesFile, name);
	broken.Edit([&](Json &root) { change(root.at("species").at(0)); });
	return LoadError(broken);
}
} // namespace

TEST_CASE("SpeciesCatalogLoadsTheShippedSpecies")
{
	const SpeciesCatalog catalog = SpeciesCatalog::Load(kSpeciesFile, *SharedMatter());
	REQUIRE(catalog.All().size() >= 3);

	const Species *oak = catalog.Find("species.oak");
	RequireFatal(oak != nullptr, "species.oak is missing");
	CHECK(oak->form == PlantForm::Broadleaf);
	CHECK(oak->heightMeters.min <= oak->heightMeters.max);
	CHECK(oak->matter.trunk == MatterNamed("oak.trunk").id);
	CHECK(oak->matter.leaves == MatterNamed("oak.leaves").id);

	CHECK(catalog.Find("species.spruce")->form == PlantForm::Conifer);
	CHECK(catalog.Find("species.palm")->crownShape == CrownShape::Umbrella);
	CHECK(catalog.Find("species.nothing") == nullptr);
}

TEST_CASE("PlantMatterCarryTheTagsTheGameAsksFor")
{
	for (const char *name : {"oak", "spruce", "palm"})
	{
		const std::string prefix = std::string(name) + ".";
		for (const char *part : {"trunk", "branch", "twig", "root"})
		{
			const MatterDef &wood = MatterNamed(prefix + part);
			CHECK(wood.blockClass == BlockClass::Plant);
			CHECK(!wood.walkable);
			CHECK(wood.opacity == LightRgb{kMaxLightLevel, kMaxLightLevel, kMaxLightLevel});
		}
		const MatterDef &leaves = MatterNamed(prefix + "leaves");
		CHECK(leaves.opacity.r < kMaxLightLevel);
		CHECK(leaves.blockClass == BlockClass::Plant);
	}
}

TEST_CASE("EverySpeciesHasASaneNiche")
{
	const SpeciesCatalog catalog = SpeciesCatalog::Load(kSpeciesFile, *SharedMatter());
	for (const Species &species : catalog.All())
	{
		const Trapezoid &temperature = species.niche.temperatureCelsius;
		CHECK_MESSAGE(temperature.lowest < temperature.optimumFrom, species.key);
		CHECK_MESSAGE(temperature.optimumTo < temperature.highest, species.key);
		CHECK_MESSAGE(species.niche.maxSlope > 0.0, species.key);
		CHECK_MESSAGE(species.crownStartFraction <= 1.0, species.key);
	}
}

TEST_CASE("SpeciesCatalogRejectsBrokenRecords")
{
	CHECK(ErrorAfter("species_bad_prefix", [](Json &s) { s["id"] = "tree.oak"; }).find("must start with 'species.'") !=
	      std::string::npos);
	CHECK(ErrorAfter("species_bad_form", [](Json &s) { s["form"] = "cactus"; }).find("unknown value 'cactus'") !=
	      std::string::npos);
	CHECK(ErrorAfter("species_bad_height", [](Json &s) { s["heightM"] = Json::array({20, 10}); }).find("heightM") !=
	      std::string::npos);
	CHECK(ErrorAfter("species_bad_niche", [](Json &s) {
		      s["niche"]["temperatureC"] = Json::array({5, 3, 10, 20});
	      }).find("temperatureC") != std::string::npos);
	CHECK(ErrorAfter("species_missing_niche", [](Json &s) { s.erase("niche"); }).find("missing field 'niche'") !=
	      std::string::npos);
	CHECK(ErrorAfter("species_bad_material", [](Json &s) {
		      s["matter"]["leaves"] = "granite";
	      }).find("not a plant material") != std::string::npos);
	CHECK(ErrorAfter("species_unknown_material", [](Json &s) {
		      s["matter"]["trunk"] = "plant.oak.nothing";
	      }).find("not a plant material") != std::string::npos);
	CHECK(ErrorAfter("species_wrong_part", [](Json &s) {
		      s["matter"]["leaves"] = "oak.trunk";
	      }).find("lacks the tag 'tree_leaves'") != std::string::npos);
	CHECK(ErrorAfter("species_bad_yield", [](Json &s) { s["yield"]["logsPerCubicM"] = 0; }).find("logsPerCubicM") !=
	      std::string::npos);
}

TEST_CASE("SpeciesCatalogRejectsRepeatedIdsAndEmptyFiles")
{
	BrokenFile repeated(kSpeciesFile, "species_repeated");
	repeated.Edit([](Json &root) { root.at("species").at(1)["id"] = root.at("species").at(0).at("id"); });
	CHECK(LoadError(repeated).find("used twice") != std::string::npos);

	BrokenFile empty(kSpeciesFile, "species_empty");
	empty.Edit([](Json &root) { root["species"] = Json::array(); });
	CHECK(LoadError(empty).find("no species") != std::string::npos);

	BrokenFile missing(kSpeciesFile, "species_missing");
	missing.Remove();
	CHECK(LoadError(missing).find("cannot open") != std::string::npos);
}

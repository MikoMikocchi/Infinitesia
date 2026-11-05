#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <set>
#include <string>
#include <utility>

#include "support/BrokenFile.h"
#include "support/MatterFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

namespace
{
bool HasTag(const MatterDef &material, const char *tag)
{
	return std::find(material.tags.begin(), material.tags.end(), tag) != material.tags.end();
}

// The message of the error loading a broken copy of matter.xml; empty when it
// loads.
std::string LoadError(const BrokenFile &broken)
{
	try
	{
		MatterCatalog::LoadFromFile(broken.Path());
	}
	catch (const CatalogError &error)
	{
		return error.what();
	}
	return {};
}

std::vector<const MatterDef *> OfClass(const MatterCatalog &catalog, BlockClass blockClass)
{
	std::vector<const MatterDef *> found;
	for (const MatterDef &material : catalog.Matter())
	{
		if (material.blockClass == blockClass)
			found.push_back(&material);
	}
	return found;
}
} // namespace

TEST_CASE("CatalogAssignsEachMaterialItsPlaceAsTheId")
{
	const MatterCatalog &catalog = *SharedMatter();
	CHECK(OfClass(catalog, BlockClass::Mineral).size() >= 160);
	CHECK(OfClass(catalog, BlockClass::Rock).size() >= 95);
	CHECK(catalog.Matter().size() >= 290);

	// The id is the record's place in the file (docs/DataDriven.md, section 4.2):
	// air first, then the fluids, then the rest in the order they are written.
	// The data names a material by its stable string id, never by this number.
	CHECK(catalog.Find("air")->id == kAirMatter);
	CHECK(catalog.Find("water")->id == kWaterMatter);
	for (std::size_t i = 0; i < catalog.Matter().size(); ++i)
	{
		Require(catalog.Matter()[i].id == static_cast<MatterId>(i), "the ids are not dense");
	}
	CHECK(catalog.Find("granite")->id > kWaterMatter);
	CHECK(catalog.Find("quartz")->id > kWaterMatter);

	std::set<MatterId> ids;
	std::set<std::string> keys;
	for (const MatterDef &material : catalog.Matter())
	{
		Require(ids.insert(material.id).second, "duplicate material id");
		Require(keys.insert(material.key).second, "duplicate material key");
		Require(catalog.Get(material.id) == &material, "material lookup by id is wrong");
		Require(catalog.Find(material.key) == &material, "material lookup by key is wrong");
	}
	CHECK(catalog.Get(kUnspecifiedMatter) == nullptr);
	CHECK(catalog.ColorOf(kUnspecifiedMatter) == RgbColor{255, 0, 255});
}

TEST_CASE("AMineralIsAMaterialLikeAnyOther")
{
	const MatterCatalog &catalog = *SharedMatter();
	const MatterDef &quartz = *catalog.Find("quartz");

	// The same record as granite or planks: density, colour, tags, light and
	// rules.
	CHECK(quartz.blockClass == BlockClass::Mineral);
	CHECK(HasTag(quartz, "mineral"));
	CHECK(quartz.density == doctest::Approx(2650.0));
	CHECK(quartz.opacity == LightRgb{15, 15, 15});
	CHECK(quartz.walkable);
	CHECK(quartz.diggable);
	CHECK(quartz.digClass >= 1);

	// What only some materials have lives in side tables, found by the same id.
	CHECK(catalog.ChemistryOf(quartz.id) != nullptr);
	CHECK(catalog.ModalOf(quartz.id) == nullptr);
	CHECK(catalog.ModalOf(catalog.Find("granite")->id) != nullptr);
	CHECK(catalog.ChemistryOf(catalog.Find("granite")->id) == nullptr);
	for (const char *plain : {"air", "water", "sand", "glacier", "planks"})
	{
		CHECK_MESSAGE(catalog.ChemistryOf(catalog.Find(plain)->id) == nullptr, plain);
		CHECK_MESSAGE(catalog.ModalOf(catalog.Find(plain)->id) == nullptr, plain);
	}

	// A mineral has the element fractions of its formula; a fluid has none.
	const auto quartzElements = catalog.ElementMassFractionsOf(quartz.id);
	CHECK(Fraction(quartzElements, "Si") == doctest::Approx(0.4674).epsilon(0.002));
	CHECK(Fraction(quartzElements, "O") == doctest::Approx(0.5326).epsilon(0.002));
	CHECK(catalog.ElementMassFractionsOf(catalog.Find("water")->id).Empty());
}

TEST_CASE("MaterialsCarryTagsAndLightProperties")
{
	const MatterCatalog &catalog = *SharedMatter();
	for (const MatterDef &material : catalog.Matter())
	{
		Require(HasTag(material, ClassTag(material.blockClass)), "a material lacks the tag of its class");
		for (const std::uint8_t level : {material.opacity.r, material.opacity.g, material.opacity.b,
		                                 material.emission.r, material.emission.g, material.emission.b})
		{
			Require(level <= kMaxLightLevel, "a light value is above 15");
		}
	}

	// Defaults follow the class: air passes light, water and ice dim it a little,
	// ground and rock stop it.
	CHECK(catalog.Find("air")->opacity == LightRgb{0, 0, 0});
	CHECK(catalog.Find("water")->opacity == LightRgb{2, 2, 2});
	CHECK(catalog.Find("glacier")->opacity == LightRgb{3, 3, 3});
	CHECK(catalog.Find("sand")->opacity == LightRgb{15, 15, 15});
	CHECK(catalog.Find("granite")->opacity == LightRgb{15, 15, 15});

	// Magma gives off orange light and blocks it like rock; the lamp glows
	// white-yellow.
	CHECK(catalog.Find("magma")->emission == LightRgb{14, 6, 1});
	CHECK(catalog.Find("magma")->opacity == LightRgb{15, 15, 15});
	CHECK(catalog.Find("oil")->emission == LightRgb{});
	CHECK(catalog.Find("lamp")->emission != LightRgb{});
	CHECK(catalog.Find("glass")->opacity == LightRgb{0, 0, 0});
	CHECK(catalog.Find("glass_red")->opacity.r < catalog.Find("glass_red")->opacity.g);

	// Grass-covered ground and the stand-in rock for terrains without geology
	// (docs/06_BLOCKS.md, section 4).
	const MatterDef &turf = *catalog.Find("turf");
	CHECK(turf.blockClass == BlockClass::Loose);
	CHECK(HasTag(turf, "soil"));
	CHECK(turf.digClass == 1);
	const MatterDef &bedrock = *catalog.Find("bedrock");
	CHECK(bedrock.blockClass == BlockClass::Rock);
	CHECK(bedrock.digClass == 4);
	CHECK(catalog.ModalOf(bedrock.id)->parts.empty());
	CHECK(catalog.Find("sand")->tags.size() == 2);

	// One water material for every water body; a fluid is a material like any
	// other, so brine, oil and magma need no separate water type.
	const MatterDef &water = *catalog.Find("water");
	CHECK(HasTag(water, "water"));
	CHECK(catalog.Find("seawater") == nullptr);
}

TEST_CASE("MaterialLightFieldsAreValidated")
{
	{
		const BrokenFile broken(kMatterFile, "infinitesia_materials_opacity");
		broken.EditXml([](pugi::xml_node root) {
			BrokenFile::RecordXml(root, "matter", "magma").attribute("opacity").set_value(16);
		});
		const std::string error = LoadError(broken);
		CHECK(error.find("opacity") != std::string::npos);
		CHECK(error.find("magma") != std::string::npos);
	}
	{
		// A tint is either one level or "r,g,b"; a two-part value is malformed.
		const BrokenFile broken(kMatterFile, "infinitesia_materials_emission");
		broken.EditXml([](pugi::xml_node root) {
			BrokenFile::RecordXml(root, "matter", "magma").attribute("emission").set_value("14,6");
		});
		CHECK(LoadError(broken).find("emission") != std::string::npos);
	}
	{
		const BrokenFile broken(kMatterFile, "infinitesia_materials_empty_tag");
		broken.EditXml([](pugi::xml_node root) {
			BrokenFile::RecordXml(root, "matter", "water").child("tags").append_child("tag");
		});
		CHECK(LoadError(broken).find("tag") != std::string::npos);
	}
	{
		// Red glass lets red through and stops green and blue.
		const BrokenFile broken(kMatterFile, "infinitesia_materials_tint");
		broken.EditXml([](pugi::xml_node root) {
			BrokenFile::RecordXml(root, "matter", "magma").attribute("opacity").set_value("1,8,8");
		});
		CHECK(LoadError(broken).empty());
	}
}

TEST_CASE("MineralMolarMassesMatchTheReference")
{
	// Reference molar masses (g/mol) of well known minerals; the catalog computes
	// its own from the formula and must agree within 0.1% (docs/03_GEOLOGY.md,
	// section 16).
	const std::pair<const char *, double> reference[] = {
	    {"quartz", 60.084},       {"calcite", 100.087},         {"hematite", 159.688}, {"magnetite", 231.533},
	    {"pyrite", 119.98},       {"halite", 58.443},           {"gypsum", 172.168},   {"galena", 239.27},
	    {"chalcopyrite", 183.54}, {"corundum", 101.961},        {"fluorite", 78.075},  {"graphite", 12.011},
	    {"native_gold", 196.967}, {"kaolinite", 258.16},        {"muscovite", 398.31}, {"orthoclase", 278.33},
	    {"albite", 262.22},       {"anorthite", 278.21},        {"sphalerite", 97.47}, {"cassiterite", 150.71},
	    {"malachite", 221.12},    {"dolomite_mineral", 184.40}, {"barite", 233.39},    {"rutile", 79.866},
	    {"diopside", 216.55},     {"spodumene", 186.09},        {"zircon", 183.31},
	};
	for (const auto &[key, expected] : reference)
	{
		const MatterDef &mineral = MatterNamed(key);
		const ChemistryInfo *chemistry = SharedMatter()->ChemistryOf(mineral.id);
		REQUIRE_MESSAGE(chemistry != nullptr, key);
		CHECK_MESSAGE(chemistry->molarMass == doctest::Approx(expected).epsilon(0.001), key);
	}
}

TEST_CASE("EveryMineralIsAConsistentCompositionWithPlausibleProperties")
{
	const MatterCatalog &catalog = *SharedMatter();
	for (const MatterDef *mineral : OfClass(catalog, BlockClass::Mineral))
	{
		const ChemistryInfo *chemistry = catalog.ChemistryOf(mineral->id);
		RequireFatal(chemistry != nullptr, "a mineral has no chemistry");
		Require(!chemistry->composition.Empty(), "mineral has an empty composition");
		Require(chemistry->molarMass > 1.0, "mineral molar mass is implausible");
		Require(std::abs(chemistry->composition.MassFractions().Sum() - 1.0) < 1e-9,
		        "mineral element fractions do not sum to 1");
		Require(mineral->density >= 900.0 && mineral->density <= 22000.0, "mineral density out of range");
		Require(chemistry->mohs >= 1.0 && chemistry->mohs <= 10.0, "mineral hardness out of range");
		Require(mineral->digClass >= 1 && mineral->digClass <= 5, "mineral dig class out of range");
		// Atoms of every element are accounted for: no element beyond uranium in a
		// mineral.
		for (const Chemistry::Composition::Entry &entry : chemistry->composition.Entries())
		{
			Require(entry.atomicNumber <= 92, "mineral contains a transuranic element");
		}
	}
}

TEST_CASE("RocksAreMixturesOfMinerals")
{
	const MatterCatalog &catalog = *SharedMatter();
	int filled = 0;
	for (const MatterDef *rock : OfClass(catalog, BlockClass::Rock))
	{
		Require(rock->density >= 700.0 && rock->density <= 4000.0, "rock density out of range");
		const ModalComposition *modal = catalog.ModalOf(rock->id);
		RequireFatal(modal != nullptr, "a rock has no modal composition record");
		if (modal->parts.empty())
			continue;
		++filled;
		double volume = 0.0;
		double crystallineDensity = 0.0;
		for (const ModalEntry &entry : modal->parts)
		{
			volume += entry.fraction;
			crystallineDensity += entry.fraction * catalog.Get(entry.mineral)->density;
		}
		Require(std::abs(volume - 1.0) < 1e-3, "modal fractions do not sum to 1");

		const Chemistry::ElementMassFractions elements = catalog.ElementMassFractionsOf(rock->id);
		Require(std::abs(elements.Sum() - 1.0) < 1e-9, "rock element fractions do not sum to 1");

		// A crystalline rock is as dense as its minerals; sedimentary rocks are
		// porous, so their bulk density can only be lower.
		const bool crystalline = modal->rockClass == RockClass::IgneousIntrusive ||
		                         modal->rockClass == RockClass::IgneousExtrusive ||
		                         modal->rockClass == RockClass::Metamorphic;
		if (crystalline)
		{
			const bool close = std::abs(crystallineDensity - rock->density) / rock->density < 0.12;
			CHECK_MESSAGE(close, rock->key, ": stated ", rock->density, ", minerals give ", crystallineDensity);
		}
		else
		{
			Require(crystallineDensity >= rock->density * 0.95, "a porous rock is denser than its minerals");
		}
	}
	CHECK(filled >= 70);
}

TEST_CASE("RockChemistryFollowsFromItsMinerals")
{
	const MatterCatalog &catalog = *SharedMatter();
	const auto elements = [&](const char *key) { return catalog.ElementMassFractionsOf(MatterNamed(key).id); };

	const auto granite = elements("granite");
	const auto basalt = elements("basalt");
	const auto peridotite = elements("peridotite");
	const auto limestone = elements("limestone");
	const auto quartzite = elements("quartzite");
	const auto rockSalt = elements("rock_salt");

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
	CHECK(elements("anthracite").Empty());
}

TEST_CASE("LoaderRejectsBrokenFilesWithAMessageNamingTheRecord")
{
	const auto check = [](const char *name, const std::function<void(pugi::xml_node)> &change, const char *inError,
	                      const char *inRecord) {
		const BrokenFile broken(kMatterFile, name);
		broken.EditXml(change);
		const std::string error = LoadError(broken);
		CHECK_MESSAGE(error.find(inError) != std::string::npos, name, ": ", error);
		if (inRecord != nullptr)
			CHECK_MESSAGE(error.find(inRecord) != std::string::npos, name, ": ", error);
	};
	const auto record = [](pugi::xml_node root, const char *key) { return BrokenFile::RecordXml(root, "matter", key); };
	const auto part = [](pugi::xml_node rock, int index) {
		pugi::xml_node node = rock.child("part");
		for (int i = 0; i < index; ++i)
			node = node.next_sibling("part");
		return node;
	};

	check(
	    "infinitesia_mat_formula",
	    [&](pugi::xml_node r) { record(r, "quartz").child("chemistry").attribute("formula").set_value("Si(O2"); },
	    "Invalid formula", "quartz");
	check(
	    "infinitesia_mat_duplicate",
	    [&](pugi::xml_node r) { record(r, "calcite").attribute("id").set_value("quartz"); }, "duplicate id", "quartz");
	check(
	    "infinitesia_mat_density", [&](pugi::xml_node r) { record(r, "granite").attribute("density").set_value(100); },
	    "density", "granite");
	check(
	    "infinitesia_mat_modal",
	    [&](pugi::xml_node r) {
		    part(record(r, "granite").child("rock"), 1).attribute("mineral").set_value("nothing");
	    },
	    "unknown mineral", "granite");
	check(
	    "infinitesia_mat_modal_rock",
	    [&](pugi::xml_node r) { part(record(r, "granite").child("rock"), 1).attribute("mineral").set_value("basalt"); },
	    "not a mineral", "granite");
	check(
	    "infinitesia_mat_sum",
	    [&](pugi::xml_node r) { part(record(r, "granite").child("rock"), 0).attribute("fraction").set_value("0.5"); },
	    "sum", "granite");
	check(
	    "infinitesia_mat_color",
	    [&](pugi::xml_node r) { record(r, "granite").attribute("color").set_value("granite"); }, "colour", nullptr);
	check(
	    "infinitesia_mat_nochem", [&](pugi::xml_node r) { record(r, "quartz").remove_child("chemistry"); }, "chemistry",
	    "quartz");
	check(
	    "infinitesia_mat_norock", [&](pugi::xml_node r) { record(r, "granite").remove_child("rock"); }, "rock",
	    "granite");
	check(
	    "infinitesia_mat_rockpart",
	    [&](pugi::xml_node r) {
		    record(r, "quartz").append_child("rock").append_attribute("class").set_value("metamorphic");
	    },
	    "rock", "quartz");
	check(
	    "infinitesia_mat_class", [&](pugi::xml_node r) { record(r, "granite").attribute("class").set_value("plasma"); },
	    "unknown material class", nullptr);
	check(
	    "infinitesia_mat_digclass", [&](pugi::xml_node r) { record(r, "granite").attribute("digClass").set_value(9); },
	    "digClass", nullptr);
	{
		const BrokenFile broken(kMatterFile, "infinitesia_mat_nowater");
		broken.EditXml([&](pugi::xml_node r) { record(r, "water").attribute("class").set_value("loose"); });
		CHECK(!LoadError(broken).empty());
	}
	{
		const BrokenFile broken(kMatterFile, "infinitesia_mat_xml");
		broken.WriteText("<catalog><matter id=\"quartz\"");
		CHECK(LoadError(broken).find("matter.xml") != std::string::npos);
	}
	{
		const BrokenFile broken(kMatterFile, "infinitesia_mat_missing");
		broken.Remove();
		CHECK(LoadError(broken).find("Cannot open") != std::string::npos);
	}
}

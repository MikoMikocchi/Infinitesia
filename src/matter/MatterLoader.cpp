#include "matter/MatterLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "content/xml/XmlDocument.h"
#include "content/xml/XmlRead.h"
#include "core/ContentRegistry.h"
#include "matter/CatalogError.h"

namespace Infinitesia
{
namespace
{
using Content::Xml::Where;

BlockClass MaterialClass(const std::string &text, const Where &where)
{
	static constexpr std::pair<const char *, BlockClass> kNames[] = {
	    {"air", BlockClass::Air},         {"fluid", BlockClass::Fluid}, {"loose", BlockClass::Loose},
	    {"ice", BlockClass::Ice},         {"rock", BlockClass::Rock},   {"built", BlockClass::Built},
	    {"mineral", BlockClass::Mineral}, {"plant", BlockClass::Plant},
	};
	for (const auto &[name, value] : kNames)
	{
		if (text == name)
			return value;
	}
	Content::Xml::Fail(where, "unknown material class '" + text + "'");
}

RockClass RockClassOf(const std::string &text, const Where &where)
{
	static constexpr std::pair<const char *, RockClass> kNames[] = {
	    {"igneous_intrusive", RockClass::IgneousIntrusive},
	    {"igneous_extrusive", RockClass::IgneousExtrusive},
	    {"volcaniclastic", RockClass::Volcaniclastic},
	    {"sedimentary_clastic", RockClass::SedimentaryClastic},
	    {"sedimentary_chemical", RockClass::SedimentaryChemical},
	    {"sedimentary_biogenic", RockClass::SedimentaryBiogenic},
	    {"metamorphic", RockClass::Metamorphic},
	};
	for (const auto &[name, value] : kNames)
	{
		if (text == name)
			return value;
	}
	Content::Xml::Fail(where, "unknown rock class '" + text + "'");
}

Provenance ProvenanceOf(const std::string &text, const Where &where)
{
	if (text == "reference")
		return Provenance::Reference;
	if (text == "derived")
		return Provenance::Derived;
	if (text == "gameplay")
		return Provenance::Gameplay;
	Content::Xml::Fail(where, "provenance must be reference, derived or gameplay, got '" + text + "'");
}

std::uint8_t LightLevel(const std::string &text, const char *name, const Where &where)
{
	char *end = nullptr;
	const long level = std::strtol(text.c_str(), &end, 10);
	if (end == text.c_str() || *end != '\0' || level < 0 || level > kMaxLightLevel)
	{
		Content::Xml::Fail(where, std::string("attribute '") + name + "' must be 0-15, got '" + text + "'");
	}
	return static_cast<std::uint8_t>(level);
}

// A light value: one level (all three channels equal, opacity only) or
// "r,g,b". Absent gives `fallback`.
LightRgb LightAttribute(const pugi::xml_node &node, const char *name, const Where &where, LightRgb fallback,
                        bool allowNumber)
{
	const std::string text = Content::Xml::OptionalAttribute(node, name, "");
	if (text.empty())
		return fallback;
	std::vector<std::string> parts;
	std::size_t start = 0;
	for (;;)
	{
		const std::size_t comma = text.find(',', start);
		parts.push_back(text.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
		if (comma == std::string::npos)
			break;
		start = comma + 1;
	}
	if (allowNumber && parts.size() == 1)
	{
		const std::uint8_t level = LightLevel(text, name, where);
		return {level, level, level};
	}
	if (parts.size() != 3)
	{
		Content::Xml::Fail(where, std::string("attribute '") + name + "' must be " +
		                              (allowNumber ? "a number or " : "") + "\"r,g,b\"");
	}
	return {LightLevel(parts[0], name, where), LightLevel(parts[1], name, where), LightLevel(parts[2], name, where)};
}

int HexDigit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

RgbColor ColorAttribute(const pugi::xml_node &node, const Where &where)
{
	const std::string text = Content::Xml::Attribute(node, "color", where);
	if (text.size() != 7 || text[0] != '#')
	{
		Content::Xml::Fail(where, "colour must look like #rrggbb, got '" + text + "'");
	}
	int channel[3];
	for (int i = 0; i < 3; ++i)
	{
		const int high = HexDigit(text[static_cast<std::size_t>(1 + 2 * i)]);
		const int low = HexDigit(text[static_cast<std::size_t>(2 + 2 * i)]);
		if (high < 0 || low < 0)
		{
			Content::Xml::Fail(where, "colour must look like #rrggbb, got '" + text + "'");
		}
		channel[i] = high * 16 + low;
	}
	return {static_cast<std::uint8_t>(channel[0]), static_cast<std::uint8_t>(channel[1]),
	        static_cast<std::uint8_t>(channel[2])};
}

// The tags of a material: the tag of its class plus the ones in <tags>.
std::vector<std::string> TagsOf(const pugi::xml_node &material, BlockClass blockClass, const Where &where)
{
	std::vector<std::string> tags{ClassTag(blockClass)};
	const pugi::xml_node list = material.child("tags");
	if (!list)
		return tags;
	for (const pugi::xml_node tag : list.children("tag"))
	{
		const std::string text = Content::Xml::Text(tag);
		if (text.empty())
		{
			Content::Xml::Fail(where, "a <tag> must hold a non-empty id");
		}
		if (std::find(tags.begin(), tags.end(), text) == tags.end())
		{
			tags.push_back(text);
		}
	}
	return tags;
}

ChemistryInfo ReadChemistry(const pugi::xml_node &node, const Where &where)
{
	ChemistryInfo chemistry;
	chemistry.formula = Content::Xml::Attribute(node, "formula", where);
	std::string error;
	if (!Chemistry::Composition::TryParse(chemistry.formula, chemistry.composition, &error))
	{
		Content::Xml::Fail(where, error);
	}
	chemistry.molarMass = chemistry.composition.MolarMass();
	chemistry.mohs = Content::Xml::NumberAttribute(node, "mohs", where);
	if (!(chemistry.mohs >= 0.0 && chemistry.mohs <= 10.0))
	{
		Content::Xml::Fail(where, "Mohs hardness must be 0-10");
	}
	chemistry.solidSolution = Content::Xml::OptionalBoolAttribute(node, "solidSolution", where, false);
	return chemistry;
}

// A rock's modal composition as the data writes it: the minerals are still
// named by their string id; the catalog resolves them while assembling.
MatterCatalog::PendingModal ReadModal(const pugi::xml_node &rock, const Where &where)
{
	MatterCatalog::PendingModal modal;
	modal.rockClass = RockClassOf(Content::Xml::Attribute(rock, "class", where), where);
	double sum = 0.0;
	std::set<std::string> used;
	for (const pugi::xml_node part : rock.children("part"))
	{
		const std::string mineral = Content::Xml::Attribute(part, "mineral", where);
		if (!used.insert(mineral).second)
		{
			Content::Xml::Fail(where, "modal composition lists '" + mineral + "' twice");
		}
		const double fraction = Content::Xml::NumberAttribute(part, "fraction", where);
		if (!(fraction > 0.0 && fraction <= 1.0))
		{
			Content::Xml::Fail(where, "modal fraction of '" + mineral + "' must be in (0, 1]");
		}
		sum += fraction;
		modal.parts.emplace_back(mineral, fraction);
	}
	if (!modal.parts.empty() && std::abs(sum - 1.0) > 1.0e-3)
	{
		Content::Xml::Fail(where, "modal fractions sum to " + std::to_string(sum) + ", not 1");
	}
	return modal;
}

// Reads matter.xml; throws ContentError, which the entry point turns into the
// CatalogError its callers expect.
void ReadMaterialFile(const std::filesystem::path &path, std::vector<MatterDef> &materials,
                      MatterCatalog::ChemistryByKey &chemistry, MatterCatalog::ModalByKey &modal)
{
	const Content::XmlDocument document = Content::XmlDocument::LoadFromFile(path);
	const std::string file = path.string();

	std::set<std::string> keys;
	for (const pugi::xml_node entry : document.Root().children("matter"))
	{
		const Where head{file, document.LineOf(entry), ""};
		MatterDef material;
		material.key = Content::Xml::Attribute(entry, "id", head);
		const Where record{file, head.line, material.key};
		material.blockClass = MaterialClass(Content::Xml::Attribute(entry, "class", record), record);
		if (!keys.insert(material.key).second)
		{
			Content::Xml::Fail(record, "duplicate id");
		}

		material.name = Content::Xml::Attribute(entry, "name", record);
		material.density = Content::Xml::NumberAttribute(entry, "density", record);
		if (material.blockClass == BlockClass::Rock)
		{
			if (!(material.density >= 500.0 && material.density <= 6000.0))
			{
				Content::Xml::Fail(record, "density must be 500-6000 kg/m3");
			}
		}
		else if (material.blockClass == BlockClass::Mineral)
		{
			if (!(material.density >= 500.0 && material.density <= 25000.0))
			{
				Content::Xml::Fail(record, "density must be 500-25000 kg/m3");
			}
		}
		else if (!(material.density > 0.0))
		{
			Content::Xml::Fail(record, "density must be positive");
		}

		material.digClass = Content::Xml::IntAttribute(entry, "digClass", record);
		const bool needsDigClass =
		    material.blockClass == BlockClass::Rock || material.blockClass == BlockClass::Mineral;
		if (material.digClass < (needsDigClass ? 1 : 0) || material.digClass > 5)
		{
			Content::Xml::Fail(record, needsDigClass ? "digClass must be 1-5" : "digClass must be 0-5");
		}

		material.color = ColorAttribute(entry, record);
		material.provenance = ProvenanceOf(Content::Xml::Attribute(entry, "provenance", record), record);
		material.tags = TagsOf(entry, material.blockClass, record);
		material.opacity = LightAttribute(entry, "opacity", record, DefaultOpacity(material.blockClass), true);
		material.emission = LightAttribute(entry, "emission", record, LightRgb{}, false);
		const bool solid = material.blockClass != BlockClass::Air;
		material.walkable = Content::Xml::OptionalBoolAttribute(entry, "walkable", record, solid);
		material.diggable = Content::Xml::OptionalBoolAttribute(entry, "diggable", record, solid);

		const pugi::xml_node chemistryPart = entry.child("chemistry");
		if (material.blockClass == BlockClass::Mineral && !chemistryPart)
		{
			Content::Xml::Fail(record, "a mineral needs a 'chemistry' part");
		}
		if (chemistryPart)
		{
			chemistry.emplace(material.key, ReadChemistry(chemistryPart, record));
		}

		const pugi::xml_node rockPart = entry.child("rock");
		if (material.blockClass == BlockClass::Rock && !rockPart)
		{
			Content::Xml::Fail(record, "a rock needs a 'rock' part");
		}
		if (rockPart)
		{
			if (material.blockClass != BlockClass::Rock)
			{
				Content::Xml::Fail(record, "only a rock may have a 'rock' part");
			}
			modal.emplace(material.key, ReadModal(rockPart, record));
		}

		materials.push_back(std::move(material));
	}
}
} // namespace

void LoadMaterialFile(const std::filesystem::path &path, std::vector<MatterDef> &materials,
                      MatterCatalog::ChemistryByKey &chemistry, MatterCatalog::ModalByKey &modal)
{
	try
	{
		ReadMaterialFile(path, materials, chemistry, modal);
	}
	catch (const ContentError &error)
	{
		// The matter catalog reports CatalogError to its callers (MatterLoader.h);
		// the content layer underneath speaks ContentError.
		throw CatalogError(error.what());
	}
}
} // namespace Infinitesia

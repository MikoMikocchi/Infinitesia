#include "gameplay/content/GameContent.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace Infinitesia
{
namespace
{
using Json = nlohmann::json;

[[noreturn]] void Fail(const std::string &where, const std::string &message)
{
	throw ContentError(where + ": " + message);
}

Json ReadArray(const std::filesystem::path &path, const char *arrayName)
{
	std::ifstream file(path);
	if (!file)
	{
		Fail(path.string(), "cannot open the file");
	}
	Json root = Json::parse(file, nullptr, false);
	if (root.is_discarded())
	{
		Fail(path.string(), "not valid JSON");
	}
	if (!root.is_object() || !root.contains(arrayName) || !root[arrayName].is_array())
	{
		Fail(path.string(), std::string("expected an object with a '") + arrayName + "' array");
	}
	return root[arrayName];
}

std::string RecordName(const std::filesystem::path &path, const Json &record, std::size_t index)
{
	const std::string id = record.is_object() && record.contains("id") && record["id"].is_string()
	                           ? record["id"].get<std::string>()
	                           : "#" + std::to_string(index);
	return path.filename().string() + " record " + id;
}

std::string RequireString(const std::string &where, const Json &record, const char *field)
{
	if (!record.is_object() || !record.contains(field) || !record[field].is_string() ||
	    record[field].get<std::string>().empty())
	{
		Fail(where, std::string("'") + field + "' must be a non-empty string");
	}
	return record[field].get<std::string>();
}

ItemVisual ParseItemVisual(const std::string &where, const std::string &text)
{
	if (text == "generic")
	{
		return ItemVisual::Generic;
	}
	if (text == "logs")
	{
		return ItemVisual::Logs;
	}
	Fail(where, "unknown item visual '" + text + "'");
}

ObjectVisual ParseObjectVisual(const std::string &where, const std::string &text)
{
	if (text == "worker")
	{
		return ObjectVisual::Worker;
	}
	if (text == "item")
	{
		return ObjectVisual::Item;
	}
	Fail(where, "unknown object visual '" + text + "'");
}

std::uint8_t ParseAbility(const std::string &where, const std::string &text)
{
	if (text == "worker")
	{
		return static_cast<std::uint8_t>(ObjectAbility::Worker);
	}
	if (text == "pickable")
	{
		return static_cast<std::uint8_t>(ObjectAbility::Pickable);
	}
	Fail(where, "unknown ability '" + text + "'");
}

void LoadItems(const std::filesystem::path &path, ContentRegistry<ItemDef> &items)
{
	const Json records = ReadArray(path, "items");
	for (std::size_t index = 0; index < records.size(); ++index)
	{
		const Json &record = records[index];
		const std::string where = RecordName(path, record, index);
		ItemDef def;
		def.key = RequireString(where, record, "id");
		def.name = RequireString(where, record, "name");
		def.maxStack = record.value("maxStack", 64);
		if (def.maxStack < 1)
		{
			Fail(where, "'maxStack' must be at least 1");
		}
		def.visual = ParseItemVisual(where, record.value("visual", std::string("generic")));
		try
		{
			items.Add(std::move(def));
		}
		catch (const ContentError &error)
		{
			Fail(where, error.what());
		}
	}
}

void LoadObjectKinds(const std::filesystem::path &path, ContentRegistry<ObjectKindDef> &kinds)
{
	const Json records = ReadArray(path, "objects");
	for (std::size_t index = 0; index < records.size(); ++index)
	{
		const Json &record = records[index];
		const std::string where = RecordName(path, record, index);
		ObjectKindDef def;
		def.key = RequireString(where, record, "id");
		def.name = RequireString(where, record, "name");
		if (!record.contains("size") || !record["size"].is_array() || record["size"].size() != 2 ||
		    !record["size"][0].is_number() || !record["size"][1].is_number())
		{
			Fail(where, "'size' must be [width, height] in cells");
		}
		def.widthInCells = record["size"][0].get<float>();
		def.heightInCells = record["size"][1].get<float>();
		if (def.widthInCells <= 0.0f || def.heightInCells <= 0.0f)
		{
			Fail(where, "'size' must be positive");
		}
		def.visual = ParseObjectVisual(where, RequireString(where, record, "visual"));
		def.carriesItem = record.value("carriesItem", false);
		def.shadowOpacity = record.value("shadowOpacity", 110);
		if (def.shadowOpacity < 0 || def.shadowOpacity > 255)
		{
			Fail(where, "'shadowOpacity' must be 0..255");
		}
		if (record.contains("abilities"))
		{
			if (!record["abilities"].is_array())
			{
				Fail(where, "'abilities' must be an array");
			}
			for (const Json &ability : record["abilities"])
			{
				if (!ability.is_string())
				{
					Fail(where, "'abilities' must hold strings");
				}
				def.abilities |= ParseAbility(where, ability.get<std::string>());
			}
		}
		try
		{
			kinds.Add(std::move(def));
		}
		catch (const ContentError &error)
		{
			Fail(where, error.what());
		}
	}
}
} // namespace

std::shared_ptr<const GameContent> GameContent::LoadFromDirectory(const std::filesystem::path &directory)
{
	std::shared_ptr<GameContent> content(new GameContent());
	LoadItems(directory / "items.json", content->m_items);
	LoadObjectKinds(directory / "objects.json", content->m_objectKinds);
	return content;
}
} // namespace Infinitesia

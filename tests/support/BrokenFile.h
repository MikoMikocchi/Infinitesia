#pragma once

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <pugixml.hpp>
#include <string>

// A copy of a data file in a fresh temporary directory, to be broken on
// purpose. Edits go through the parsed tree (JSON or XML), so they do not
// depend on how the file happens to be formatted.
namespace InfinitesiaTests
{
using Json = nlohmann::json;

class BrokenFile
{
public:
	BrokenFile(const std::filesystem::path &original, const char *name)
	    : m_directory(std::filesystem::temp_directory_path() / name), m_path(m_directory / original.filename())
	{
		std::filesystem::remove_all(m_directory);
		std::filesystem::create_directories(m_directory);
		std::filesystem::copy_file(original, m_path);
	}
	~BrokenFile()
	{
		std::filesystem::remove_all(m_directory);
	}
	BrokenFile(const BrokenFile &) = delete;
	BrokenFile &operator=(const BrokenFile &) = delete;

	const std::filesystem::path &Path() const
	{
		return m_path;
	}

	void Edit(const std::function<void(Json &)> &change) const
	{
		std::ifstream in(m_path);
		Json root = Json::parse(in);
		in.close();
		change(root);
		std::ofstream(m_path, std::ios::trunc) << root.dump(2);
	}

	// The XML counterpart of Edit: parses the file, lets `change` rewrite the
	// tree and writes it back.
	void EditXml(const std::function<void(pugi::xml_node)> &change) const
	{
		pugi::xml_document document;
		const pugi::xml_parse_result result = document.load_file(m_path.c_str());
		REQUIRE_MESSAGE(result, "cannot parse ", m_path.string(), ": ", result.description());
		change(document.document_element());
		document.save_file(m_path.c_str());
	}

	void WriteText(const std::string &text) const
	{
		std::ofstream(m_path, std::ios::trunc) << text;
	}
	void Remove() const
	{
		std::filesystem::remove(m_path);
	}

	// The record of `array` whose "id" is `key`.
	static Json &Record(Json &root, const char *array, const std::string &key)
	{
		for (Json &record : root.at(array))
		{
			if (record.at("id") == key)
				return record;
		}
		FAIL("test record not found: " << key);
		return root;
	}

	// The <element> of `root` whose "id" attribute is `key`.
	static pugi::xml_node RecordXml(pugi::xml_node root, const char *element, const std::string &key)
	{
		for (pugi::xml_node record : root.children(element))
		{
			if (key == record.attribute("id").value())
				return record;
		}
		FAIL("test record not found: " << key);
		return {};
	}

private:
	std::filesystem::path m_directory;
	std::filesystem::path m_path;
};
} // namespace InfinitesiaTests

#pragma once

#include <filesystem>
#include <string>

#include <pugixml.hpp>

namespace Infinitesia::Content
{
// One XML content file parsed into memory. It owns the tree; the nodes handed
// out by Root stay valid for as long as the document lives and is not moved.
class XmlDocument
{
public:
	// Reads `path` and parses it. Throws ContentError, naming the file and the
	// line, when the file cannot be read or is not well-formed XML.
	static XmlDocument LoadFromFile(const std::filesystem::path &path);

	// Parses `text` as if it came from a file called `name` (tests, embedded
	// content).
	static XmlDocument Parse(const std::string &text, const std::string &name);

	// The root element; an invalid node when the document is empty.
	pugi::xml_node Root() const noexcept
	{
		return m_doc.document_element();
	}

	// The file the document came from, for diagnostics.
	const std::string &Name() const noexcept
	{
		return m_name;
	}

	// The line `node` starts on, or 0 when the parser did not keep it.
	int LineOf(const pugi::xml_node &node) const noexcept;

private:
	pugi::xml_document m_doc;
	std::string m_name;
	std::string m_text;
};
} // namespace Infinitesia::Content

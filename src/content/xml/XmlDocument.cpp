#include "content/xml/XmlDocument.h"

#include <cstddef>
#include <fstream>
#include <sstream>

#include "core/ContentRegistry.h"

namespace Infinitesia::Content
{
namespace
{
// The 1-based line the byte `offset` falls on.
int LineAt(const std::string &text, std::ptrdiff_t offset)
{
	if (offset < 0)
		return 0;
	const std::size_t end =
	    static_cast<std::size_t>(offset) > text.size() ? text.size() : static_cast<std::size_t>(offset);
	int line = 1;
	for (std::size_t i = 0; i < end; ++i)
	{
		if (text[i] == '\n')
			++line;
	}
	return line;
}
} // namespace

XmlDocument XmlDocument::LoadFromFile(const std::filesystem::path &path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
	{
		throw ContentError("Cannot open content file '" + path.string() + "'");
	}
	std::ostringstream buffer;
	buffer << stream.rdbuf();
	return Parse(buffer.str(), path.string());
}

XmlDocument XmlDocument::Parse(const std::string &text, const std::string &name)
{
	XmlDocument document;
	document.m_name = name;
	// The text is kept first, so the parser reads from this document's own copy
	// and the line numbers can be counted later.
	document.m_text = text;
	const pugi::xml_parse_result result = document.m_doc.load_string(document.m_text.c_str());
	if (!result)
	{
		throw ContentError(name + ":" + std::to_string(LineAt(text, result.offset)) + ": " + result.description());
	}
	return document;
}

int XmlDocument::LineOf(const pugi::xml_node &node) const noexcept
{
	const std::ptrdiff_t offset = node.offset_debug();
	return offset < 0 ? 0 : LineAt(m_text, offset);
}
} // namespace Infinitesia::Content

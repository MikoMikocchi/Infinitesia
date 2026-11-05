#pragma once

#include <pugixml.hpp>

#include <string>

namespace Infinitesia::Content::Xml
{
// Where a reader is: the file, the line and the record, so an error names the
// exact place in the data (docs/DataDriven.md, section 4.6). A line of 0 means
// the parser did not keep it.
struct Where
{
	std::string file;
	int line = 0;
	std::string record;
};

[[noreturn]] void Fail(const Where &where, const std::string &what);

// Required attribute; Fail when it is absent or holds the wrong kind of value.
std::string Attribute(const pugi::xml_node &node, const char *name, const Where &where);
double NumberAttribute(const pugi::xml_node &node, const char *name, const Where &where);
int IntAttribute(const pugi::xml_node &node, const char *name, const Where &where);
bool BoolAttribute(const pugi::xml_node &node, const char *name, const Where &where);

// Optional attribute; `fallback` when it is absent. A string value cannot be
// malformed, so this one needs no place.
std::string OptionalAttribute(const pugi::xml_node &node, const char *name, const std::string &fallback);
// Optional attribute; `fallback` when it is absent. A value that is present but
// malformed still fails, naming `where`.
double OptionalNumberAttribute(const pugi::xml_node &node, const char *name, const Where &where, double fallback);
int OptionalIntAttribute(const pugi::xml_node &node, const char *name, const Where &where, int fallback);
bool OptionalBoolAttribute(const pugi::xml_node &node, const char *name, const Where &where, bool fallback);

// The child element `name`; Fail when it is absent.
pugi::xml_node Child(const pugi::xml_node &parent, const char *name, const Where &where);
// The child element `name`, or an invalid node when it is absent.
pugi::xml_node OptionalChild(const pugi::xml_node &parent, const char *name) noexcept;

// The text of a leaf element, trimmed of surrounding whitespace.
std::string Text(const pugi::xml_node &node);

bool StartsWith(const std::string &text, const char *prefix) noexcept;
} // namespace Infinitesia::Content::Xml

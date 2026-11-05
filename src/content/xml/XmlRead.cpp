#include "content/xml/XmlRead.h"

#include <cerrno>
#include <climits>
#include <cstdlib>

#include "core/ContentRegistry.h"

namespace Infinitesia::Content::Xml
{
[[noreturn]] void Fail(const Where &where, const std::string &what)
{
	std::string message = where.file.empty() ? "content" : where.file;
	if (where.line > 0)
		message += ":" + std::to_string(where.line);
	if (!where.record.empty())
		message += ": '" + where.record + "'";
	throw ContentError(message + ": " + what);
}

namespace
{
double ParseNumber(const std::string &text, const char *name, const Where &where)
{
	errno = 0;
	char *end = nullptr;
	const double value = std::strtod(text.c_str(), &end);
	if (end == text.c_str() || *end != '\0' || errno == ERANGE)
	{
		Fail(where, std::string("attribute '") + name + "' must be a number, got '" + text + "'");
	}
	return value;
}

int ParseInt(const std::string &text, const char *name, const Where &where)
{
	errno = 0;
	char *end = nullptr;
	const long value = std::strtol(text.c_str(), &end, 10);
	if (end == text.c_str() || *end != '\0' || errno == ERANGE || value < INT_MIN || value > INT_MAX)
	{
		Fail(where, std::string("attribute '") + name + "' must be a whole number, got '" + text + "'");
	}
	return static_cast<int>(value);
}

bool ParseBool(const std::string &text, const char *name, const Where &where)
{
	if (text == "true" || text == "1")
		return true;
	if (text == "false" || text == "0")
		return false;
	Fail(where, std::string("attribute '") + name + "' must be true or false, got '" + text + "'");
}

std::string Trimmed(const std::string &text)
{
	const std::size_t first = text.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return {};
	const std::size_t last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}
} // namespace

std::string Attribute(const pugi::xml_node &node, const char *name, const Where &where)
{
	const pugi::xml_attribute attribute = node.attribute(name);
	if (!attribute)
		Fail(where, std::string("missing attribute '") + name + "'");
	return attribute.value();
}

double NumberAttribute(const pugi::xml_node &node, const char *name, const Where &where)
{
	return ParseNumber(Attribute(node, name, where), name, where);
}

int IntAttribute(const pugi::xml_node &node, const char *name, const Where &where)
{
	return ParseInt(Attribute(node, name, where), name, where);
}

bool BoolAttribute(const pugi::xml_node &node, const char *name, const Where &where)
{
	return ParseBool(Attribute(node, name, where), name, where);
}

std::string OptionalAttribute(const pugi::xml_node &node, const char *name, const std::string &fallback)
{
	const pugi::xml_attribute attribute = node.attribute(name);
	return attribute ? std::string(attribute.value()) : fallback;
}

double OptionalNumberAttribute(const pugi::xml_node &node, const char *name, const Where &where, double fallback)
{
	const pugi::xml_attribute attribute = node.attribute(name);
	return attribute ? ParseNumber(attribute.value(), name, where) : fallback;
}

int OptionalIntAttribute(const pugi::xml_node &node, const char *name, const Where &where, int fallback)
{
	const pugi::xml_attribute attribute = node.attribute(name);
	return attribute ? ParseInt(attribute.value(), name, where) : fallback;
}

bool OptionalBoolAttribute(const pugi::xml_node &node, const char *name, const Where &where, bool fallback)
{
	const pugi::xml_attribute attribute = node.attribute(name);
	return attribute ? ParseBool(attribute.value(), name, where) : fallback;
}

pugi::xml_node Child(const pugi::xml_node &parent, const char *name, const Where &where)
{
	const pugi::xml_node child = parent.child(name);
	if (!child)
		Fail(where, std::string("missing element <") + name + ">");
	return child;
}

pugi::xml_node OptionalChild(const pugi::xml_node &parent, const char *name) noexcept
{
	return parent.child(name);
}

std::string Text(const pugi::xml_node &node)
{
	return Trimmed(node.text().get());
}

bool StartsWith(const std::string &text, const char *prefix) noexcept
{
	return text.rfind(prefix, 0) == 0;
}
} // namespace Infinitesia::Content::Xml

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "content/Diagnostics.h"
#include "content/loaders/ContentLoader.h"
#include "content/registries/DefinitionRegistry.h"
#include "content/validation/References.h"
#include "content/xml/XmlDocument.h"
#include "content/xml/XmlRead.h"
#include "core/ContentRegistry.h"

using namespace Infinitesia;
using namespace Infinitesia::Content;

namespace
{
namespace Xml = Infinitesia::Content::Xml;

// A directory of content files for one test, removed afterwards.
class TempContent
{
public:
	explicit TempContent(const char *name) : m_directory(std::filesystem::temp_directory_path() / name)
	{
		std::filesystem::remove_all(m_directory);
		std::filesystem::create_directories(m_directory);
	}
	~TempContent()
	{
		std::filesystem::remove_all(m_directory);
	}

	std::filesystem::path Write(const std::string &name, const std::string &text) const
	{
		const std::filesystem::path path = m_directory / name;
		std::filesystem::create_directories(path.parent_path());
		std::ofstream(path, std::ios::trunc) << text;
		return path;
	}

	std::filesystem::path Path(const std::string &name) const
	{
		return m_directory / name;
	}

private:
	std::filesystem::path m_directory;
};

// A stand-in definition, to exercise the registry and reference helpers.
struct SampleDefinition
{
	std::string key;
	int value = 0;
};
} // namespace

TEST_CASE("XmlDocumentReadsTheTreeAndLineNumbers")
{
	const XmlDocument document = XmlDocument::Parse("<root>\n  <child id=\"7\"/>\n</root>\n", "inline.xml");
	CHECK(std::string(document.Root().name()) == "root");
	const pugi::xml_node child = document.Root().child("child");
	REQUIRE(static_cast<bool>(child));
	CHECK(document.LineOf(child) == 2);
	const Xml::Where where{document.Name(), document.LineOf(child), "child"};
	CHECK(Xml::IntAttribute(child, "id", where) == 7);
}

TEST_CASE("MissingAttributeNamesFileAndLine")
{
	const XmlDocument document = XmlDocument::Parse("<root>\n  <child/>\n</root>\n", "inline.xml");
	const pugi::xml_node child = document.Root().child("child");
	const Xml::Where where{document.Name(), document.LineOf(child), "child"};
	try
	{
		Xml::Attribute(child, "id", where);
		FAIL("expected ContentError");
	}
	catch (const ContentError &error)
	{
		const std::string message = error.what();
		CHECK(message.find("inline.xml:2") != std::string::npos);
		CHECK(message.find("missing attribute 'id'") != std::string::npos);
	}
}

TEST_CASE("MalformedNumbersAreRejected")
{
	const XmlDocument document = XmlDocument::Parse("<root density=\"heavy\"/>", "inline.xml");
	const Xml::Where where{document.Name(), document.LineOf(document.Root()), "root"};
	CHECK_THROWS_AS(Xml::NumberAttribute(document.Root(), "density", where), ContentError);
}

TEST_CASE("TextIsTrimmed")
{
	const XmlDocument document = XmlDocument::Parse("<root>\n  <name> Iron </name>\n</root>", "inline.xml");
	CHECK(Xml::Text(document.Root().child("name")) == "Iron");
}

TEST_CASE("ContentTreeReadsIncludesOnceInOrder")
{
	const TempContent temp("infinitesia_content_tree");
	temp.Write("leaf.xml", "<leaf/>");
	temp.Write("a.xml", "<a><include file=\"leaf.xml\"/></a>");
	temp.Write("root.xml", "<root>\n  <include file=\"a.xml\"/>\n"
	                       "  <include file=\"leaf.xml\"/>\n</root>");

	const std::vector<XmlDocument> documents = LoadContentTree(temp.Path("root.xml"));
	REQUIRE(documents.size() == 3);
	CHECK(std::string(documents[0].Root().name()) == "root");
	CHECK(std::string(documents[1].Root().name()) == "a");
	CHECK(std::string(documents[2].Root().name()) == "leaf");
}

TEST_CASE("ContentTreeRejectsIncludeCycles")
{
	const TempContent temp("infinitesia_content_cycle");
	temp.Write("a.xml", "<a><include file=\"b.xml\"/></a>");
	temp.Write("b.xml", "<b><include file=\"a.xml\"/></b>");
	CHECK_THROWS_AS(LoadContentTree(temp.Path("a.xml")), ContentError);
}

TEST_CASE("DiagnosticsFormatEveryProblem")
{
	Diagnostics diagnostics;
	diagnostics.Error("matter.xml", 12, "duplicate id 'rock.granite'");
	diagnostics.Warning("matter.xml", 0, "unused definition 'rock.bedrock'");
	CHECK(diagnostics.Size() == 2);
	const std::vector<std::string> lines = diagnostics.Formatted();
	CHECK(lines[0] == "error: matter.xml:12: duplicate id 'rock.granite'");
	CHECK(lines[1] == "warning: matter.xml: unused definition 'rock.bedrock'");
}

TEST_CASE("ResolveFindsDefinitionsAndReportsMissing")
{
	DefinitionRegistry<SampleDefinition> registry;
	registry.Add({"matter.iron", 1});
	const Xml::Where where{"matter.xml", 3, "matter.iron"};
	CHECK(Resolve(registry, "matter.iron", where).value == 1);
	CHECK_THROWS_AS(Resolve(registry, "matter.gold", where), ContentError);
}

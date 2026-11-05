#include "content/loaders/ContentLoader.h"

#include <set>
#include <utility>

#include "core/ContentRegistry.h"

namespace Infinitesia::Content
{
namespace
{
namespace fs = std::filesystem;

// Reads `path` and, before it, everything it includes. `loaded` holds the files
// already read; `onStack` holds the files being read higher up the recursion, so
// a file that includes itself is caught.
void Collect(const fs::path &path, std::set<fs::path> &loaded, std::set<fs::path> &onStack,
             std::vector<XmlDocument> &documents)
{
	const fs::path canonical = fs::weakly_canonical(path);
	if (onStack.contains(canonical))
	{
		throw ContentError("Content include cycle at '" + path.string() + "'");
	}
	if (loaded.contains(canonical))
		return;

	XmlDocument document = XmlDocument::LoadFromFile(path);
	// The include paths are gathered before the document is moved into the list,
	// so no node outlives its document.
	std::vector<fs::path> includes;
	for (const pugi::xml_node include : document.Root().children("include"))
	{
		const pugi::xml_attribute file = include.attribute("file");
		if (!file || file.value()[0] == '\0')
		{
			throw ContentError(path.string() + ":" + std::to_string(document.LineOf(include)) +
			                   ": <include> without a 'file'");
		}
		includes.emplace_back(path.parent_path() / file.value());
	}

	loaded.insert(canonical);
	documents.push_back(std::move(document));

	onStack.insert(canonical);
	for (const fs::path &include : includes)
	{
		Collect(include, loaded, onStack, documents);
	}
	onStack.erase(canonical);
}
} // namespace

std::vector<XmlDocument> LoadContentTree(const std::filesystem::path &root)
{
	std::set<fs::path> loaded;
	std::set<fs::path> onStack;
	std::vector<XmlDocument> documents;
	Collect(root, loaded, onStack, documents);
	return documents;
}
} // namespace Infinitesia::Content

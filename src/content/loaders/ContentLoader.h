#pragma once

#include <filesystem>
#include <vector>

#include "content/xml/XmlDocument.h"

namespace Infinitesia::Content {
// Reads a content file together with everything it includes, in the order a
// deterministic load needs (docs/DataDriven.md, sections 4.1 and 4.6 — the
// discovery and parsing steps).
//
// An index file pulls in other files with <include file="..."/> children; the
// paths resolve relative to the file that names them. Every file is read once,
// in a depth-first order (a file before the files it includes, includes in
// document order), and an include that loops back to a file still being read is
// an error. Throws ContentError.
std::vector<XmlDocument> LoadContentTree(const std::filesystem::path& root);
}  // namespace Infinitesia::Content

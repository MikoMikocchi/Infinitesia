#pragma once

#include <string>
#include <string_view>

#include "content/xml/XmlRead.h"
#include "core/ContentRegistry.h"

namespace Infinitesia::Content {
// Resolves a definition by key, failing with the place in the data that named
// it when nothing matches (docs/DataDriven.md, section 4.6 — the reference
// resolution step). Used once every definition is registered, so a forward
// reference in the files is fine.
template <typename Definition>
const Definition& Resolve(const ContentRegistry<Definition>& registry,
                          std::string_view key, const Xml::Where& where) {
  const Definition* definition = registry.Find(key);
  if (definition == nullptr) {
    Xml::Fail(where, "unknown reference '" + std::string(key) + "'");
  }
  return *definition;
}
}  // namespace Infinitesia::Content

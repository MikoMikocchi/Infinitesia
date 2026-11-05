#pragma once

#include "core/ContentRegistry.h"

namespace Infinitesia::Content
{
// One registry per kind of definition — materials, species, items, machines
// (docs/DataDriven.md, section 4.7). The mechanism is core's ContentRegistry:
// definitions are added while loading and are found by their stable string key
// afterwards, so this alias names what the docs describe without a second copy
// of the container.
template <typename Definition> using DefinitionRegistry = ContentRegistry<Definition>;
} // namespace Infinitesia::Content

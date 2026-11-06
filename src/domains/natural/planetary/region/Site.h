#pragma once

#include <cstdint>

#include "core/PlanetId.h"
#include "core/PlanetaryCoordinates.h"
#include "domains/natural/planetary/region/LocalFrame.h"
#include "domains/natural/planetary/region/LocalGrid.h"

namespace Infinitesia
{
// A stable address of a place the player has founded or visited. The app owns
// the runtime state; the domain only knows where the site is.
using SiteId = std::uint64_t;
inline constexpr SiteId kNoSite = 0;

enum class SiteKind : std::uint8_t
{
	Settlement,
	Mine,
	Outpost
};

// One place on the planet: the anchor of its local grid, the region it was
// founded in, and the flat frame centred on that region. `grid` is kept while
// the local terrain and geology still address metres from the anchor; `frame`
// is what the local world samples through.
struct Site
{
	SiteId id = kNoSite;
	PlanetId planet = kHomePlanet;
	SiteKind kind = SiteKind::Settlement;
	GeographicCoordinates center{};
	LocalFrame frame{};
	LocalGrid grid{};
	RegionId region{};
};
} // namespace Infinitesia

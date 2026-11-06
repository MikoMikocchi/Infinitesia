#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gameplay/campaign/Campaign.h"

namespace
{
using namespace Infinitesia;
} // namespace

TEST_CASE("CampaignRunsThePlanetClockOnItsOwnCalendar")
{
	// One tick is one game second, one day is one second and one month is one
	// day: every tick crosses a month boundary, which keeps the test instant.
	Campaign campaign(20.0f, GameCalendar{1, 1, 1});

	CHECK(campaign.Clock().GetTickCount() == 0);
	CHECK(campaign.ElapsedMonths() == 0);
	CHECK(campaign.Runtime().Registry().Contains(SystemId("system.planet.clock")));

	const float tick = campaign.Clock().GetFixedDeltaTime();
	CHECK(campaign.AdvanceFrame(tick) == 1);
	CHECK(campaign.Clock().GetTickCount() == 1);
	CHECK(campaign.ElapsedMonths() == 1);

	CHECK(campaign.AdvanceFrame(tick) == 1);
	CHECK(campaign.Clock().GetTickCount() == 2);
	CHECK(campaign.ElapsedMonths() == 2);
	CHECK(campaign.Calendar().SecondsPerMonth() == 1);
}

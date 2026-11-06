#include "gameplay/campaign/Campaign.h"

#include <cstdio>
#include <memory>

#include "simulation/api/Cadence.h"
#include "simulation/api/SimulationSystem.h"
#include "simulation/api/SystemDescriptor.h"
#include "simulation/api/SystemPhase.h"

namespace Infinitesia
{
namespace
{
// A minimal planet-level system. Its only job today is to prove that the
// campaign clock keeps running outside a site and that month cadences work; the
// real population, founding and diplomacy systems will take the same slot.
class PlanetClockSystem final : public SimulationSystem
{
public:
	explicit PlanetClockSystem(std::uint64_t &months) : m_months(months)
	{
	}

	static SystemDescriptor Descriptor()
	{
		return {SystemId("system.planet.clock"), SystemPhase::PostUpdate, Cadence::EveryGameMonths(1)};
	}

	void Update(const SystemContext &context) override
	{
		m_months = context.time.month;
		std::printf("[campaign] month %llu\n", static_cast<unsigned long long>(m_months));
	}

private:
	std::uint64_t &m_months;
};
} // namespace

Campaign::Campaign(float ticksPerSecond, GameCalendar calendar)
    : m_clock(ticksPerSecond), m_runtime(m_clock, calendar)
{
	m_runtime.Registry().Register(PlanetClockSystem::Descriptor(), std::make_unique<PlanetClockSystem>(m_months));
}

Campaign::~Campaign() = default;

std::uint64_t Campaign::AdvanceFrame(float frameDelta)
{
	return m_clock.AdvanceFrame(frameDelta);
}
} // namespace Infinitesia

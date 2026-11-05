#include "simulation/api/GameCalendar.h"

#include <stdexcept>

namespace Infinitesia
{
GameCalendar::GameCalendar(std::uint32_t secondsPerTick, std::uint32_t secondsPerDay, std::uint32_t daysPerMonth)
    : m_secondsPerTick(secondsPerTick), m_secondsPerDay(secondsPerDay), m_daysPerMonth(daysPerMonth)
{
	if (secondsPerTick == 0 || secondsPerDay == 0 || daysPerMonth == 0)
	{
		throw std::invalid_argument("Game calendar needs a non-zero tick, day and month length.");
	}
}

std::uint64_t GameCalendar::SecondsPerMonth() const
{
	return static_cast<std::uint64_t>(m_secondsPerDay) * m_daysPerMonth;
}

std::uint64_t GameCalendar::SecondsAfterTicks(std::uint64_t tickCount) const
{
	return tickCount * m_secondsPerTick;
}

GameTime GameCalendar::At(std::uint64_t seconds) const
{
	GameTime time;
	time.seconds = seconds;
	time.day = seconds / m_secondsPerDay;
	time.month = time.day / m_daysPerMonth;
	time.secondOfDay = static_cast<std::uint32_t>(seconds % m_secondsPerDay);
	time.dayOfMonth = static_cast<std::uint32_t>(time.day % m_daysPerMonth);
	return time;
}
} // namespace Infinitesia

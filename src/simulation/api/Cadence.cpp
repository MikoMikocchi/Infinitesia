#include "simulation/api/Cadence.h"

#include <stdexcept>

namespace Infinitesia {
namespace {
std::uint64_t PeriodInGameSeconds(const Cadence& cadence,
                                  const GameCalendar& calendar) {
  switch (cadence.unit) {
    case CadenceUnit::GameSecond:
      return cadence.interval;
    case CadenceUnit::GameDay:
      return cadence.interval * calendar.SecondsPerDay();
    case CadenceUnit::GameMonth:
      return cadence.interval * calendar.SecondsPerMonth();
    case CadenceUnit::Tick:
      break;
  }
  throw std::logic_error("Cadence unit has no length in game seconds.");
}
}  // namespace

bool IsDue(const Cadence& cadence, const GameCalendar& calendar,
           std::uint64_t tick) {
  if (!cadence.IsValid()) {
    throw std::invalid_argument("Cadence interval must be greater than zero.");
  }

  if (cadence.unit == CadenceUnit::Tick) {
    return (tick + 1) % cadence.interval == 0;
  }

  const std::uint64_t period = PeriodInGameSeconds(cadence, calendar);
  const std::uint64_t before = calendar.SecondsAfterTicks(tick) / period;
  const std::uint64_t after = calendar.SecondsAfterTicks(tick + 1) / period;
  return after > before;
}
}  // namespace Infinitesia

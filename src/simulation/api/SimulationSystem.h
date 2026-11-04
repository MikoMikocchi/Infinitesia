#pragma once

#include "simulation/api/SystemContext.h"

namespace Infinitesia {
// One piece of the simulation that runs on its schedule. A system holds (or
// points to) the state it works on; the scheduler only decides when it runs. It
// must not register systems or touch the scheduler from `Update`. An exception
// thrown from it is handled by the executor's error policy.
class SimulationSystem {
 public:
  virtual ~SimulationSystem() = default;

  virtual void Update(const SystemContext& context) = 0;
};
}  // namespace Infinitesia

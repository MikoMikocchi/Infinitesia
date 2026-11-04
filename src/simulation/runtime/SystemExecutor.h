#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "simulation/api/SystemContext.h"
#include "simulation/runtime/SystemRegistry.h"

namespace Infinitesia {
// What happens when a system throws.
enum class SystemErrorPolicy : std::uint8_t {
  Disable,   // record the failure and switch the system off; the rest keep
             // running
  Continue,  // record the failure; the system runs again when next due
  Rethrow,   // record the failure and let the exception out of the tick
};

struct SystemFailure {
  SystemId id;
  std::uint64_t tick = 0;
  SystemPhase phase = SystemPhase::Update;
  std::string message;
};

// Runs systems one at a time, handing each its context and dealing with its
// errors. It also guards the sequence: ticks run one after another from 0, and
// inside a tick the phases never go backwards. A violation is a bug of the
// caller and throws std::logic_error.
class SystemExecutor {
 public:
  explicit SystemExecutor(
      SystemErrorPolicy policy = SystemErrorPolicy::Disable);

  // `tick` must be the next one. A tick left open by an exception may be begun
  // again.
  void BeginTick(std::uint64_t tick);
  void Execute(RegisteredSystem& entry, const SystemContext& context);
  void EndTick();

  SystemErrorPolicy GetPolicy() const { return m_policy; }
  void SetPolicy(SystemErrorPolicy policy) { m_policy = policy; }
  // Ticks completed so far.
  std::uint64_t CompletedTicks() const { return m_completedTicks; }
  const std::vector<SystemFailure>& Failures() const { return m_failures; }
  void ClearFailures() { m_failures.clear(); }

 private:
  // Called from inside a catch block: Rethrow rethrows the exception being
  // handled.
  void HandleFailure(RegisteredSystem& entry, const SystemContext& context,
                     const char* message);

  SystemErrorPolicy m_policy;
  std::uint64_t m_completedTicks = 0;
  bool m_inTick = false;
  std::uint64_t m_currentTick = 0;
  SystemPhase m_currentPhase = SystemPhase::PreUpdate;
  std::vector<SystemFailure> m_failures;
};
}  // namespace Infinitesia

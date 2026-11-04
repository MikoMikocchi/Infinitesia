#include "core/Time.h"

#include <cmath>
#include <stdexcept>

namespace Infinitesia {
void Time::Advance(float deltaTime) {
  if (!std::isfinite(deltaTime) || deltaTime < 0.0f) {
    throw std::invalid_argument(
        "Frame delta time must be finite and non-negative.");
  }

  m_deltaTime = deltaTime;
  m_totalTime += static_cast<double>(m_deltaTime);
}

float Time::GetDeltaTime() const { return m_deltaTime; }

double Time::GetTotalTime() const { return m_totalTime; }
}  // namespace Infinitesia

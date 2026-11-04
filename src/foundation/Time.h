#pragma once

namespace Infinitesia {
class Time {
 public:
  void Advance(float deltaTime);

  float GetDeltaTime() const;
  double GetTotalTime() const;

 private:
  float m_deltaTime = 0.0f;
  double m_totalTime = 0.0;
};
}  // namespace Infinitesia

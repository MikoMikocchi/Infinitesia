#pragma once

#include <cstdint>
#include <functional>

namespace Infinitesia
{
class Simulation
{
public:
	// Called for every fixed tick with its 0-based number, before the tick is
	// counted. If it throws, the tick is not counted and the exception leaves
	// AdvanceFrame.
	using TickListener = std::function<void(std::uint64_t tick)>;

	explicit Simulation(float ticksPerSecond = 20.0f);

	// One listener at a time; an empty one removes it.
	void SetTickListener(TickListener listener);

	std::uint64_t AdvanceFrame(float frameDelta);

	std::uint64_t GetTickCount() const;
	float GetFixedDeltaTime() const;
	float GetTicksPerSecond() const;

private:
	void Tick();

	float m_ticksPerSecond;
	float m_fixedDeltaTime;
	float m_accumulator = 0.0f;
	std::uint64_t m_tickCount = 0;
	TickListener m_tickListener;
};
} // namespace Infinitesia

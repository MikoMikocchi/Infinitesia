#include "core/Simulation.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
Simulation::Simulation(float ticksPerSecond) : m_ticksPerSecond(ticksPerSecond), m_fixedDeltaTime(0.0f)
{
	if (!std::isfinite(ticksPerSecond) || ticksPerSecond <= 0.0f)
	{
		throw std::invalid_argument("Simulation tick rate must be finite and greater than zero.");
	}

	m_fixedDeltaTime = 1.0f / m_ticksPerSecond;
	if (!std::isfinite(m_fixedDeltaTime) || m_fixedDeltaTime <= 0.0f)
	{
		throw std::invalid_argument("Simulation tick rate produces an invalid fixed time step.");
	}
}

std::uint64_t Simulation::AdvanceFrame(float frameDelta)
{
	if (!std::isfinite(frameDelta) || frameDelta < 0.0f)
	{
		throw std::invalid_argument("Frame delta time must be finite and non-negative.");
	}

	std::uint64_t advancedTicks = 0;
	m_accumulator += frameDelta;

	while (m_accumulator >= m_fixedDeltaTime && advancedTicks < kMaxTicksPerFrame)
	{
		Tick();
		m_accumulator -= m_fixedDeltaTime;
		++advancedTicks;
	}

	// A frame this slow is already the problem; carrying its backlog into the
	// next frame would only make it slower.
	if (m_accumulator >= m_fixedDeltaTime)
	{
		m_accumulator = 0.0f;
	}

	return advancedTicks;
}

void Simulation::SetTickListener(TickListener listener)
{
	m_tickListener = std::move(listener);
}

void Simulation::Tick()
{
	if (m_tickListener)
	{
		m_tickListener(m_tickCount);
	}
	++m_tickCount;
}

std::uint64_t Simulation::GetTickCount() const
{
	return m_tickCount;
}

float Simulation::GetFixedDeltaTime() const
{
	return m_fixedDeltaTime;
}

float Simulation::GetTicksPerSecond() const
{
	return m_ticksPerSecond;
}
} // namespace Infinitesia

#pragma once

#include <array>
#include <cstdint>

namespace Infinitesia
{
// The stages of one simulation tick, in the order they run. Every system
// belongs to one.
enum class SystemPhase : std::uint8_t
{
	PreUpdate,
	Update,
	PostUpdate,
};

inline constexpr std::array<SystemPhase, 3> kSystemPhases{SystemPhase::PreUpdate, SystemPhase::Update,
                                                          SystemPhase::PostUpdate};

constexpr const char *SystemPhaseName(SystemPhase phase)
{
	switch (phase)
	{
	case SystemPhase::PreUpdate:
		return "PreUpdate";
	case SystemPhase::Update:
		return "Update";
	case SystemPhase::PostUpdate:
		return "PostUpdate";
	}
	return "Unknown";
}
} // namespace Infinitesia

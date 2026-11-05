#pragma once

#include <algorithm>

#include "raylib.h"

namespace Infinitesia
{
// How the levels below the viewed one fade, so that depth reads at a glance.
// The same numbers drive the world shader (terrain) and the CPU tint of
// objects.
namespace DepthShading
{
// Brightness lost for each level under the viewed one, and the floor it stops
// at.
inline constexpr float kDarkenPerLevel = 0.10f;
inline constexpr float kMinBrightness = 0.35f;
// Colour drained towards grey for each level, and the most it drains.
inline constexpr float kDesaturatePerLevel = 0.06f;
inline constexpr float kMaxDesaturation = 0.45f;

// The colour as seen the given number of levels below the viewed one.
inline Color Apply(Color color, int levelsBelow)
{
	if (levelsBelow <= 0)
	{
		return color;
	}
	const float levels = static_cast<float>(levelsBelow);
	const float brightness = std::max(1.0f - levels * kDarkenPerLevel, kMinBrightness);
	const float drained = std::min(levels * kDesaturatePerLevel, kMaxDesaturation);
	const float grey = 0.299f * static_cast<float>(color.r) + 0.587f * static_cast<float>(color.g) +
	                   0.114f * static_cast<float>(color.b);
	const auto channel = [&](unsigned char value) {
		const float mixed = static_cast<float>(value) + (grey - static_cast<float>(value)) * drained;
		return static_cast<unsigned char>(std::clamp(mixed * brightness, 0.0f, 255.0f));
	};
	return {channel(color.r), channel(color.g), channel(color.b), color.a};
}
} // namespace DepthShading
} // namespace Infinitesia

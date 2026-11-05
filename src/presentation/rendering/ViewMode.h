#pragma once

namespace Infinitesia
{
// What of the world the camera shows relative to the selected level.
enum class ViewMode
{
	CutAtLevel, // everything above the selected level is cut away
	ShowAll     // the whole world, above the selected level too
};

inline bool IsLevelShown(ViewMode mode, int level, int selectedLevel)
{
	return mode == ViewMode::ShowAll || level <= selectedLevel;
}
} // namespace Infinitesia

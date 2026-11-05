#include "presentation/ui/PlanetViewerLayout.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia
{
ViewerLayout GetLayout(int screenWidth, int screenHeight, const PlanetMap &planetMap)
{
	const float width = static_cast<float>(screenWidth);
	const float height = static_cast<float>(screenHeight);
	const float contentTop = kHeaderHeight;
	const float contentBottom = std::max(contentTop + 1.0f, height - kFooterHeight);
	const float contentHeight = contentBottom - contentTop;
	const float splitX = width * 0.48f;

	ViewerLayout layout;
	layout.mapPanel = {kPanelPadding, contentTop, std::max(1.0f, splitX - kPanelPadding * 1.5f), contentHeight};
	layout.globePanel = {splitX, contentTop, std::max(1.0f, width - splitX - kPanelPadding), contentHeight};

	const float mapWidth = layout.mapPanel.width - 36.0f;
	const float mapHeight = layout.mapPanel.height - 100.0f;
	const float mapAspectRatio =
	    static_cast<float>(planetMap.GetLongitudeSegmentCount()) / static_cast<float>(planetMap.GetLatitudeBandCount());
	float fittedWidth = std::min(mapWidth, mapHeight * mapAspectRatio);
	float fittedHeight = fittedWidth / mapAspectRatio;
	if (fittedWidth <= 0.0f || fittedHeight <= 0.0f)
	{
		fittedWidth = std::max(1.0f, mapWidth);
		fittedHeight = std::max(1.0f, mapHeight);
	}
	layout.mapBounds = {layout.mapPanel.x + (layout.mapPanel.width - fittedWidth) * 0.5f,
	                    layout.mapPanel.y + (layout.mapPanel.height - fittedHeight) * 0.5f, fittedWidth, fittedHeight};

	layout.regionBounds = {layout.mapPanel.x + (layout.mapPanel.width - mapWidth) * 0.5f,
	                       layout.mapPanel.y + (layout.mapPanel.height - mapHeight) * 0.5f, std::max(1.0f, mapWidth),
	                       std::max(1.0f, mapHeight)};

	layout.globeCenter = {layout.globePanel.x + layout.globePanel.width * 0.5f,
	                      layout.globePanel.y + layout.globePanel.height * 0.5f};
	layout.globeRadius = std::max(1.0f, std::min(layout.globePanel.width * 0.39f, layout.globePanel.height * 0.42f));
	return layout;
}

float GetMinimumZoom(const ViewerLayout &layout, int screenHeight)
{
	const float fieldOfViewRadians = kCameraFieldOfView * kPi / 180.0f;
	const float focalLength = static_cast<float>(screenHeight) / (2.0f * std::tan(fieldOfViewRadians * 0.5f));
	const float baseCameraDistance = std::max(2.4f, focalLength / layout.globeRadius * 1.08f);
	const float availableRadius =
	    std::max(1.0f, std::min(layout.globePanel.width, layout.globePanel.height) * 0.5f - 12.0f);
	const float distanceScale = std::sqrt(1.0f + std::pow(focalLength / availableRadius, 2.0f)) / baseCameraDistance;
	return std::clamp(distanceScale, 0.55f, 2.0f);
}

bool RectangleContains(Rectangle rectangle, Vector2 point)
{
	return point.x >= rectangle.x && point.x < rectangle.x + rectangle.width && point.y >= rectangle.y &&
	       point.y < rectangle.y + rectangle.height;
}
} // namespace Infinitesia

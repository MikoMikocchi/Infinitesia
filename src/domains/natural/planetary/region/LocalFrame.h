#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/PlanetaryCoordinates.h"

namespace Infinitesia
{
// A small flat frame over the surface at one point: positions are metres east
// and north of its centre. Over the few kilometres a site spans the difference
// from the sphere is far below one block, so local generation needs no tangent
// planes, gnomonic projection or pole special cases.
//
// This is the local gameplay coordinate frame. The planet map and the globe
// keep using GeographicCoordinates; only the site itself is flat.
struct LocalFrame
{
	static constexpr double kMetersPerDegree = 111320.0;

	GeographicCoordinates center{};
	double metersPerDegreeEast = kMetersPerDegree;
	double metersPerDegreeNorth = kMetersPerDegree;

	static LocalFrame At(GeographicCoordinates position) noexcept
	{
		LocalFrame frame;
		frame.center = position;
		// Near a pole a degree of longitude shrinks to nothing; the clamp keeps
		// the frame usable there, and a site is never large enough for the
		// distortion to matter.
		frame.metersPerDegreeEast =
		    kMetersPerDegree * std::max(1e-4, std::cos(position.latitudeDegrees * std::numbers::pi / 180.0));
		return frame;
	}

	GeographicCoordinates ToGeographic(double eastMeters, double northMeters) const noexcept
	{
		GeographicCoordinates position;
		position.latitudeDegrees = std::clamp(center.latitudeDegrees + northMeters / metersPerDegreeNorth, -90.0, 90.0);
		double longitude = center.longitudeDegrees + eastMeters / metersPerDegreeEast;
		longitude = std::fmod(longitude, 360.0);
		if (longitude < 0.0)
		{
			longitude += 360.0;
		}
		position.longitudeDegrees = longitude;
		return position;
	}

	// The inverse within the frame's flat approximation.
	void ToLocal(GeographicCoordinates position, double &eastMeters, double &northMeters) const noexcept
	{
		northMeters = (position.latitudeDegrees - center.latitudeDegrees) * metersPerDegreeNorth;
		double deltaLongitude = position.longitudeDegrees - center.longitudeDegrees;
		if (deltaLongitude > 180.0)
		{
			deltaLongitude -= 360.0;
		}
		else if (deltaLongitude < -180.0)
		{
			deltaLongitude += 360.0;
		}
		eastMeters = deltaLongitude * metersPerDegreeEast;
	}
};
} // namespace Infinitesia

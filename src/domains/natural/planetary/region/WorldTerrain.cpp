#include "domains/natural/planetary/region/WorldTerrain.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/PlanetaryCoordinates.h"

namespace Infinitesia
{
namespace
{
// Water lies this far below the surrounding bank so a river reads as a channel
// rather than a sheet of water on the ground.
constexpr double kRiverFreeboardMeters = 0.25;
// The part of the channel where there is actual water, and the extra bank over
// which the ground is lowered smoothly to the bed.
constexpr double kMinimumWaterHalfWidthMeters = 0.5;
constexpr double kMinimumBankWidthMeters = 1.5;
constexpr double kMaximumBankWidthMeters = 8.0;

struct ChannelSample
{
	bool inChannel = false;
	double distanceMeters = 0.0;
	double incisionMeters = 0.0;
	// Closest point on the centreline, in metres from the tile's south-west
	// corner.
	double centerX = 0.0;
	double centerY = 0.0;
};

double SmoothStep(double t) noexcept
{
	t = std::clamp(t, 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

ChannelSample SampleChannel(const RiverChannel &river, double localX, double localY) noexcept
{
	ChannelSample sample;
	if (!river.IsValid())
	{
		return sample;
	}

	const double fromX = static_cast<double>(river.fromX) * kRegionTileSizeMeters;
	const double fromY = static_cast<double>(river.fromY) * kRegionTileSizeMeters;
	const double toX = static_cast<double>(river.toX) * kRegionTileSizeMeters;
	const double toY = static_cast<double>(river.toY) * kRegionTileSizeMeters;
	const double segmentX = toX - fromX;
	const double segmentY = toY - fromY;
	const double lengthSquared = segmentX * segmentX + segmentY * segmentY;
	if (lengthSquared < 1.0)
	{
		return sample;
	}

	const double t = std::clamp(((localX - fromX) * segmentX + (localY - fromY) * segmentY) / lengthSquared, 0.0, 1.0);
	sample.centerX = fromX + segmentX * t;
	sample.centerY = fromY + segmentY * t;
	const double dx = localX - sample.centerX;
	const double dy = localY - sample.centerY;
	sample.distanceMeters = std::sqrt(dx * dx + dy * dy);

	const double waterHalfWidth = std::max(kMinimumWaterHalfWidthMeters, static_cast<double>(river.widthMeters) * 0.5);
	const double bankWidth =
	    std::clamp(static_cast<double>(river.widthMeters) * 0.5, kMinimumBankWidthMeters, kMaximumBankWidthMeters);
	const double bankHalfWidth = waterHalfWidth + bankWidth;
	if (sample.distanceMeters >= bankHalfWidth)
	{
		return sample;
	}

	sample.inChannel = true;
	const double bankT = (bankHalfWidth - sample.distanceMeters) / bankWidth;
	const double profile = sample.distanceMeters <= waterHalfWidth ? 1.0 : SmoothStep(bankT);
	sample.incisionMeters = static_cast<double>(river.depthMeters) * profile;
	return sample;
}
} // namespace

WorldTerrain::WorldTerrain(const PlanetMap &map, std::shared_ptr<const WorldMap> world) noexcept
    : m_planet(map), m_world(std::move(world))
{
}

PlanetFieldSample WorldTerrain::ApplyTile(PlanetFieldSample sample, const RegionTile *tile) noexcept
{
	if (tile == nullptr)
	{
		return sample;
	}

	// The detail amplitude and the coast blend stay tied to the planet field:
	// the tile only replaces the surface the detail is added to.
	sample.baseElevationMeters = sample.elevationMeters;
	sample.elevationMeters = tile->elevationMeters;
	sample.surface.temperatureCelsius = tile->temperatureCelsius;
	sample.surface.precipitationMm = tile->precipitationMm;
	sample.surface.drySeasonMonths = tile->drySeasonMonths;
	sample.surface.soilFertility = tile->soilFertility;
	sample.surface.province = tile->province;
	sample.surface.secondaryProvince = kNoProvince;
	sample.surface.secondaryWeight = 0.0;
	// The world's own water replaces the coarse planet hydrology inside the
	// window, so the same lake and river reach the local terrain and the biome
	// classifier.
	sample.surface.drainageAccumulation = tile->riverDischarge;
	sample.surface.lakeFraction = tile->IsLake() ? 1.0 : 0.0;
	sample.surface.lakeWaterLevelKm = static_cast<double>(tile->waterLevelMeters) / 1000.0;
	sample.surface.lakeSpillHeightKm = sample.surface.lakeWaterLevelKm;
	return sample;
}

PlanetFieldSample WorldTerrain::SampleAt(const GeographicCoordinates &position) const noexcept
{
	PlanetFieldSample sample = m_planet.SampleAt(position);
	if (m_world != nullptr)
	{
		const auto tile = m_world->SampleAt(PlanetaryCoordinates::ToCartesian(position));
		if (tile.has_value())
		{
			sample = ApplyTile(sample, &*tile);
		}
	}
	return sample;
}

PlanetFieldSample WorldTerrain::SamplePlanet(CartesianPoint3D direction) const noexcept
{
	return SampleAt(PlanetaryCoordinates::ToGeographic(direction));
}

double WorldTerrain::ElevationMeters(const GeographicCoordinates &position, double minWavelengthMeters) const noexcept
{
	return ElevationMeters(position, SampleAt(position), minWavelengthMeters);
}

double WorldTerrain::ElevationMeters(const GeographicCoordinates &position, const PlanetFieldSample &planet,
                                     double minWavelengthMeters) const noexcept
{
	const double elevation = m_planet.ElevationMeters(position, planet, minWavelengthMeters);
	const LocalWater water = WaterAt(position, minWavelengthMeters);
	return elevation - water.incisionMeters;
}

double WorldTerrain::ElevationMeters(CartesianPoint3D direction, double minWavelengthMeters) const noexcept
{
	return ElevationMeters(PlanetaryCoordinates::ToGeographic(direction), minWavelengthMeters);
}

double WorldTerrain::ElevationMeters(CartesianPoint3D direction, const PlanetFieldSample &planet,
                                     double minWavelengthMeters) const noexcept
{
	return ElevationMeters(PlanetaryCoordinates::ToGeographic(direction), planet, minWavelengthMeters);
}

LocalWater WorldTerrain::WaterAt(const GeographicCoordinates &position, double minWavelengthMeters) const noexcept
{
	LocalWater water;
	if (m_world == nullptr)
	{
		return water;
	}

	const RegionTileLocation location = m_world->Locate(PlanetaryCoordinates::ToCartesian(position));
	if (!location)
	{
		return water;
	}

	const RegionTile &tile = *location.tile;
	const double flowLength = std::sqrt(static_cast<double>(tile.flowEast) * tile.flowEast +
	                                    static_cast<double>(tile.flowNorth) * tile.flowNorth);
	if (flowLength > 0.0)
	{
		water.flowEast = static_cast<double>(tile.flowEast) / flowLength;
		water.flowNorth = static_cast<double>(tile.flowNorth) / flowLength;
	}
	if (tile.IsOcean())
	{
		water.origin = FluidOrigin::Sea;
		water.surfaceMeters = 0.0;
		water.depthMeters = std::max(0.0, -static_cast<double>(tile.elevationMeters));
		return water;
	}
	if (tile.IsLake())
	{
		water.origin = FluidOrigin::Lake;
		water.surfaceMeters = tile.waterLevelMeters;
		water.depthMeters = tile.waterDepthMeters;
		return water;
	}
	if (!tile.HasRiver())
	{
		return water;
	}

	const double localX = location.x * kRegionTileSizeMeters;
	const double localY = location.y * kRegionTileSizeMeters;
	const ChannelSample channel = SampleChannel(tile.river, localX, localY);
	if (!channel.inChannel)
	{
		return water;
	}
	water.incisionMeters = channel.incisionMeters;

	// Only the bed carries water; the banks are dry but lowered.
	const double waterHalfWidth =
	    std::max(kMinimumWaterHalfWidthMeters, static_cast<double>(tile.river.widthMeters) * 0.5);
	if (channel.distanceMeters > waterHalfWidth)
	{
		return water;
	}

	// The water surface follows the centreline ground, smoothed by the same
	// tile blend the terrain uses, so the river runs downhill with the land and
	// neighbouring tiles agree at the border.
	const double eastMeters = (static_cast<double>(location.id.i) - 0.5) * kRegionTileSizeMeters + channel.centerX;
	const double northMeters = (static_cast<double>(location.id.j) - 0.5) * kRegionTileSizeMeters + channel.centerY;
	const GeographicCoordinates centerPosition =
	    PlanetaryCoordinates::ToGeographic(m_world->Grid().Direction(eastMeters, northMeters));

	const auto blended = m_world->SampleAt(PlanetaryCoordinates::ToCartesian(centerPosition));
	PlanetFieldSample centerSample = m_planet.SampleAt(centerPosition);
	if (blended.has_value())
	{
		centerSample = ApplyTile(centerSample, &*blended);
	}
	const double groundMeters = m_planet.ElevationMeters(centerPosition, centerSample, minWavelengthMeters);

	water.origin = FluidOrigin::River;
	water.surfaceMeters = groundMeters - kRiverFreeboardMeters;
	water.depthMeters = std::max(0.0, channel.incisionMeters - kRiverFreeboardMeters);
	return water;
}
} // namespace Infinitesia

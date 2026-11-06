#include "domains/natural/planetary/region/WorldTerrain.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "core/PlanetaryCoordinates.h"

namespace Infinitesia
{
namespace
{
// Water lies this far below the surrounding bank so a river reads as a channel
// rather than a sheet of water on the ground.
constexpr double kRiverFreeboardMeters = 0.25;
// The part of the channel where there is actual water, and the dry bank around
// it. The narrow strip at the water's edge slopes up to the surface, like a
// shore, instead of ending in a vertical wall. Beyond the bank crest the ground
// blends back into the natural terrain over a wide valley, so the channel does
// not look like a walled canal.
constexpr double kMinimumWaterHalfWidthMeters = 0.5;
constexpr double kMinimumBankWidthMeters = 6.0;
constexpr double kMaximumBankWidthMeters = 24.0;
constexpr double kMinimumShoreWidthMeters = 1.0;
// Share of the bank width that rises to the crest. The rest is the outer
// valley slope that fades the levee back into the landscape.
constexpr double kBankCrestFraction = 0.6;
// A bank must stand above the water surface, otherwise the fluid field is free
// to level the river across the surrounding land. The crest is a little more
// than one full block above the water, so even a completely full top fluid cell
// cannot stand above the crest; the lip keeps the crest just clear of the
// blocks below it.
constexpr double kBankLipMeters = 0.05;
constexpr double kBankRiseMeters = 1.55;
// A river that crosses a region-tile corner must still be continuous on the
// other side. Near a tile border, neighbouring tiles' channels are considered
// too, so the water and the carved valley are not clipped by the tile edge.
constexpr double kChannelSearchMarginMeters = 80.0;

struct ChannelSample
{
	bool inChannel = false;
	bool inWater = false;
	double distanceMeters = 0.0;
	// Closest point on the centreline, in metres from the tile's south-west
	// corner.
	double centerX = 0.0;
	double centerY = 0.0;
	double waterHalfWidth = 0.0;
	// Radius where the dry bank reaches its crest, and where the outer valley
	// slope has faded completely into the natural terrain.
	double crestHalfWidth = 0.0;
	double valleyHalfWidth = 0.0;
};

double SmoothStep(double t) noexcept
{
	t = std::clamp(t, 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

double DistanceToSegment(double px, double py, double ax, double ay, double bx, double by) noexcept
{
	const double dx = bx - ax;
	const double dy = by - ay;
	const double lengthSquared = dx * dx + dy * dy;
	if (lengthSquared < 1.0e-9)
	{
		return std::hypot(px - ax, py - ay);
	}
	const double t = std::clamp(((px - ax) * dx + (py - ay) * dy) / lengthSquared, 0.0, 1.0);
	return std::hypot(px - (ax + dx * t), py - (ay + dy * t));
}

struct CurvePoint
{
	double x = 0.0;
	double y = 0.0;
	double distance = 0.0;
};

CurvePoint ClosestPointOnBezier(double px, double py, double fromX, double fromY, double controlX, double controlY,
                                double toX, double toY) noexcept
{
	const auto point = [&](double t) {
		const double inverse = 1.0 - t;
		return CurvePoint{inverse * inverse * fromX + 2.0 * inverse * t * controlX + t * t * toX,
		                  inverse * inverse * fromY + 2.0 * inverse * t * controlY + t * t * toY, 0.0};
	};

	constexpr int kCoarseSamples = 8;
	CurvePoint best;
	best.distance = std::numeric_limits<double>::infinity();
	int bestIndex = 0;
	for (int sample = 0; sample <= kCoarseSamples; ++sample)
	{
		const double t = static_cast<double>(sample) / kCoarseSamples;
		const CurvePoint candidate = point(t);
		const double distance = std::hypot(px - candidate.x, py - candidate.y);
		if (distance < best.distance)
		{
			best = candidate;
			best.distance = distance;
			bestIndex = sample;
		}
	}

	// Refine the bracketing interval by ternary search. The coarse pass alone is
	// too coarse for narrow channels.
	double low = std::max(0.0, (static_cast<double>(bestIndex) - 1.0) / kCoarseSamples);
	double high = std::min(1.0, (static_cast<double>(bestIndex) + 1.0) / kCoarseSamples);
	for (int iteration = 0; iteration < 16; ++iteration)
	{
		const double first = low + (high - low) / 3.0;
		const double second = high - (high - low) / 3.0;
		const CurvePoint firstPoint = point(first);
		const CurvePoint secondPoint = point(second);
		const double firstDistance = std::hypot(px - firstPoint.x, py - firstPoint.y);
		const double secondDistance = std::hypot(px - secondPoint.x, py - secondPoint.y);
		if (firstDistance < secondDistance)
		{
			high = second;
		}
		else
		{
			low = first;
		}
	}

	CurvePoint refined = point((low + high) * 0.5);
	refined.distance = std::hypot(px - refined.x, py - refined.y);
	return refined.distance < best.distance ? refined : best;
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

	const double waterHalfWidth = std::max(kMinimumWaterHalfWidthMeters, static_cast<double>(river.widthMeters) * 0.5);
	const double bankWidth =
	    std::clamp(static_cast<double>(river.widthMeters) * 0.5, kMinimumBankWidthMeters, kMaximumBankWidthMeters);
	const double crestHalfWidth = waterHalfWidth + bankWidth * kBankCrestFraction;
	const double valleyHalfWidth = waterHalfWidth + 2.0 * bankWidth;

	if (river.HasBend())
	{
		const double controlX = static_cast<double>(river.bendX) * kRegionTileSizeMeters;
		const double controlY = static_cast<double>(river.bendY) * kRegionTileSizeMeters;
		const double maxBendOffset = DistanceToSegment(controlX, controlY, fromX, fromY, toX, toY);
		if (DistanceToSegment(localX, localY, fromX, fromY, toX, toY) > valleyHalfWidth + maxBendOffset + 1.0)
		{
			return sample;
		}
		const CurvePoint closest = ClosestPointOnBezier(localX, localY, fromX, fromY, controlX, controlY, toX, toY);
		sample.centerX = closest.x;
		sample.centerY = closest.y;
		sample.distanceMeters = closest.distance;
	}
	else
	{
		const double t =
		    std::clamp(((localX - fromX) * segmentX + (localY - fromY) * segmentY) / lengthSquared, 0.0, 1.0);
		sample.centerX = fromX + segmentX * t;
		sample.centerY = fromY + segmentY * t;
		sample.distanceMeters = std::hypot(localX - sample.centerX, localY - sample.centerY);
	}

	if (sample.distanceMeters >= valleyHalfWidth)
	{
		return sample;
	}

	sample.inChannel = true;
	sample.inWater = sample.distanceMeters <= waterHalfWidth;
	sample.waterHalfWidth = waterHalfWidth;
	sample.crestHalfWidth = crestHalfWidth;
	sample.valleyHalfWidth = valleyHalfWidth;
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
	const double natural = m_planet.ElevationMeters(position, planet, minWavelengthMeters);
	const LocalWater water = WaterAt(position, natural, minWavelengthMeters);
	return natural - water.incisionMeters + water.bankRaiseMeters;
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
	if (m_world == nullptr)
	{
		return {};
	}
	const PlanetFieldSample planet = SampleAt(position);
	const double natural = m_planet.ElevationMeters(position, planet, minWavelengthMeters);
	return WaterAt(position, natural, minWavelengthMeters);
}

LocalWater WorldTerrain::WaterAt(const GeographicCoordinates &position, double naturalGroundMeters,
                                 double minWavelengthMeters) const noexcept
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

	const double localX = location.x * kRegionTileSizeMeters;
	const double localY = location.y * kRegionTileSizeMeters;
	const bool nearWest = localX <= kChannelSearchMarginMeters;
	const bool nearEast = kRegionTileSizeMeters - localX <= kChannelSearchMarginMeters;
	const bool nearSouth = localY <= kChannelSearchMarginMeters;
	const bool nearNorth = kRegionTileSizeMeters - localY <= kChannelSearchMarginMeters;
	const bool nearBorder = nearWest || nearEast || nearSouth || nearNorth;
	if (!tile.HasRiver() && !nearBorder)
	{
		return water;
	}

	// The closest channel decides both the water surface and the shape of the
	// ground around this column. The surface is sampled once at the closest
	// centreline point, so every column of one cross-section sees the same
	// level: the river follows the valley downhill instead of becoming a series
	// of independent pools.
	const RiverChannel *nearestChannel = nullptr;
	ChannelSample nearestSample;
	RegionTileId nearestTileId = location.id;
	double nearestDistance = std::numeric_limits<double>::infinity();
	const RiverChannel *waterChannel = nullptr;
	ChannelSample waterSample;
	RegionTileId waterTileId = location.id;
	double waterDistance = std::numeric_limits<double>::infinity();

	// Consider one tile's channels. `pointX/pointY` are in that tile's local
	// coordinates, so a channel from a neighbour can be measured without a
	// second conversion.
	const auto considerTile = [&](const RegionTile &candidateTile, RegionTileId candidateId) {
		const double pointX =
		    localX - static_cast<double>(candidateId.i - location.id.i) * kRegionTileSizeMeters;
		const double pointY =
		    localY - static_cast<double>(candidateId.j - location.id.j) * kRegionTileSizeMeters;
		for (const RiverChannel &river : m_world->RiversOf(candidateTile))
		{
			const ChannelSample channel = SampleChannel(river, pointX, pointY);
			if (!channel.inChannel)
			{
				continue;
			}
			if (channel.distanceMeters < nearestDistance)
			{
				nearestDistance = channel.distanceMeters;
				nearestChannel = &river;
				nearestSample = channel;
				nearestTileId = candidateId;
			}
			if (channel.inWater && channel.distanceMeters < waterDistance)
			{
				waterDistance = channel.distanceMeters;
				waterChannel = &river;
				waterSample = channel;
				waterTileId = candidateId;
			}
		}
	};

	considerTile(tile, location.id);
	if (nearBorder)
	{
		for (int deltaJ = -1; deltaJ <= 1; ++deltaJ)
		{
			if ((deltaJ < 0 && !nearSouth) || (deltaJ > 0 && !nearNorth))
			{
				continue;
			}
			for (int deltaI = -1; deltaI <= 1; ++deltaI)
			{
				if ((deltaI == 0 && deltaJ == 0) || (deltaI < 0 && !nearWest) || (deltaI > 0 && !nearEast))
				{
					continue;
				}
				const RegionTileId neighbourId{location.id.i + deltaI, location.id.j + deltaJ};
				if (WorldMap::Contains(neighbourId))
				{
					considerTile(m_world->At(neighbourId), neighbourId);
				}
			}
		}
	}
	if (nearestChannel == nullptr)
	{
		return water;
	}
	// Water wins over a nearby bank of another channel: a point covered by a
	// river must be wet even if the closest centreline belongs to a neighbour's
	// bank. This is what closes the gap where two channels meet at a region-tile
	// corner.
	if (waterChannel != nullptr)
	{
		nearestChannel = waterChannel;
		nearestSample = waterSample;
		nearestTileId = waterTileId;
	}

	// At a tile border the nearest channel can belong to the neighbour. Its
	// flow is the one that should travel with the water there.
	const RegionTile &flowTile = m_world->At(nearestTileId);
	const double riverFlowLength = std::sqrt(static_cast<double>(flowTile.flowEast) * flowTile.flowEast +
	                                         static_cast<double>(flowTile.flowNorth) * flowTile.flowNorth);
	if (riverFlowLength > 0.0)
	{
		water.flowEast = static_cast<double>(flowTile.flowEast) / riverFlowLength;
		water.flowNorth = static_cast<double>(flowTile.flowNorth) / riverFlowLength;
	}

	// The water surface follows the centreline ground, smoothed by the same
	// tile blend the terrain uses, so the river runs downhill with the land and
	// neighbouring tiles agree at the border.
	const double eastMeters =
	    (static_cast<double>(nearestTileId.i) - 0.5) * kRegionTileSizeMeters + nearestSample.centerX;
	const double northMeters =
	    (static_cast<double>(nearestTileId.j) - 0.5) * kRegionTileSizeMeters + nearestSample.centerY;
	const GeographicCoordinates centerPosition =
	    PlanetaryCoordinates::ToGeographic(m_world->Grid().Direction(eastMeters, northMeters));

	const auto blended = m_world->SampleAt(PlanetaryCoordinates::ToCartesian(centerPosition));
	PlanetFieldSample centerSample = m_planet.SampleAt(centerPosition);
	if (blended.has_value())
	{
		centerSample = ApplyTile(centerSample, &*blended);
	}
	const double groundMeters = m_planet.ElevationMeters(centerPosition, centerSample, minWavelengthMeters);
	const double surfaceMeters = groundMeters - kRiverFreeboardMeters;
	water.surfaceMeters = surfaceMeters;

	const double depth = static_cast<double>(nearestChannel->depthMeters);
	if (nearestSample.distanceMeters <= nearestSample.waterHalfWidth)
	{
		// Water bed: a flat bottom with a short shore at the water's edge. The
		// ground may only be cut down, never raised: a natural hollow is simply
		// deeper water.
		const double shoreWidth = std::min(nearestSample.waterHalfWidth,
		                                   std::max(kMinimumShoreWidthMeters, depth));
		const double bedHalfWidth = nearestSample.waterHalfWidth - shoreWidth;
		double bedMeters = surfaceMeters - depth;
		if (nearestSample.distanceMeters > bedHalfWidth && shoreWidth > 0.0)
		{
			const double shoreT =
			    (nearestSample.distanceMeters - bedHalfWidth) / std::max(shoreWidth, 1.0e-6);
			bedMeters = surfaceMeters - depth * (1.0 - SmoothStep(shoreT));
		}
		if (naturalGroundMeters > bedMeters)
		{
			water.incisionMeters = naturalGroundMeters - bedMeters;
		}
	}
	else
	{
		// Dry bank. The inner part rises to a crest just above the water; the
		// outer part fades that crest back into the natural terrain over a
		// wide valley, so the result reads as a river bank rather than a walled
		// canal. The crest is more than one block above the water, so even a
		// full top fluid cell stays below it; the block grid, not a special
		// fluid rule, is what contains the river.
		const double crestLevel = surfaceMeters + kBankLipMeters + kBankRiseMeters;
		if (nearestSample.distanceMeters <= nearestSample.crestHalfWidth)
		{
			const double innerWidth =
			    std::max(nearestSample.crestHalfWidth - nearestSample.waterHalfWidth, 1.0e-6);
			const double bankT = std::clamp(
			    (nearestSample.distanceMeters - nearestSample.waterHalfWidth) / innerWidth, 0.0, 1.0);
			const double crest = surfaceMeters + kBankLipMeters + kBankRiseMeters * SmoothStep(bankT);
			if (naturalGroundMeters < crest)
			{
				water.bankRaiseMeters = crest - naturalGroundMeters;
			}
		}
		else
		{
			const double outerWidth =
			    std::max(nearestSample.valleyHalfWidth - nearestSample.crestHalfWidth, 1.0e-6);
			const double valleyT = std::clamp(
			    (nearestSample.distanceMeters - nearestSample.crestHalfWidth) / outerWidth, 0.0, 1.0);
			const double decay = 1.0 - SmoothStep(valleyT);
			water.bankRaiseMeters = std::max(0.0, crestLevel - naturalGroundMeters) * decay;
		}
	}

	if (waterChannel == nullptr)
	{
		return water;
	}

	const double carvedGroundMeters = naturalGroundMeters - water.incisionMeters + water.bankRaiseMeters;
	water.origin = FluidOrigin::River;
	water.depthMeters = std::max(0.0, surfaceMeters - carvedGroundMeters);
	return water;
}
} // namespace Infinitesia

#include "domains/natural/planetary/world/WorldClimate.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace Infinitesia
{
namespace
{
constexpr int kSide = kRegionTilesPerWorldSide;
constexpr std::size_t kTileCount = static_cast<std::size_t>(kSide) * static_cast<std::size_t>(kSide);
constexpr float kUnreachable = std::numeric_limits<float>::infinity();

// Large water bodies store heat and pull the annual mean of the nearby land
// toward their surface temperature. Oceans reach farther inland than lakes.
constexpr double kOceanModerationScaleMeters = 25000.0;
constexpr double kLakeModerationScaleMeters = 6000.0;
constexpr double kOceanModerationStrength = 0.55;
constexpr double kLakeModerationStrength = 0.35;

// Continental interiors are colder in winter and hotter in summer. With only
// an annual mean on the world tile, the cold-side effect dominates where
// winters are severe, while warm interiors gain a small sensible-heat offset.
constexpr int kContinentalityRadiusTiles = 32; // about 25 km
constexpr double kContinentalHotOffset = 2.2;
constexpr double kContinentalColdOffset = 3.0;

// Moisture advection. Ocean air loses moisture over land; a short scale keeps
// the world's 197 km window from being climatically uniform.
constexpr double kOceanMoistureScaleMeters = 150000.0;
constexpr double kContinentalMoistureFactor = 0.55;
constexpr double kLakeMoistureStrength = 0.82;

// Orographic precipitation and the rain shadow behind high ground. The shadow
// fades with the distance to the upwind ridge, so a mountain 100 km away does
// not dry the lee as strongly as the ridge immediately upwind.
constexpr double kOrographicSlopeScale = 0.04; // 4 % slope gives full uplift
constexpr double kOrographicStrength = 0.9;
constexpr double kShadowDecayMeters = 45000.0;
constexpr double kShadowDepthMeters = 800.0;
constexpr int kMaxUpwindSteps = kSide;

struct NearestWater
{
	float distanceTiles = kUnreachable;
	float temperatureCelsius = 0.0f;
};

std::size_t Index(int x, int y) noexcept
{
	return static_cast<std::size_t>(y) * static_cast<std::size_t>(kSide) + static_cast<std::size_t>(x);
}

bool Inside(int x, int y) noexcept
{
	return x >= 0 && x < kSide && y >= 0 && y < kSide;
}

double Sign(double value) noexcept
{
	return value > 0.0 ? 1.0 : (value < 0.0 ? -1.0 : 0.0);
}

// Chamfer distance transform over 8 neighbours. Besides the distance it
// carries the surface temperature of the nearest source, so every land tile
// knows how far the nearest sea or lake is and how warm that water is. Two
// passes are enough for the small local window and keep the result
// deterministic.
void BuildWaterDistance(const std::vector<RegionTile> &tiles, bool wantLakes, std::vector<NearestWater> &nearest)
{
	nearest.assign(kTileCount, NearestWater{});

	for (int y = 0; y < kSide; ++y)
	{
		for (int x = 0; x < kSide; ++x)
		{
			const RegionTile &tile = tiles[Index(x, y)];
			const bool source = wantLakes ? (tile.IsLake() && !tile.IsOcean()) : tile.IsOcean();
			if (source)
			{
				NearestWater &entry = nearest[Index(x, y)];
				entry.distanceTiles = 0.0f;
				entry.temperatureCelsius = tile.temperatureCelsius;
			}
		}
	}

	constexpr float kOrthogonal = 1.0f;
	constexpr float kDiagonal = 1.41421356f;
	const auto relax = [&nearest](int x, int y, int neighbourX, int neighbourY, float step) {
		if (!Inside(neighbourX, neighbourY))
		{
			return;
		}
		const NearestWater &neighbour = nearest[Index(neighbourX, neighbourY)];
		if (neighbour.distanceTiles == kUnreachable)
		{
			return;
		}
		const float candidate = neighbour.distanceTiles + step;
		NearestWater &entry = nearest[Index(x, y)];
		if (candidate < entry.distanceTiles)
		{
			entry.distanceTiles = candidate;
			entry.temperatureCelsius = neighbour.temperatureCelsius;
		}
	};

	for (int y = 0; y < kSide; ++y)
	{
		for (int x = 0; x < kSide; ++x)
		{
			relax(x, y, x - 1, y, kOrthogonal);
			relax(x, y, x, y - 1, kOrthogonal);
			relax(x, y, x - 1, y - 1, kDiagonal);
			relax(x, y, x + 1, y - 1, kDiagonal);
		}
	}
	for (int y = kSide - 1; y >= 0; --y)
	{
		for (int x = kSide - 1; x >= 0; --x)
		{
			relax(x, y, x + 1, y, kOrthogonal);
			relax(x, y, x, y + 1, kOrthogonal);
			relax(x, y, x + 1, y + 1, kDiagonal);
			relax(x, y, x - 1, y + 1, kDiagonal);
		}
	}
}

// Land share in a square window, the local continentality of a tile. A coast
// has a low share, a deep interior a high one; using area rather than distance
// keeps a long narrow sea from reading as a continent.
void BuildLandFraction(const std::vector<RegionTile> &tiles, std::vector<float> &fraction)
{
	constexpr int kStride = kSide + 1;
	std::vector<int> integral(static_cast<std::size_t>(kStride) * static_cast<std::size_t>(kStride), 0);

	for (int y = 0; y < kSide; ++y)
	{
		int rowSum = 0;
		for (int x = 0; x < kSide; ++x)
		{
			const RegionTile &tile = tiles[Index(x, y)];
			rowSum += tile.IsLand() ? 1 : 0;
			integral[static_cast<std::size_t>(y + 1) * kStride + static_cast<std::size_t>(x + 1)] =
			    integral[static_cast<std::size_t>(y) * kStride + static_cast<std::size_t>(x + 1)] + rowSum;
		}
	}

	fraction.assign(kTileCount, 0.0f);
	const auto sum = [&integral](int x0, int y0, int x1, int y1) {
		return integral[static_cast<std::size_t>(y1) * kStride + static_cast<std::size_t>(x1)] -
		       integral[static_cast<std::size_t>(y0) * kStride + static_cast<std::size_t>(x1)] -
		       integral[static_cast<std::size_t>(y1) * kStride + static_cast<std::size_t>(x0)] +
		       integral[static_cast<std::size_t>(y0) * kStride + static_cast<std::size_t>(x0)];
	};

	for (int y = 0; y < kSide; ++y)
	{
		for (int x = 0; x < kSide; ++x)
		{
			const int x0 = std::max(0, x - kContinentalityRadiusTiles);
			const int x1 = std::min(kSide, x + kContinentalityRadiusTiles + 1);
			const int y0 = std::max(0, y - kContinentalityRadiusTiles);
			const int y1 = std::min(kSide, y + kContinentalityRadiusTiles + 1);
			const int land = sum(x0, y0, x1, y1);
			const int area = (x1 - x0) * (y1 - y0);
			fraction[Index(x, y)] = area > 0 ? static_cast<float>(land) / static_cast<float>(area) : 0.0f;
		}
	}
}

void ApplyLocalTemperature(std::vector<RegionTile> &tiles, const std::vector<NearestWater> &ocean,
                           const std::vector<NearestWater> &lake, const std::vector<float> &landFraction)
{
	for (std::size_t index = 0; index < kTileCount; ++index)
	{
		RegionTile &tile = tiles[index];
		if (!tile.IsLand())
		{
			continue;
		}

		double temperature = tile.temperatureCelsius;

		// Maritime moderation. Pull toward the temperature of the nearest water
		// tile; the influence fades inland.
		if (ocean[index].distanceTiles < kUnreachable)
		{
			const double distanceMeters =
			    static_cast<double>(ocean[index].distanceTiles) * kRegionTileSizeMeters;
			const double influence = std::exp(-distanceMeters / kOceanModerationScaleMeters);
			temperature += (static_cast<double>(ocean[index].temperatureCelsius) - temperature) *
			               kOceanModerationStrength * influence;
		}
		if (lake[index].distanceTiles < kUnreachable)
		{
			const double distanceMeters = static_cast<double>(lake[index].distanceTiles) * kRegionTileSizeMeters;
			const double influence = std::exp(-distanceMeters / kLakeModerationScaleMeters);
			temperature += (static_cast<double>(lake[index].temperatureCelsius) - temperature) *
			               kLakeModerationStrength * influence;
		}

		// Continentality: stronger annual range, with the cold-winter side
		// dominating the annual mean in cool climates and a small warm offset
		// in hot, dry interiors.
		const double continentality = landFraction[index];
		const double coldFactor = std::clamp((10.0 - temperature) / 30.0, 0.0, 1.0);
		const double hotFactor = std::clamp((temperature - 18.0) / 15.0, 0.0, 1.0);
		temperature += continentality * (kContinentalHotOffset * hotFactor - kContinentalColdOffset * coldFactor);

		tile.temperatureCelsius = static_cast<float>(std::clamp(temperature, -90.0, 60.0));
	}
}

void ApplyLocalPrecipitation(std::vector<RegionTile> &tiles, std::span<const GeographicCoordinates> positions,
                             const WorldClimateInputs &inputs)
{
	std::vector<float> basePrecipitation(kTileCount);
	for (std::size_t index = 0; index < kTileCount; ++index)
	{
		basePrecipitation[index] = tiles[index].precipitationMm;
	}

	const double shadowStepDecay = std::exp(-kRegionTileSizeMeters / kShadowDecayMeters);

	for (std::size_t index = 0; index < kTileCount; ++index)
	{
		RegionTile &tile = tiles[index];
		const double base = basePrecipitation[index];
		if (!tile.IsLand())
		{
			tile.precipitationMm = static_cast<float>(std::max(0.0, base));
			continue;
		}

		const PrevailingWind wind = WorldPrevailingWind(positions[index].latitudeDegrees, inputs.rotationPeriodHours);
		const double upwindEast = -wind.east;
		const double upwindNorth = -wind.north;
		const double largestComponent = std::max(std::abs(upwindEast), std::abs(upwindNorth));
		const double stepEast = largestComponent > 0.0 ? upwindEast / largestComponent : -1.0;
		const double stepNorth = largestComponent > 0.0 ? upwindNorth / largestComponent : 0.0;
		const double stepMeters = kRegionTileSizeMeters * std::hypot(stepEast, stepNorth);

		const int startX = static_cast<int>(index % static_cast<std::size_t>(kSide));
		const int startY = static_cast<int>(index / static_cast<std::size_t>(kSide));

		double landFetchMeters = 0.0;
		double sourceStrength = 0.0;
		bool foundWater = false;
		double immediateUpwindElevation = tile.elevationMeters;
		double shadowRise = 0.0;
		double shadowDecay = shadowStepDecay;

		for (int step = 1; step <= kMaxUpwindSteps; ++step)
		{
			const int sampleX = static_cast<int>(std::lround(static_cast<double>(startX) + stepEast * step));
			const int sampleY = static_cast<int>(std::lround(static_cast<double>(startY) + stepNorth * step));
			if (!Inside(sampleX, sampleY))
			{
				break;
			}

			const RegionTile &upwind = tiles[Index(sampleX, sampleY)];
			const double travelledMeters = static_cast<double>(step) * stepMeters;
			landFetchMeters = travelledMeters;

			if (step == 1)
			{
				immediateUpwindElevation = upwind.elevationMeters;
			}

			// A ridge only dries the lee if it stands above the current tile;
			// the influence fades with distance upwind.
			const double riseAboveTile = static_cast<double>(upwind.elevationMeters) - tile.elevationMeters;
			if (riseAboveTile > 0.0)
			{
				shadowRise = std::max(shadowRise, riseAboveTile * shadowDecay);
			}
			shadowDecay *= shadowStepDecay;

			if (upwind.IsOcean() || upwind.IsLake())
			{
				foundWater = true;
				sourceStrength = upwind.IsOcean() ? 1.0 : kLakeMoistureStrength;
				break;
			}
		}

		double moistureFactor = kContinentalMoistureFactor;
		if (foundWater)
		{
			moistureFactor =
			    0.60 + 0.40 * sourceStrength * std::exp(-landFetchMeters / kOceanMoistureScaleMeters);
		}

		// Windward slopes lift the air; the lee side of the upwind ridge is
		// dried by the same lift.
		const double immediateRise = static_cast<double>(tile.elevationMeters) - immediateUpwindElevation;
		const double slope = stepMeters > 0.0 ? immediateRise / stepMeters : 0.0;
		const double uplift = std::clamp(slope / kOrographicSlopeScale, 0.0, 1.5);
		const double windwardFactor = 1.0 + kOrographicStrength * uplift;
		const double rainShadowFactor = std::exp(-std::max(0.0, shadowRise) / kShadowDepthMeters);

		const double localFactor = std::clamp(moistureFactor * windwardFactor * rainShadowFactor, 0.05, 2.5);
		tile.precipitationMm = static_cast<float>(std::clamp(base * localFactor, 0.0, 12000.0));
	}
}
} // namespace

PrevailingWind WorldPrevailingWind(double latitudeDegrees, double rotationPeriodHours) noexcept
{
	const double absoluteLatitude = std::abs(latitudeDegrees);
	const double beltScale = std::clamp(24.0 / std::max(0.1, rotationPeriodHours), 0.4, 2.0);
	const double tradeLimit = 30.0 / beltScale;
	const double midLatitudeLimit = 60.0 / beltScale;

	double zonal = 1.0;
	double meridional = 0.0;
	if (absoluteLatitude < tradeLimit)
	{
		// Trade winds: easterly, blowing toward the equator.
		zonal = -1.0;
		meridional = -Sign(latitudeDegrees);
	}
	else if (absoluteLatitude < midLatitudeLimit)
	{
		// Westerlies: blowing poleward and eastward.
		zonal = 1.0;
		meridional = Sign(latitudeDegrees);
	}
	else
	{
		// Polar easterlies: cold air flowing back toward lower latitudes.
		zonal = -1.0;
		meridional = -Sign(latitudeDegrees);
	}

	// The zonal component dominates the annual mean; the meridional component
	// tilts the flow enough to matter for rain shadows.
	const double north = 0.35 * meridional;
	const double length = std::hypot(zonal, north);
	if (length <= 0.0)
	{
		return {-1.0, 0.0};
	}
	return {zonal / length, north / length};
}

void ComputeWorldClimate(std::vector<RegionTile> &tiles, std::span<const GeographicCoordinates> positions,
                         const WorldClimateInputs &inputs)
{
	if (tiles.size() != kTileCount || positions.size() != kTileCount)
	{
		return;
	}

	std::vector<NearestWater> ocean;
	std::vector<NearestWater> lake;
	std::vector<float> landFraction;
	BuildWaterDistance(tiles, false, ocean);
	BuildWaterDistance(tiles, true, lake);
	BuildLandFraction(tiles, landFraction);

	ApplyLocalTemperature(tiles, ocean, lake, landFraction);
	ApplyLocalPrecipitation(tiles, positions, inputs);
}
} // namespace Infinitesia

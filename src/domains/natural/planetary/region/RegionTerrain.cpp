#include "domains/natural/planetary/region/RegionTerrain.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Infinitesia
{
namespace
{
std::uint32_t HashLattice(int x, int y, int z, std::uint32_t seed) noexcept
{
	std::uint32_t hash = seed ^ 0x9E3779B9u;
	hash ^= static_cast<std::uint32_t>(x) * 0x85EBCA6Bu;
	hash = (hash << 13) | (hash >> 19);
	hash ^= static_cast<std::uint32_t>(y) * 0xC2B2AE3Du;
	hash = (hash << 11) | (hash >> 21);
	hash ^= static_cast<std::uint32_t>(z) * 0x27D4EB2Fu;
	hash ^= hash >> 16;
	hash *= 0x85EBCA6Bu;
	hash ^= hash >> 13;
	hash *= 0xC2B2AE3Du;
	hash ^= hash >> 16;
	return hash;
}

double LatticeValue(int x, int y, int z, std::uint32_t seed) noexcept
{
	return static_cast<double>(HashLattice(x, y, z, seed)) / 2147483647.5 - 1.0;
}

double Fade(double t) noexcept
{
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// 3D value noise in [-1, 1].
double ValueNoise(double px, double py, double pz, std::uint32_t seed) noexcept
{
	const double fx = std::floor(px);
	const double fy = std::floor(py);
	const double fz = std::floor(pz);
	const int x0 = static_cast<int>(fx);
	const int y0 = static_cast<int>(fy);
	const int z0 = static_cast<int>(fz);
	const double tx = Fade(px - fx);
	const double ty = Fade(py - fy);
	const double tz = Fade(pz - fz);

	const auto lerp = [](double a, double b, double t) { return a + (b - a) * t; };
	const auto corner = [&](int dx, int dy, int dz) { return LatticeValue(x0 + dx, y0 + dy, z0 + dz, seed); };

	const double x00 = lerp(corner(0, 0, 0), corner(1, 0, 0), tx);
	const double x10 = lerp(corner(0, 1, 0), corner(1, 1, 0), tx);
	const double x01 = lerp(corner(0, 0, 1), corner(1, 0, 1), tx);
	const double x11 = lerp(corner(0, 1, 1), corner(1, 1, 1), tx);
	return lerp(lerp(x00, x10, ty), lerp(x01, x11, ty), tz);
}

constexpr double kSoftAbsMean = 0.375;
constexpr double kCoastBlendMeters = 150.0;
// Each octave is this much weaker than the one before (half the wavelength):
// amplitude ~ wavelength, a rough natural-looking land. The first octave
// takes the share that makes the whole series add up to the detail amplitude.
constexpr double kOctaveDecay = 0.5;

// Micro-relief: even a plain undulates a metre or two over a hundred metres,
// which the series above would flatten to nothing where the planet cell is
// smooth. This second series fades much more slowly with wavelength (amplitude
// ~ wavelength^0.5), starts at kMicroLargestWavelengthMeters, and applies on
// land only.
constexpr double kMicroLargestWavelengthMeters = 512.0;
constexpr double kMicroAmplitudeMeters = 4.0;
constexpr double kMicroDecay = 0.7;

// A province boundary is a contact between two piles of rock, not a grid
// line. Within this distance of a planet-cell border the contact is pushed
// around by smooth noise, so it leaves the cell edge and becomes a wavy
// geological contact.
constexpr double kProvinceTransitionMeters = 400.0;
// Width of the mixed zone on either side of the wavy contact.
constexpr double kProvinceMixMeters = 160.0;
constexpr std::uint32_t kProvinceContactSalt = 0xB0A7C0DEu;

int WrapIndex(int index, int count) noexcept
{
	return ((index % count) + count) % count;
}

// The surface fields are carried together through the pole blend; this keeps
// the list in one place.
#define INFINITESIA_FOR_EACH_SURFACE_FIELD(M)                                                                          \
	M(temperatureCelsius)                                                                                              \
	M(precipitationMm)                                                                                                 \
	M(drySeasonMonths)                                                                                                 \
	M(snowDays)                                                                                                        \
	M(humidityNormalized)                                                                                              \
	M(soilMoisture)                                                                                                    \
	M(soilFertility)                                                                                                   \
	M(soilDepthNorm)                                                                                                   \
	M(drainageAccumulation)                                                                                            \
	M(lakeFraction)                                                                                                    \
	M(lakeWaterLevelKm)                                                                                                \
	M(lakeSpillHeightKm)                                                                                               \
	M(tectonicStress)

void AddSurface(PlanetSurface &sum, const PlanetRegion &region) noexcept
{
#define INFINITESIA_ADD_SURFACE_FIELD(field) sum.field += region.field;
	INFINITESIA_FOR_EACH_SURFACE_FIELD(INFINITESIA_ADD_SURFACE_FIELD)
#undef INFINITESIA_ADD_SURFACE_FIELD
	sum.volcanicFraction += region.isVolcanicZone ? 1.0 : 0.0;
}

void ScaleSurface(PlanetSurface &surface, double scale) noexcept
{
#define INFINITESIA_SCALE_SURFACE_FIELD(field) surface.field *= scale;
	INFINITESIA_FOR_EACH_SURFACE_FIELD(INFINITESIA_SCALE_SURFACE_FIELD)
#undef INFINITESIA_SCALE_SURFACE_FIELD
	surface.volcanicFraction *= scale;
}

void BlendSurface(PlanetSurface &surface, const PlanetSurface &pole, double toward) noexcept
{
#define INFINITESIA_BLEND_SURFACE_FIELD(field) surface.field = pole.field + (surface.field - pole.field) * toward;
	INFINITESIA_FOR_EACH_SURFACE_FIELD(INFINITESIA_BLEND_SURFACE_FIELD)
#undef INFINITESIA_BLEND_SURFACE_FIELD
	surface.volcanicFraction = pole.volcanicFraction + (surface.volcanicFraction - pole.volcanicFraction) * toward;
}

void ClampSurface(PlanetSurface &surface) noexcept
{
	surface.humidityNormalized = std::clamp(surface.humidityNormalized, 0.0, 1.0);
	surface.soilMoisture = std::clamp(surface.soilMoisture, 0.0, 1.0);
	surface.soilFertility = std::clamp(surface.soilFertility, 0.0, 1.0);
	surface.soilDepthNorm = std::clamp(surface.soilDepthNorm, 0.0, 1.0);
	surface.drainageAccumulation = std::clamp(surface.drainageAccumulation, 0.0, 1.0);
	surface.lakeFraction = std::clamp(surface.lakeFraction, 0.0, 1.0);
	surface.tectonicStress = std::clamp(surface.tectonicStress, 0.0, 1.0);
	surface.volcanicFraction = std::clamp(surface.volcanicFraction, 0.0, 1.0);
	surface.precipitationMm = std::max(0.0, surface.precipitationMm);
	surface.lakeWaterLevelKm = std::max(0.0, surface.lakeWaterLevelKm);
	surface.lakeSpillHeightKm = std::max(0.0, surface.lakeSpillHeightKm);
}
} // namespace

RegionTerrain::RegionTerrain(const PlanetMap &map) noexcept
    : m_map(map), m_radiusMeters(map.GetPlanet().GetRadius()), m_seed(map.GetPlanet().GetSeed() ^ 0x5EED5EEDu)
{
	const int latitudeBands = map.GetLatitudeBandCount();
	const int longitudeSegments = map.GetLongitudeSegmentCount();
	for (int pole = 0; pole < 2; ++pole)
	{
		const int row = pole == 0 ? 0 : latitudeBands - 1;
		PoleMeans means;
		for (int longitude = 0; longitude < longitudeSegments; ++longitude)
		{
			const PlanetRegion &region = map.GetRegion(row, longitude);
			means.elevation += region.elevationNormalized;
			means.roughness += region.slopeNormalized;
			AddSurface(means.surface, region);
		}
		const double scale = 1.0 / static_cast<double>(longitudeSegments);
		means.elevation *= scale;
		means.roughness *= scale;
		ScaleSurface(means.surface, scale);
		m_pole[pole] = means;
	}
}

PlanetFieldSample RegionTerrain::SampleAt(const GeographicCoordinates &position) const noexcept
{
	const int latitudeBands = m_map.GetLatitudeBandCount();
	const int longitudeSegments = m_map.GetLongitudeSegmentCount();

	// Continuous cell coordinates with cell centres at integer positions.
	const double latitudeCell = (position.latitudeDegrees + 90.0) / 180.0 * latitudeBands - 0.5;
	const double longitudeCell = position.longitudeDegrees / 360.0 * longitudeSegments - 0.5;
	const double latitudeFloor = std::floor(latitudeCell);
	const double longitudeFloor = std::floor(longitudeCell);
	const double tLatitude = latitudeCell - latitudeFloor;
	const double tLongitude = longitudeCell - longitudeFloor;

	const int la0 = std::clamp(static_cast<int>(latitudeFloor), 0, latitudeBands - 1);
	const int la1 = std::clamp(static_cast<int>(latitudeFloor) + 1, 0, latitudeBands - 1);
	const int lo0 = WrapIndex(static_cast<int>(longitudeFloor), longitudeSegments);
	const int lo1 = WrapIndex(static_cast<int>(longitudeFloor) + 1, longitudeSegments);

	const auto bilinear = [&](auto field) {
		const double a =
		    field(m_map.GetRegion(la0, lo0)) * (1.0 - tLongitude) + field(m_map.GetRegion(la0, lo1)) * tLongitude;
		const double b =
		    field(m_map.GetRegion(la1, lo0)) * (1.0 - tLongitude) + field(m_map.GetRegion(la1, lo1)) * tLongitude;
		return a * (1.0 - tLatitude) + b * tLatitude;
	};

	// Bilinear interpolation leaves slope kinks along cell borders that read as
	// straight creases at region scale, so elevation uses Catmull-Rom instead.
	const auto catmullRom = [](double p0, double p1, double p2, double p3, double t) {
		return p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0)));
	};
	const int baseLatitude = static_cast<int>(latitudeFloor);
	const int baseLongitude = static_cast<int>(longitudeFloor);
	double rows[4];
	for (int row = 0; row < 4; ++row)
	{
		const int la = std::clamp(baseLatitude - 1 + row, 0, latitudeBands - 1);
		double columns[4];
		for (int column = 0; column < 4; ++column)
		{
			const int lo = WrapIndex(baseLongitude - 1 + column, longitudeSegments);
			columns[column] = m_map.GetRegion(la, lo).elevationNormalized;
		}
		rows[row] = catmullRom(columns[0], columns[1], columns[2], columns[3], tLongitude);
	}
	double elevationNormalized = catmullRom(rows[0], rows[1], rows[2], rows[3], tLatitude);
	double roughness = bilinear([](const PlanetRegion &r) { return r.slopeNormalized; });

	PlanetSurface surface;
#define INFINITESIA_SAMPLE_SURFACE_FIELD(field) surface.field = bilinear([](const PlanetRegion &r) { return r.field; });
	INFINITESIA_FOR_EACH_SURFACE_FIELD(INFINITESIA_SAMPLE_SURFACE_FIELD)
#undef INFINITESIA_SAMPLE_SURFACE_FIELD
	surface.volcanicFraction = bilinear([](const PlanetRegion &r) { return r.isVolcanicZone ? 1.0 : 0.0; });

	// Lakes are small bodies, not a smooth climate field: keep the level of the
	// cell with the most lake cover while the coverage itself interpolates. A
	// level interpolated towards a dry cell would tilt or truncate the lake at
	// the cell border.
	double bestLakeFraction = 0.0;
	for (int row = 0; row < 2; ++row)
	{
		for (int column = 0; column < 2; ++column)
		{
			const PlanetRegion &region = m_map.GetRegion(row == 0 ? la0 : la1, column == 0 ? lo0 : lo1);
			if (region.lakeFraction > bestLakeFraction)
			{
				bestLakeFraction = region.lakeFraction;
				surface.lakeWaterLevelKm = region.lakeWaterLevelKm;
				surface.lakeSpillHeightKm = region.lakeSpillHeightKm;
			}
		}
	}

	// Between the centre of the polar row and the pole itself, fade the variation
	// with longitude out: a point at the pole belongs to every longitude at once.
	const double cellDegrees = 180.0 / latitudeBands;
	const double poleDistance = 90.0 - std::abs(position.latitudeDegrees);
	if (poleDistance < 0.5 * cellDegrees)
	{
		const double t = std::clamp(poleDistance / (0.5 * cellDegrees), 0.0, 1.0);
		const double toward = t * t * (3.0 - 2.0 * t); // 1 away from the pole, 0 at the pole
		const int pole = position.latitudeDegrees > 0.0 ? 1 : 0;
		elevationNormalized = m_pole[pole].elevation + (elevationNormalized - m_pole[pole].elevation) * toward;
		roughness = m_pole[pole].roughness + (roughness - m_pole[pole].roughness) * toward;
		BlendSurface(surface, m_pole[pole].surface, toward);
	}

	ClampSurface(surface);

	const int nearestLatitude = std::clamp(
	    static_cast<int>(std::floor((position.latitudeDegrees + 90.0) / 180.0 * latitudeBands)), 0, latitudeBands - 1);
	const int nearestLongitude = WrapIndex(
	    static_cast<int>(std::floor(position.longitudeDegrees / 360.0 * longitudeSegments)), longitudeSegments);
	surface.province = m_map.GetRegion(nearestLatitude, nearestLongitude).provinceId;

	// Where the nearest cell border is also a province border, let smooth noise
	// move the contact away from the grid line. The rule is symmetric across the
	// border: the two provinces are ordered by id and the signed distance from
	// the border decides which side of the noise contour is which, so both sides
	// agree on the contact.
	const double heightMeters = std::numbers::pi * m_radiusMeters / static_cast<double>(latitudeBands);
	const double widthMeters = std::max(1.0, 2.0 * std::numbers::pi * m_radiusMeters *
	                                             std::cos(position.latitudeDegrees * std::numbers::pi / 180.0) /
	                                             static_cast<double>(longitudeSegments));
	const double northDistance = (static_cast<double>(nearestLatitude) + 0.5 - latitudeCell) * heightMeters;
	const double southDistance = (latitudeCell - (static_cast<double>(nearestLatitude) - 0.5)) * heightMeters;
	double longitudeDelta = longitudeCell - static_cast<double>(nearestLongitude);
	while (longitudeDelta > static_cast<double>(longitudeSegments) * 0.5)
	{
		longitudeDelta -= static_cast<double>(longitudeSegments);
	}
	while (longitudeDelta < -static_cast<double>(longitudeSegments) * 0.5)
	{
		longitudeDelta += static_cast<double>(longitudeSegments);
	}
	const double eastDistance = (0.5 - longitudeDelta) * widthMeters;
	const double westDistance = (0.5 + longitudeDelta) * widthMeters;

	double borderDistance = northDistance;
	int neighbourLatitude = std::min(nearestLatitude + 1, latitudeBands - 1);
	int neighbourLongitude = nearestLongitude;
	if (southDistance < borderDistance)
	{
		borderDistance = southDistance;
		neighbourLatitude = std::max(nearestLatitude - 1, 0);
		neighbourLongitude = nearestLongitude;
	}
	if (eastDistance < borderDistance)
	{
		borderDistance = eastDistance;
		neighbourLatitude = nearestLatitude;
		neighbourLongitude = WrapIndex(nearestLongitude + 1, longitudeSegments);
	}
	if (westDistance < borderDistance)
	{
		borderDistance = westDistance;
		neighbourLatitude = nearestLatitude;
		neighbourLongitude = WrapIndex(nearestLongitude - 1, longitudeSegments);
	}

	const ProvinceId neighbourProvince = m_map.GetRegion(neighbourLatitude, neighbourLongitude).provinceId;
	if (neighbourProvince != surface.province && borderDistance < kProvinceTransitionMeters)
	{
		const ProvinceId first = std::min(surface.province, neighbourProvince);
		const ProvinceId second = std::max(surface.province, neighbourProvince);
		const double signedDistance = surface.province == first ? borderDistance : -borderDistance;
		const CartesianPoint3D direction = PlanetaryCoordinates::ToCartesian(position);
		const double px = direction.x * m_radiusMeters;
		const double py = direction.y * m_radiusMeters;
		const double pz = direction.z * m_radiusMeters;
		const double contactNoise = ValueNoise(px / kProvinceTransitionMeters, py / kProvinceTransitionMeters,
		                                       pz / kProvinceTransitionMeters, m_seed ^ kProvinceContactSalt);
		surface.province = contactNoise > signedDistance / kProvinceTransitionMeters ? second : first;
		surface.secondaryProvince = surface.province == first ? second : first;
		// Distance from the actual (noise-displaced) contact. The mixture is
		// strongest on the contact and fades out over kProvinceMixMeters.
		const double distanceToContact = std::abs(signedDistance - contactNoise * kProvinceTransitionMeters);
		const double mixT = std::clamp(1.0 - distanceToContact / kProvinceMixMeters, 0.0, 1.0);
		const double mix = mixT * mixT * (3.0 - 2.0 * mixT);
		surface.secondaryWeight = 0.5 * mix;
		if (surface.secondaryWeight <= 0.0)
		{
			surface.secondaryProvince = kNoProvince;
		}
	}

	PlanetFieldSample sample;
	sample.elevationMeters = std::clamp(elevationNormalized, -1.0, 1.0) * kMaxElevationKm * 1000.0;
	sample.roughness = std::clamp(roughness, 0.0, 1.0);
	sample.nearestLatitudeIndex = nearestLatitude;
	sample.nearestLongitudeIndex = nearestLongitude;
	sample.surface = surface;
	return sample;
}

PlanetFieldSample RegionTerrain::SamplePlanet(CartesianPoint3D direction) const noexcept
{
	return SampleAt(PlanetaryCoordinates::ToGeographic(direction));
}

double RegionTerrain::ElevationMeters(const GeographicCoordinates &position, double minWavelengthMeters) const noexcept
{
	return ElevationMeters(position, SampleAt(position), minWavelengthMeters);
}

double RegionTerrain::ElevationMeters(const GeographicCoordinates &position, const PlanetFieldSample &planet,
                                      double minWavelengthMeters) const noexcept
{
	const double roughness = planet.roughness;

	// Total detail amplitude is fixed per location, so a finer minimum
	// wavelength only adds octaves and never rescales the coarse ones.
	// Sea floor and land differ in character, but the switch is blended over
	// +/- kCoastBlendMeters of coarse elevation: a hard switch at the zero
	// contour would leave a visible cliff along it.
	const double seaFloorAmplitude = 20.0 + 150.0 * roughness;
	const double landAmplitude = 60.0 + 1400.0 * roughness * std::sqrt(roughness);
	const double t = std::clamp(planet.elevationMeters / (2.0 * kCoastBlendMeters) + 0.5, 0.0, 1.0);
	const double land = t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
	const double detailAmplitude = seaFloorAmplitude + (landAmplitude - seaFloorAmplitude) * land;
	const double ridgeMix = land * std::clamp(roughness * 1.5, 0.0, 1.0);

	const CartesianPoint3D direction = PlanetaryCoordinates::ToCartesian(position);
	const double px = direction.x * m_radiusMeters;
	const double py = direction.y * m_radiusMeters;
	const double pz = direction.z * m_radiusMeters;

	double detail = 0.0;
	double amplitude = detailAmplitude * (1.0 - kOctaveDecay);
	double wavelength = kLargestDetailWavelengthMeters;
	for (int octave = 0; wavelength >= minWavelengthMeters; ++octave)
	{
		const double offset = octave * 7919.0;
		const double noise =
		    ValueNoise(px / wavelength + offset, py / wavelength + offset * 0.37, pz / wavelength + offset * 0.71,
			           m_seed + static_cast<std::uint32_t>(octave) * 0x9E3779B1u);
		// Softened |noise| keeps crests from becoming razor creases; the constant
		// is the measured mean of the softened value so the detail stays zero-mean.
		const double ridged = 2.0 * (kSoftAbsMean - std::sqrt(noise * noise + 0.02));
		detail += amplitude * (noise * (1.0 - ridgeMix) + ridged * ridgeMix);
		amplitude *= kOctaveDecay;
		wavelength *= 0.5;
	}

	// Micro-relief, a little stronger on rougher ground (a plain keeps half of
	// it).
	const double microScale = land * (0.5 + 0.5 * roughness);
	double micro = 0.0;
	if (microScale > 0.0)
	{
		double microAmplitude = kMicroAmplitudeMeters * microScale;
		double microWavelength = kMicroLargestWavelengthMeters;
		for (int octave = 0; microWavelength >= minWavelengthMeters; ++octave)
		{
			const double offset = 104729.0 + octave * 6007.0;
			micro +=
			    microAmplitude * ValueNoise(px / microWavelength + offset, py / microWavelength + offset * 0.53,
				                            pz / microWavelength + offset * 0.29,
				                            m_seed ^ (0xA5A5A5A5u + static_cast<std::uint32_t>(octave) * 0x7F4A7C15u));
			microAmplitude *= kMicroDecay;
			microWavelength *= 0.5;
		}
	}

	return planet.elevationMeters + detail + micro;
}

double RegionTerrain::ElevationMeters(CartesianPoint3D direction, double minWavelengthMeters) const noexcept
{
	return ElevationMeters(PlanetaryCoordinates::ToGeographic(direction), minWavelengthMeters);
}

double RegionTerrain::ElevationMeters(CartesianPoint3D direction, const PlanetFieldSample &planet,
                                      double minWavelengthMeters) const noexcept
{
	return ElevationMeters(PlanetaryCoordinates::ToGeographic(direction), planet, minWavelengthMeters);
}
} // namespace Infinitesia

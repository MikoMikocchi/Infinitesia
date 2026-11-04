#include "domains/natural/planetary/region/RegionTerrain.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia {
namespace {
std::uint32_t HashLattice(int x, int y, int z, std::uint32_t seed) noexcept {
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

double LatticeValue(int x, int y, int z, std::uint32_t seed) noexcept {
  return static_cast<double>(HashLattice(x, y, z, seed)) / 2147483647.5 - 1.0;
}

double Fade(double t) noexcept {
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// 3D value noise in [-1, 1].
double ValueNoise(double px, double py, double pz,
                  std::uint32_t seed) noexcept {
  const double fx = std::floor(px);
  const double fy = std::floor(py);
  const double fz = std::floor(pz);
  const int x0 = static_cast<int>(fx);
  const int y0 = static_cast<int>(fy);
  const int z0 = static_cast<int>(fz);
  const double tx = Fade(px - fx);
  const double ty = Fade(py - fy);
  const double tz = Fade(pz - fz);

  const auto lerp = [](double a, double b, double t) {
    return a + (b - a) * t;
  };
  const auto corner = [&](int dx, int dy, int dz) {
    return LatticeValue(x0 + dx, y0 + dy, z0 + dz, seed);
  };

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

int WrapIndex(int index, int count) noexcept {
  return ((index % count) + count) % count;
}
}  // namespace

RegionTerrain::RegionTerrain(const PlanetMap& map) noexcept
    : m_map(map),
      m_radiusMeters(map.GetPlanet().GetRadius()),
      m_seed(map.GetPlanet().GetSeed() ^ 0x5EED5EEDu) {
  const int latitudeBands = map.GetLatitudeBandCount();
  const int longitudeSegments = map.GetLongitudeSegmentCount();
  for (int pole = 0; pole < 2; ++pole) {
    const int row = pole == 0 ? 0 : latitudeBands - 1;
    double elevation = 0.0;
    double roughness = 0.0;
    for (int longitude = 0; longitude < longitudeSegments; ++longitude) {
      elevation += map.GetRegion(row, longitude).elevationNormalized;
      roughness += map.GetRegion(row, longitude).slopeNormalized;
    }
    m_poleElevation[pole] = elevation / longitudeSegments;
    m_poleRoughness[pole] = roughness / longitudeSegments;
  }
}

PlanetFieldSample RegionTerrain::SamplePlanet(
    CartesianPoint3D direction) const noexcept {
  const GeographicCoordinates geo =
      PlanetaryCoordinates::ToGeographic(direction);
  const int latitudeBands = m_map.GetLatitudeBandCount();
  const int longitudeSegments = m_map.GetLongitudeSegmentCount();

  // Continuous cell coordinates with cell centres at integer positions.
  const double latitudeCell =
      (geo.latitudeDegrees + 90.0) / 180.0 * latitudeBands - 0.5;
  const double longitudeCell =
      geo.longitudeDegrees / 360.0 * longitudeSegments - 0.5;
  const double latitudeFloor = std::floor(latitudeCell);
  const double longitudeFloor = std::floor(longitudeCell);
  const double tLatitude = latitudeCell - latitudeFloor;
  const double tLongitude = longitudeCell - longitudeFloor;

  const int la0 =
      std::clamp(static_cast<int>(latitudeFloor), 0, latitudeBands - 1);
  const int la1 =
      std::clamp(static_cast<int>(latitudeFloor) + 1, 0, latitudeBands - 1);
  const int lo0 =
      WrapIndex(static_cast<int>(longitudeFloor), longitudeSegments);
  const int lo1 =
      WrapIndex(static_cast<int>(longitudeFloor) + 1, longitudeSegments);

  const auto bilinear = [&](auto field) {
    const double a = field(m_map.GetRegion(la0, lo0)) * (1.0 - tLongitude) +
                     field(m_map.GetRegion(la0, lo1)) * tLongitude;
    const double b = field(m_map.GetRegion(la1, lo0)) * (1.0 - tLongitude) +
                     field(m_map.GetRegion(la1, lo1)) * tLongitude;
    return a * (1.0 - tLatitude) + b * tLatitude;
  };

  // Bilinear interpolation leaves slope kinks along cell borders that read as
  // straight creases at region scale, so elevation uses Catmull-Rom instead.
  const auto catmullRom = [](double p0, double p1, double p2, double p3,
                             double t) {
    return p1 + 0.5 * t *
                    (p2 - p0 +
                     t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 +
                          t * (3.0 * (p1 - p2) + p3 - p0)));
  };
  const int baseLatitude = static_cast<int>(latitudeFloor);
  const int baseLongitude = static_cast<int>(longitudeFloor);
  double rows[4];
  for (int row = 0; row < 4; ++row) {
    const int la = std::clamp(baseLatitude - 1 + row, 0, latitudeBands - 1);
    double columns[4];
    for (int column = 0; column < 4; ++column) {
      const int lo = WrapIndex(baseLongitude - 1 + column, longitudeSegments);
      columns[column] = m_map.GetRegion(la, lo).elevationNormalized;
    }
    rows[row] =
        catmullRom(columns[0], columns[1], columns[2], columns[3], tLongitude);
  }
  double elevationNormalized =
      catmullRom(rows[0], rows[1], rows[2], rows[3], tLatitude);
  double roughness =
      bilinear([](const PlanetRegion& r) { return r.slopeNormalized; });

  // Between the centre of the polar row and the pole itself, fade the variation
  // with longitude out: a point at the pole belongs to every longitude at once.
  const double cellDegrees = 180.0 / latitudeBands;
  const double poleDistance = 90.0 - std::abs(geo.latitudeDegrees);
  if (poleDistance < 0.5 * cellDegrees) {
    const double t = std::clamp(poleDistance / (0.5 * cellDegrees), 0.0, 1.0);
    const double toward =
        t * t * (3.0 - 2.0 * t);  // 1 away from the pole, 0 at the pole
    const int pole = geo.latitudeDegrees > 0.0 ? 1 : 0;
    elevationNormalized =
        m_poleElevation[pole] +
        (elevationNormalized - m_poleElevation[pole]) * toward;
    roughness =
        m_poleRoughness[pole] + (roughness - m_poleRoughness[pole]) * toward;
  }

  PlanetFieldSample sample;
  sample.elevationMeters =
      std::clamp(elevationNormalized, -1.0, 1.0) * kMaxElevationKm * 1000.0;
  sample.roughness = std::clamp(roughness, 0.0, 1.0);
  sample.nearestLatitudeIndex =
      std::clamp(static_cast<int>(std::floor((geo.latitudeDegrees + 90.0) /
                                             180.0 * latitudeBands)),
                 0, latitudeBands - 1);
  sample.nearestLongitudeIndex =
      WrapIndex(static_cast<int>(std::floor(geo.longitudeDegrees / 360.0 *
                                            longitudeSegments)),
                longitudeSegments);
  return sample;
}

double RegionTerrain::ElevationMeters(
    CartesianPoint3D direction, double minWavelengthMeters) const noexcept {
  return ElevationMeters(direction, SamplePlanet(direction),
                         minWavelengthMeters);
}

double RegionTerrain::ElevationMeters(
    CartesianPoint3D direction, const PlanetFieldSample& planet,
    double minWavelengthMeters) const noexcept {
  const double roughness = planet.roughness;

  // Total detail amplitude is fixed per location, so a finer minimum
  // wavelength only adds octaves and never rescales the coarse ones.
  // Sea floor and land differ in character, but the switch is blended over
  // +/- kCoastBlendMeters of coarse elevation: a hard switch at the zero
  // contour would leave a visible cliff along it.
  const double seaFloorAmplitude = 20.0 + 150.0 * roughness;
  const double landAmplitude = 60.0 + 1400.0 * roughness * std::sqrt(roughness);
  const double t = std::clamp(
      planet.elevationMeters / (2.0 * kCoastBlendMeters) + 0.5, 0.0, 1.0);
  const double land = t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
  const double detailAmplitude =
      seaFloorAmplitude + (landAmplitude - seaFloorAmplitude) * land;
  const double ridgeMix = land * std::clamp(roughness * 1.5, 0.0, 1.0);

  const double px = direction.x * m_radiusMeters;
  const double py = direction.y * m_radiusMeters;
  const double pz = direction.z * m_radiusMeters;

  double detail = 0.0;
  double amplitude = detailAmplitude * (1.0 - kOctaveDecay);
  double wavelength = kLargestDetailWavelengthMeters;
  for (int octave = 0; wavelength >= minWavelengthMeters; ++octave) {
    const double offset = octave * 7919.0;
    const double noise =
        ValueNoise(px / wavelength + offset, py / wavelength + offset * 0.37,
                   pz / wavelength + offset * 0.71,
                   m_seed + static_cast<std::uint32_t>(octave) * 0x9E3779B1u);
    // Softened |noise| keeps crests from becoming razor creases; the constant
    // is the measured mean of the softened value so the detail stays zero-mean.
    const double ridged =
        2.0 * (kSoftAbsMean - std::sqrt(noise * noise + 0.02));
    detail += amplitude * (noise * (1.0 - ridgeMix) + ridged * ridgeMix);
    amplitude *= kOctaveDecay;
    wavelength *= 0.5;
  }

  // Micro-relief, a little stronger on rougher ground (a plain keeps half of
  // it).
  const double microScale = land * (0.5 + 0.5 * roughness);
  double micro = 0.0;
  if (microScale > 0.0) {
    double microAmplitude = kMicroAmplitudeMeters * microScale;
    double microWavelength = kMicroLargestWavelengthMeters;
    for (int octave = 0; microWavelength >= minWavelengthMeters; ++octave) {
      const double offset = 104729.0 + octave * 6007.0;
      micro += microAmplitude *
               ValueNoise(
                   px / microWavelength + offset,
                   py / microWavelength + offset * 0.53,
                   pz / microWavelength + offset * 0.29,
                   m_seed ^ (0xA5A5A5A5u +
                             static_cast<std::uint32_t>(octave) * 0x7F4A7C15u));
      microAmplitude *= kMicroDecay;
      microWavelength *= 0.5;
    }
  }

  return planet.elevationMeters + detail + micro;
}
}  // namespace Infinitesia

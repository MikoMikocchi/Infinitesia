#include "domains/natural/planetary/Planet.h"

#include <algorithm>
#include <utility>

namespace Infinitesia {
namespace {
int ClampRegionIndex(int index, int maximum) {
  if (maximum <= 0) {
    return 0;
  }
  return std::clamp(index, 0, maximum - 1);
}
}  // namespace

Planet::Planet(PlanetParameters parameters)
    : m_parameters(std::move(parameters)) {
  if (!m_parameters.IsValid()) {
    m_parameters = PlanetParameters{};
  }
}

const PlanetParameters& Planet::GetParameters() const noexcept {
  return m_parameters;
}

std::uint32_t Planet::GetSeed() const noexcept { return m_parameters.seed; }

double Planet::GetRadius() const noexcept { return m_parameters.radiusMeters; }

bool Planet::IsValid() const noexcept { return m_parameters.IsValid(); }

GeographicCoordinates Planet::GetRegionCenter(
    int latitudeIndex, int longitudeIndex) const noexcept {
  const int safeLatitudeBands = std::max(1, m_parameters.latitudeBands);
  const int safeLongitudeSegments = std::max(1, m_parameters.longitudeSegments);

  const int clampedLatitudeIndex =
      ClampRegionIndex(latitudeIndex, safeLatitudeBands);
  const int clampedLongitudeIndex =
      ClampRegionIndex(longitudeIndex, safeLongitudeSegments);

  const double latitudeDegrees =
      (static_cast<double>(clampedLatitudeIndex) + 0.5) / safeLatitudeBands *
          180.0 -
      90.0;
  const double longitudeDegrees =
      (static_cast<double>(clampedLongitudeIndex) + 0.5) /
      safeLongitudeSegments * 360.0;

  return {latitudeDegrees, longitudeDegrees};
}

CartesianPoint3D Planet::GetSurfacePoint(int latitudeIndex,
                                         int longitudeIndex) const noexcept {
  return PlanetaryCoordinates::ToCartesian(
      GetRegionCenter(latitudeIndex, longitudeIndex), GetRadius());
}

std::uint32_t Planet::DeriveRegionSeed(int latitudeIndex,
                                       int longitudeIndex) const noexcept {
  const int safeLatitudeBands = std::max(1, m_parameters.latitudeBands);
  const int safeLongitudeSegments = std::max(1, m_parameters.longitudeSegments);

  const int clampedLatitudeIndex =
      ClampRegionIndex(latitudeIndex, safeLatitudeBands);
  const int clampedLongitudeIndex =
      ClampRegionIndex(longitudeIndex, safeLongitudeSegments);

  return PlanetaryCoordinates::HashRegion(
      clampedLatitudeIndex, clampedLongitudeIndex, m_parameters.seed);
}
}  // namespace Infinitesia

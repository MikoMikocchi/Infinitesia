#pragma once

#include <cstdint>

#include "domains/natural/planetary/PlanetParameters.h"
#include "core/PlanetaryCoordinates.h"

namespace Infinitesia {
class Planet {
 public:
  explicit Planet(PlanetParameters parameters = {});

  const PlanetParameters& GetParameters() const noexcept;
  std::uint32_t GetSeed() const noexcept;
  double GetRadius() const noexcept;
  bool IsValid() const noexcept;

  GeographicCoordinates GetRegionCenter(int latitudeIndex,
                                        int longitudeIndex) const noexcept;
  CartesianPoint3D GetSurfacePoint(int latitudeIndex,
                                   int longitudeIndex) const noexcept;
  std::uint32_t DeriveRegionSeed(int latitudeIndex,
                                 int longitudeIndex) const noexcept;

 private:
  PlanetParameters m_parameters;
};
}  // namespace Infinitesia

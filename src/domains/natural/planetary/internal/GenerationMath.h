#pragma once

#include <cstdint>

// Deterministic hashing, noise and spherical geometry shared by the planet
// stages.
namespace Infinitesia::GenerationDetail {
struct Vec3d {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

std::uint32_t Hash32(std::uint32_t value);
std::uint32_t HashCombine(std::uint32_t first, std::uint32_t second);
double RandUnit(std::uint32_t seed, std::uint32_t tag);
double RandUnit(std::uint32_t seed, int latitude, int longitude);
double RandRange(std::uint32_t seed, std::uint32_t tag, double minimum,
                 double maximum);

double SmootherStep(double value);
double Lerp(double first, double second, double amount);

double ValueNoise2D(double normalizedLatitude, double normalizedLongitude,
                    int latitudeCells, int longitudeCells, std::uint32_t seed);
double fBm(double normalizedLatitude, double normalizedLongitude,
           std::uint32_t seed, int baseLatitudeCells, int baseLongitudeCells,
           int octaves);
double fBmGrid(int latitudeIndex, int longitudeIndex, int latitudeBands,
               int longitudeSegments, std::uint32_t seed, int baseLatitudeCells,
               int baseLongitudeCells, int octaves);
double SphericalFbm(double latitudeDegrees, double longitudeDegrees,
                    std::uint32_t seed, int baseCells, int octaves);

Vec3d LatLonToCart(double latitudeDegrees, double longitudeDegrees);
double Dot(const Vec3d& first, const Vec3d& second);
Vec3d Cross(const Vec3d& first, const Vec3d& second);
Vec3d Normalize(const Vec3d& vector);
// Great-circle angular distance in degrees.
double GCDistance(double latitudeA, double longitudeA, double latitudeB,
                  double longitudeB);
}  // namespace Infinitesia::GenerationDetail

#pragma once

#include <cmath>
#include <cstdint>

namespace Infinitesia::GeologyRandom {
// splitmix64 finalizer: a stable, platform-independent 64-bit mix.
constexpr std::uint64_t Mix(std::uint64_t value) noexcept {
  value += 0x9e3779b97f4a7c15ull;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
  return value ^ (value >> 31);
}

// An independent value per (seed, stage, entity, index): the geology never
// draws from a shared generator, so results do not depend on the order things
// are asked for (docs/03_GEOLOGY.md, section 4.1).
constexpr std::uint64_t Stream(std::uint64_t seed, std::uint64_t stage,
                               std::uint64_t entity,
                               std::uint64_t index) noexcept {
  return Mix(Mix(Mix(Mix(seed) ^ stage) ^ entity) ^ index);
}

// Uniform in [0, 1).
constexpr double Unit(std::uint64_t hash) noexcept {
  return static_cast<double>(hash >> 11) * (1.0 / 9007199254740992.0);
}

// Smooth value noise in [-1, 1] on the plane, one lattice cell per unit: the
// same (seed, x, y) always gives the same value, whatever else was asked
// before.
inline double ValueNoise2(std::uint64_t seed, double x, double y) noexcept {
  const double fx = std::floor(x);
  const double fy = std::floor(y);
  const auto ix = static_cast<std::int64_t>(fx);
  const auto iy = static_cast<std::int64_t>(fy);
  const double tx = x - fx;
  const double ty = y - fy;
  const double sx = tx * tx * (3.0 - 2.0 * tx);
  const double sy = ty * ty * (3.0 - 2.0 * ty);
  const auto corner = [seed](std::int64_t cx, std::int64_t cy) {
    return Unit(Stream(seed, 0x6e6f6973'65ull, static_cast<std::uint64_t>(cx),
                       static_cast<std::uint64_t>(cy)));
  };
  const double bottom =
      corner(ix, iy) + (corner(ix + 1, iy) - corner(ix, iy)) * sx;
  const double top =
      corner(ix, iy + 1) + (corner(ix + 1, iy + 1) - corner(ix, iy + 1)) * sx;
  return 2.0 * (bottom + (top - bottom) * sy) - 1.0;
}
}  // namespace Infinitesia::GeologyRandom

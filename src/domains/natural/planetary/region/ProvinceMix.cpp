#include "domains/natural/planetary/region/ProvinceMix.h"

#include <cmath>
#include <cstdint>

namespace Infinitesia
{
namespace
{
// Lenses of a few metres: coarse enough to read as rock bodies, fine enough to
// fill a 100-200 m contact zone with several of them.
constexpr double kMixLatticeMeters = 32.0;
constexpr std::uint32_t kMixSalt = 0x51ED2C3Bu;

std::uint32_t HashLattice(int x, int y, int z) noexcept
{
	std::uint32_t hash = kMixSalt ^ 0x9E3779B9u;
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

double LatticeValue(int x, int y, int z) noexcept
{
	return static_cast<double>(HashLattice(x, y, z)) / 2147483647.5 - 1.0;
}

double Fade(double t) noexcept
{
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double ValueNoise(double x, double y, double z) noexcept
{
	const double fx = std::floor(x);
	const double fy = std::floor(y);
	const double fz = std::floor(z);
	const int x0 = static_cast<int>(fx);
	const int y0 = static_cast<int>(fy);
	const int z0 = static_cast<int>(fz);
	const double tx = Fade(x - fx);
	const double ty = Fade(y - fy);
	const double tz = Fade(z - fz);
	const auto lerp = [](double a, double b, double t) { return a + (b - a) * t; };
	const auto corner = [&](int dx, int dy, int dz) { return LatticeValue(x0 + dx, y0 + dy, z0 + dz); };

	const double x00 = lerp(corner(0, 0, 0), corner(1, 0, 0), tx);
	const double x10 = lerp(corner(0, 1, 0), corner(1, 1, 0), tx);
	const double x01 = lerp(corner(0, 0, 1), corner(1, 0, 1), tx);
	const double x11 = lerp(corner(0, 1, 1), corner(1, 1, 1), tx);
	return lerp(lerp(x00, x10, ty), lerp(x01, x11, ty), tz);
}
} // namespace

bool UseSecondaryRock(double secondaryWeight, int eastMeters, int northMeters, int elevationMeters) noexcept
{
	if (secondaryWeight <= 0.0)
	{
		return false;
	}
	const double noise = ValueNoise(static_cast<double>(eastMeters) / kMixLatticeMeters,
	                                static_cast<double>(elevationMeters) / kMixLatticeMeters,
	                                static_cast<double>(northMeters) / kMixLatticeMeters);
	// Value noise is [-1, 1]; map it to a 0..1 share and compare with the
	// borrowed fraction. At the contact (0.5) this is a half-and-half mixture,
	// fading to pure primary rock at the edge of the zone.
	const double secondaryShare = noise * 0.5 + 0.5;
	return secondaryShare < secondaryWeight;
}
} // namespace Infinitesia

#include "domains/natural/planetary/internal/GenerationMath.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Infinitesia::GenerationDetail
{

// --------------------------------------------------------------------------
// Deterministic hashing / random numbers
// --------------------------------------------------------------------------

constexpr std::uint32_t kHashMul1 = 0x9e3779b9u;
constexpr std::uint32_t kHashMul2 = 0x85ebca6bu;

std::uint32_t Hash32(std::uint32_t v)
{
    v ^= v >> 16; v *= kHashMul2;
    v ^= v >> 13; v *= 0xc2b2ae3du;
    v ^= v >> 16;
    return v;
}

std::uint32_t HashCombine(std::uint32_t a, std::uint32_t b)
{
    return Hash32(a ^ (b * kHashMul1));
}

double RandUnit(std::uint32_t seed, std::uint32_t tag)
{
    const std::uint32_t h = Hash32(seed ^ (tag * kHashMul1));
    return static_cast<double>(h) / static_cast<double>(0xffffffffu);
}

double RandUnit(std::uint32_t seed, int lat, int lon)
{
    return RandUnit(seed,
        static_cast<std::uint32_t>(lat) * kHashMul1 ^
        static_cast<std::uint32_t>(lon) * kHashMul2);
}

double RandRange(std::uint32_t seed, std::uint32_t tag, double lo, double hi)
{
    return lo + (hi - lo) * RandUnit(seed, tag);
}

// --------------------------------------------------------------------------
// Interpolation
// --------------------------------------------------------------------------

double SmootherStep(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double Lerp(double a, double b, double t)
{
    return a + (b - a) * std::clamp(t, 0.0, 1.0);
}

// --------------------------------------------------------------------------
// 2-D value noise (longitude wraps, latitude clamps)
// --------------------------------------------------------------------------
double ValueNoise2D(double nLat, double nLon, int latCells, int lonCells, std::uint32_t seed)
{
    const double lp = std::clamp(nLat, 0.0, 1.0) * latCells;
    const double lo = (nLon - std::floor(nLon)) * lonCells;
    const int la0 = std::min(static_cast<int>(lp), latCells);
    const int la1 = std::min(la0 + 1, latCells);
    const int lo0 = static_cast<int>(lo) % lonCells;
    const int lo1 = (lo0 + 1) % lonCells;
    const double bt = SmootherStep(lp - la0);
    const double bl = SmootherStep(lo - std::floor(lo));

    const auto s = [seed, lonCells](int la, int lo_) -> double {
        const std::uint32_t v = seed ^
            (static_cast<std::uint32_t>(la + 1) * kHashMul1) ^
            (static_cast<std::uint32_t>(lo_ + 1) * kHashMul2);
        return static_cast<double>(Hash32(v)) / static_cast<double>(0xffffffffu);
    };

    const double n  = s(la0, lo0) + (s(la0, lo1) - s(la0, lo0)) * bl;
    const double ss = s(la1, lo0) + (s(la1, lo1) - s(la1, lo0)) * bl;
    return n + (ss - n) * bt;
}

double fBm(double nLat, double nLon, std::uint32_t seed, int baseLat, int baseLon, int octaves = 6)
{
    double v = 0.0, amp = 1.0, total = 0.0;
    std::uint32_t s = seed;
    int lc = baseLat, lnc = baseLon;
    for (int o = 0; o < octaves; ++o)
    {
        v += ValueNoise2D(nLat, nLon, lc, lnc, s) * amp;
        total += amp;
        amp *= 0.5;
        lc *= 2; lnc *= 2;
        s = Hash32(s ^ 0xdeadbeef);
    }
    return v / total;
}

double fBmGrid(int la, int lo, int latB, int lonS, std::uint32_t seed,
               int baseLat = 6, int baseLon = 12, int octaves = 6)
{
    const double nLat = (la + 0.5) / latB;
    const double nLon = (lo + 0.5) / lonS;
    return fBm(nLat, nLon, seed, baseLat, baseLon, octaves);
}

// --------------------------------------------------------------------------
// Spherical geometry
// --------------------------------------------------------------------------
Vec3d LatLonToCart(double latDeg, double lonDeg)
{
    const double lat = latDeg * std::numbers::pi / 180.0;
    const double lon = lonDeg * std::numbers::pi / 180.0;
    return { std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon) };
}

double SphericalFbm(
    double latitudeDegrees,
    double longitudeDegrees,
    std::uint32_t seed,
    int baseCells,
    int octaves)
{
    const Vec3d point = LatLonToCart(latitudeDegrees, longitudeDegrees);
    double value = 0.0;
    double amplitude = 1.0;
    double amplitudeSum = 0.0;
    int cells = baseCells;

    for (int octave = 0; octave < octaves; ++octave)
    {
        const double x = (point.x + 1.0) * 0.5 * cells;
        const double y = (point.y + 1.0) * 0.5 * cells;
        const double z = (point.z + 1.0) * 0.5 * cells;
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const int z0 = static_cast<int>(std::floor(z));
        const double tx = SmootherStep(x - x0);
        const double ty = SmootherStep(y - y0);
        const double tz = SmootherStep(z - z0);

        const auto sample = [seed](int ix, int iy, int iz)
        {
            std::uint32_t hash = seed;
            hash ^= static_cast<std::uint32_t>(ix + 1) * kHashMul1;
            hash ^= static_cast<std::uint32_t>(iy + 1) * kHashMul2;
            hash ^= Hash32(static_cast<std::uint32_t>(iz + 1) * 0xc2b2ae35u);
            return static_cast<double>(Hash32(hash)) /
                   static_cast<double>(0xffffffffu);
        };
        const auto interpolate = [&sample, tx, ty, tz, x0, y0, z0]()
        {
            const double x00 = Lerp(sample(x0, y0, z0), sample(x0 + 1, y0, z0), tx);
            const double x10 = Lerp(sample(x0, y0 + 1, z0), sample(x0 + 1, y0 + 1, z0), tx);
            const double x01 = Lerp(sample(x0, y0, z0 + 1), sample(x0 + 1, y0, z0 + 1), tx);
            const double x11 = Lerp(sample(x0, y0 + 1, z0 + 1), sample(x0 + 1, y0 + 1, z0 + 1), tx);
            return Lerp(Lerp(x00, x10, ty), Lerp(x01, x11, ty), tz);
        };

        value += interpolate() * amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5;
        cells *= 2;
        seed = Hash32(seed ^ 0xdeadbeefu);
    }

    return amplitudeSum > 0.0 ? value / amplitudeSum : 0.0;
}

double Dot(const Vec3d& a, const Vec3d& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

Vec3d Cross(const Vec3d& a, const Vec3d& b)
{
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}

Vec3d Normalize(const Vec3d& v)
{
    const double len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len < 1e-12) return { 0.0, 0.0, 0.0 };
    return { v.x/len, v.y/len, v.z/len };
}

// Great-circle angular distance in degrees
double GCDistance(double la1, double lo1, double la2, double lo2)
{
    const Vec3d a = LatLonToCart(la1, lo1);
    const Vec3d b = LatLonToCart(la2, lo2);
    const double dot = std::clamp(Dot(a, b), -1.0, 1.0);
    return std::acos(dot) * 180.0 / std::numbers::pi;
}

}

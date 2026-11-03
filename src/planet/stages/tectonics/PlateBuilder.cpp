#include "planet/stages/tectonics/TectonicsInternal.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Infinitesia::TectonicsDetail
{
    using namespace GenerationDetail;

    std::vector<TectonicPlate> BuildPlates(
        std::uint32_t seed, int numPlates, double heatScale, const CrustField& crust)
    {
        const double continentalThreshold = crust.threshold;

        // -----------------------------------------------------------------------
        // Place plate centres on a Fibonacci spiral (uniform sphere distribution)
        // -----------------------------------------------------------------------
        std::vector<TectonicPlate> plates(static_cast<std::size_t>(numPlates));
        const double goldenAngle = std::numbers::pi * (3.0 - std::sqrt(5.0));

        for (int p = 0; p < numPlates; ++p)
        {
            const auto pu = static_cast<std::uint32_t>(p);
            const double y     = 1.0 - (p / (numPlates - 1.0)) * 2.0;
            const double theta = goldenAngle * p + RandRange(seed, pu*13u+1u, -0.25, 0.25);
            plates[p].id        = p;
            plates[p].centerLat = std::asin(std::clamp(y, -1.0, 1.0)) * 180.0 / std::numbers::pi;
            plates[p].centerLon = std::fmod(theta * 180.0 / std::numbers::pi + 360.0, 360.0);

            plates[p].isOceanic =
                SphericalFbm(plates[p].centerLat, plates[p].centerLon,
                             seed ^ 0x71a1d001u, 3, 5) <= continentalThreshold;

            if (plates[p].isOceanic)
            {
                plates[p].crustDensity     = 2870.0 + RandRange(seed, pu*7u+3u, -40.0,  80.0);
                plates[p].crustThicknessKm = RandRange(seed, pu*7u+4u,   5.0,  10.0);
            }
            else
            {
                plates[p].crustDensity     = 2720.0 + RandRange(seed, pu*7u+3u, -40.0,  60.0);
                plates[p].crustThicknessKm = RandRange(seed, pu*7u+4u,  28.0,  50.0);
            }

            // -----------------------------------------------------------------------
            // Euler pole: random rotation axis + angular velocity.
            //
            // Reference angular rates of 0.09–0.9 deg/Myr correspond to
            // approximately 1–10 cm/yr at Earth's radius before heat scaling.
            // In Euler-pole terms: ω = v_equatorial / R_equatorial.
            // For a unit sphere, R=1 rad → ω_rad/Myr ≈ v_deg/Myr × π/180.
            //
            // Apply the mass/age heatScale, then convert deg/Myr to rad/Myr.
            // -----------------------------------------------------------------------
            const double speedDegPerMy = RandRange(seed, pu*7u+5u, 0.09, 0.9) * heatScale;

            // Random Euler pole axis (uniform on sphere via rejection-free method)
            const double axisLat = std::asin(RandRange(seed, pu*7u+6u, -1.0, 1.0)) * 180.0 / std::numbers::pi;
            const double axisLon = RandRange(seed, pu*7u+7u, 0.0, 360.0);
            plates[p].kinematics.euler.axis = LatLonToCart(axisLat, axisLon);
            // Convert deg/My to rad/My
            plates[p].kinematics.euler.omegaRadPerMy = speedDegPerMy * std::numbers::pi / 180.0;
        }

        return plates;
    }

    std::vector<CellAssignment> AssignCellsToPlates(
        const PlanetBuild& build, const std::vector<TectonicPlate>& plates)
    {
        const int latB = build.LatBands();
        const int lonS = build.LonSegs();
        const int numPlates = static_cast<int>(plates.size());

        // Voronoi assignment: each cell -> nearest plate (great-circle distance).
        // Records the second-nearest for boundary detection.
        std::vector<CellAssignment> assignments(static_cast<std::size_t>(latB) * lonS);

        // Great-circle distance is acos(dot) of the unit vectors, which is
        // monotonic, so the two nearest plates are the two with the largest dot.
        // Build the plate vectors once and the cell vector once per cell, and
        // evaluate acos (as GCDistance does) only for the two winners.
        std::vector<Vec3d> plateVectors(static_cast<std::size_t>(numPlates));
        for (int p = 0; p < numPlates; ++p)
            plateVectors[static_cast<std::size_t>(p)] =
                LatLonToCart(plates[p].centerLat, plates[p].centerLon);
        const auto dotToDistanceDeg = [](double dot)
        {
            return std::acos(std::clamp(dot, -1.0, 1.0)) * 180.0 / std::numbers::pi;
        };

        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                const auto& c = build.Cell(la, lo);
                auto& a = assignments[static_cast<std::size_t>(la)*lonS + lo];
                const Vec3d cellVector = LatLonToCart(c.latitudeDegrees, c.longitudeDegrees);
                double bestDot = -2.0, secondDot = -2.0;
                for (int p = 0; p < numPlates; ++p)
                {
                    const Vec3d& pv = plateVectors[static_cast<std::size_t>(p)];
                    const double dot = std::clamp(
                        cellVector.x * pv.x + cellVector.y * pv.y + cellVector.z * pv.z,
                        -1.0, 1.0);
                    if (dot > bestDot)
                    {
                        secondDot = bestDot; a.second = a.best;
                        bestDot   = dot;     a.best   = p;
                    }
                    else if (dot > secondDot)
                    {
                        secondDot = dot; a.second = p;
                    }
                }
                a.bestDist = dotToDistanceDeg(bestDot);
                a.secondDist = secondDot > -2.0 ? dotToDistanceDeg(secondDot) : 1e18;
            }
        }

        return assignments;
    }
}

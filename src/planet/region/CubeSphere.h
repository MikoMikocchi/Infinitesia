#pragma once

#include "coordinates/PlanetaryCoordinates.h"

namespace Infinitesia
{
    // Point on one of the six cube faces. u and v span [0, 1] across the face
    // using an equal-angle mapping, so cells stay close to square on the sphere.
    // Face order: +X, -X, +Y, -Y, +Z, -Z. Axes follow PlanetaryCoordinates
    // (y is the polar axis).
    struct CubeFacePoint
    {
        int face = 0;
        double u = 0.0;
        double v = 0.0;
    };

    class CubeSphere
    {
    public:
        static constexpr int kFaceCount = 6;

        // Unit direction for a face point.
        static CartesianPoint3D ToDirection(CubeFacePoint point) noexcept;

        // Face point for any non-zero direction (does not need to be normalized).
        static CubeFacePoint FromDirection(CartesianPoint3D direction) noexcept;
    };
}

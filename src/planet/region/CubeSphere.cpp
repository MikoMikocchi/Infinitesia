#include "planet/region/CubeSphere.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace Infinitesia
{
    namespace
    {
        struct Vec3
        {
            double x, y, z;
        };

        constexpr Vec3 Cross(Vec3 a, Vec3 b) noexcept
        {
            return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
        }

        constexpr double Dot(Vec3 a, Vec3 b) noexcept
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        struct FaceBasis
        {
            Vec3 normal;
            Vec3 right;
            Vec3 up;
        };

        // right = up x normal, so the face reads left-to-right when viewed from outside.
        constexpr FaceBasis MakeBasis(Vec3 normal, Vec3 up) noexcept
        {
            return { normal, Cross(up, normal), up };
        }

        constexpr std::array<FaceBasis, CubeSphere::kFaceCount> kFaces{
            MakeBasis({ 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }),
            MakeBasis({ -1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }),
            MakeBasis({ 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 }),
            MakeBasis({ 0.0, -1.0, 0.0 }, { 0.0, 0.0, -1.0 }),
            MakeBasis({ 0.0, 0.0, 1.0 }, { 0.0, 1.0, 0.0 }),
            MakeBasis({ 0.0, 0.0, -1.0 }, { 0.0, 1.0, 0.0 }),
        };

        constexpr double kQuarterPi = std::numbers::pi / 4.0;

        double FaceCoordinateToPlane(double coordinate) noexcept
        {
            return std::tan((coordinate - 0.5) * 2.0 * kQuarterPi);
        }

        double PlaneToFaceCoordinate(double plane) noexcept
        {
            return std::atan(plane) / (2.0 * kQuarterPi) + 0.5;
        }
    }

    CartesianPoint3D CubeSphere::ToDirection(CubeFacePoint point) noexcept
    {
        const FaceBasis& basis = kFaces[static_cast<std::size_t>(std::clamp(point.face, 0, kFaceCount - 1))];
        const double a = FaceCoordinateToPlane(point.u);
        const double b = FaceCoordinateToPlane(point.v);
        const double x = basis.normal.x + a * basis.right.x + b * basis.up.x;
        const double y = basis.normal.y + a * basis.right.y + b * basis.up.y;
        const double z = basis.normal.z + a * basis.right.z + b * basis.up.z;
        const double length = std::sqrt(x * x + y * y + z * z);
        return { x / length, y / length, z / length };
    }

    CubeFacePoint CubeSphere::FromDirection(CartesianPoint3D direction) noexcept
    {
        const Vec3 d{ direction.x, direction.y, direction.z };
        const double ax = std::abs(d.x);
        const double ay = std::abs(d.y);
        const double az = std::abs(d.z);

        int face;
        if (ax >= ay && ax >= az)
        {
            face = d.x >= 0.0 ? 0 : 1;
        }
        else if (ay >= az)
        {
            face = d.y >= 0.0 ? 2 : 3;
        }
        else
        {
            face = d.z >= 0.0 ? 4 : 5;
        }

        const FaceBasis& basis = kFaces[static_cast<std::size_t>(face)];
        const double depth = Dot(d, basis.normal);
        return {
            face,
            std::clamp(PlaneToFaceCoordinate(Dot(d, basis.right) / depth), 0.0, 1.0),
            std::clamp(PlaneToFaceCoordinate(Dot(d, basis.up) / depth), 0.0, 1.0)
        };
    }
}

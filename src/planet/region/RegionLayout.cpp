#include "planet/region/RegionLayout.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Infinitesia
{
    RegionLayout::RegionLayout(double planetRadiusMeters)
        : m_radiusMeters(planetRadiusMeters)
    {
        const double faceEdgeMeters = planetRadiusMeters * std::numbers::pi / 2.0;
        m_regionsPerEdge = std::max(1, static_cast<int>(std::lround(faceEdgeMeters / kTargetRegionSizeMeters)));
        m_regionSizeMeters = faceEdgeMeters / m_regionsPerEdge;
    }

    bool RegionLayout::IsValid(RegionId id) const noexcept
    {
        return id.face >= 0 && id.face < CubeSphere::kFaceCount &&
               id.i >= 0 && id.i < m_regionsPerEdge &&
               id.j >= 0 && id.j < m_regionsPerEdge;
    }

    RegionId RegionLayout::RegionAt(CartesianPoint3D direction) const noexcept
    {
        const CubeFacePoint point = CubeSphere::FromDirection(direction);
        const auto index = [this](double coordinate)
        {
            return std::clamp(static_cast<int>(coordinate * m_regionsPerEdge), 0, m_regionsPerEdge - 1);
        };
        return { point.face, index(point.u), index(point.v) };
    }

    CartesianPoint3D RegionLayout::PointDirection(RegionId id, double x, double y) const noexcept
    {
        const double latticePointsPerEdge = static_cast<double>(m_regionsPerEdge) * kCellsPerRegionEdge;
        return CubeSphere::ToDirection({
            id.face,
            (static_cast<double>(id.i) * kCellsPerRegionEdge + x) / latticePointsPerEdge,
            (static_cast<double>(id.j) * kCellsPerRegionEdge + y) / latticePointsPerEdge
        });
    }

    CartesianPoint3D RegionLayout::CenterDirection(RegionId id) const noexcept
    {
        return CubeSphere::ToDirection({
            id.face,
            (id.i + 0.5) / m_regionsPerEdge,
            (id.j + 0.5) / m_regionsPerEdge
        });
    }

    GeographicCoordinates RegionLayout::Center(RegionId id) const noexcept
    {
        return PlanetaryCoordinates::ToGeographic(CenterDirection(id));
    }

    RegionId RegionLayout::Neighbor(RegionId id, int dx, int dy) const noexcept
    {
        const int i = id.i + dx;
        const int j = id.j + dy;
        if (i >= 0 && i < m_regionsPerEdge && j >= 0 && j < m_regionsPerEdge)
        {
            return { id.face, i, j };
        }

        // Step half a region past the edge from the centre and look the point up
        // again, which handles crossing onto the adjacent face (and corners).
        const double steppedU = (id.i + 0.5 + dx) / m_regionsPerEdge;
        const double steppedV = (id.j + 0.5 + dy) / m_regionsPerEdge;

        // Past the edge the equal-angle mapping extends smoothly in the face plane.
        const double a = std::tan((steppedU - 0.5) * std::numbers::pi / 2.0);
        const double b = std::tan((steppedV - 0.5) * std::numbers::pi / 2.0);
        const CartesianPoint3D normal = CubeSphere::ToDirection({ id.face, 0.5, 0.5 });
        const CartesianPoint3D rightPoint = CubeSphere::ToDirection({ id.face, 1.0, 0.5 });
        const CartesianPoint3D upPoint = CubeSphere::ToDirection({ id.face, 0.5, 1.0 });
        // rightPoint = (normal + right) / sqrt(2), so right = rightPoint * sqrt(2) - normal.
        const double s = std::numbers::sqrt2;
        const CartesianPoint3D right{ rightPoint.x * s - normal.x, rightPoint.y * s - normal.y, rightPoint.z * s - normal.z };
        const CartesianPoint3D up{ upPoint.x * s - normal.x, upPoint.y * s - normal.y, upPoint.z * s - normal.z };
        return RegionAt({
            normal.x + a * right.x + b * up.x,
            normal.y + a * right.y + b * up.y,
            normal.z + a * right.z + b * up.z
        });
    }
}

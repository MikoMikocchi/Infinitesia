#include "domains/natural/biology/flora/TreeVoxelizer.h"

#include "domains/natural/biology/flora/FloraRandom.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Infinitesia
{
    namespace
    {
        constexpr std::uint64_t kLeafStage = 0x6c656166ull;
        constexpr int kCoordinateBias = 1024;

        std::uint64_t Pack(int x, int y, int z) noexcept
        {
            return static_cast<std::uint64_t>(x + kCoordinateBias) |
                   static_cast<std::uint64_t>(y + kCoordinateBias) << 16 |
                   static_cast<std::uint64_t>(z + kCoordinateBias) << 32;
        }

        class Grid
        {
        public:
            void Put(int x, int y, int z, TreePart part)
            {
                if (part == TreePart::Root ? y >= 0 : y < 0)
                    return;
                const auto [entry, inserted] = m_parts.emplace(Pack(x, y, z), part);
                if (!inserted && PartStrength(part) > PartStrength(entry->second))
                    entry->second = part;
            }

            // A ball of the radius around `point`: the nearest cell always, its neighbours from radius 1 up.
            void Stamp(Vec3 point, double radius, TreePart part)
            {
                const int cx = static_cast<int>(std::lround(point.x));
                const int cy = static_cast<int>(std::lround(point.y));
                const int cz = static_cast<int>(std::lround(point.z));
                Put(cx, cy, cz, part);
                if (radius < 1.0)
                    return;
                const int reach = static_cast<int>(std::ceil(radius));
                for (int dz = -reach; dz <= reach; ++dz)
                {
                    for (int dy = -reach; dy <= reach; ++dy)
                    {
                        for (int dx = -reach; dx <= reach; ++dx)
                        {
                            const double ox = cx + dx - point.x;
                            const double oy = cy + dy - point.y;
                            const double oz = cz + dz - point.z;
                            if (ox * ox + oy * oy + oz * oz <= radius * radius)
                                Put(cx + dx, cy + dy, cz + dz, part);
                        }
                    }
                }
            }

            void Line(const SkeletonSegment& segment)
            {
                const Vec3 delta{ segment.to.x - segment.from.x, segment.to.y - segment.from.y,
                                  segment.to.z - segment.from.z };
                const double length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                const int steps = std::max(1, static_cast<int>(std::ceil(length / 0.5)));
                for (int i = 0; i <= steps; ++i)
                {
                    const double t = static_cast<double>(i) / steps;
                    Stamp(segment.from + delta * t, segment.radiusFrom + (segment.radiusTo - segment.radiusFrom) * t,
                          segment.part);
                }
            }

            void Foliage(const LeafCluster& cluster, std::uint64_t seed)
            {
                const int cx = static_cast<int>(std::lround(cluster.center.x));
                const int cy = static_cast<int>(std::lround(cluster.center.y));
                const int cz = static_cast<int>(std::lround(cluster.center.z));
                Put(cx, cy, cz, TreePart::Leaves);
                const int reachH = static_cast<int>(std::ceil(cluster.radiusHorizontal));
                const int reachV = static_cast<int>(std::ceil(cluster.radiusVertical));
                for (int dz = -reachH; dz <= reachH; ++dz)
                {
                    for (int dy = -reachV; dy <= reachV; ++dy)
                    {
                        for (int dx = -reachH; dx <= reachH; ++dx)
                        {
                            const double ox = (cx + dx - cluster.center.x) / cluster.radiusHorizontal;
                            const double oy = (cy + dy - cluster.center.y) / cluster.radiusVertical;
                            const double oz = (cz + dz - cluster.center.z) / cluster.radiusHorizontal;
                            const double distance = std::sqrt(ox * ox + oy * oy + oz * oz);
                            // The core (up to 0.7 of the radius) is solid, the rim thins out by chance.
                            const double rim = FloraRandom::Unit(FloraRandom::Stream(
                                seed, kLeafStage, Pack(cx + dx, cy + dy, cz + dz), 0));
                            if (distance <= 1.0 - 0.3 * rim)
                                Put(cx + dx, cy + dy, cz + dz, TreePart::Leaves);
                        }
                    }
                }
            }

            TreeVoxels Finish() const
            {
                TreeVoxels result;
                result.cells.reserve(m_parts.size());
                for (const auto& [key, part] : m_parts)
                {
                    result.cells.push_back({ static_cast<std::int16_t>(static_cast<int>(key & 0xffff) - kCoordinateBias),
                                             static_cast<std::int16_t>(static_cast<int>(key >> 16 & 0xffff) - kCoordinateBias),
                                             static_cast<std::int16_t>(static_cast<int>(key >> 32 & 0xffff) - kCoordinateBias),
                                             part });
                }
                std::sort(result.cells.begin(), result.cells.end(), [](const TreeVoxel& a, const TreeVoxel& b)
                {
                    if (a.y != b.y) return a.y < b.y;
                    if (a.z != b.z) return a.z < b.z;
                    return a.x < b.x;
                });
                if (!result.cells.empty())
                {
                    result.minX = result.maxX = result.cells.front().x;
                    result.minY = result.maxY = result.cells.front().y;
                    result.minZ = result.maxZ = result.cells.front().z;
                    for (const TreeVoxel& cell : result.cells)
                    {
                        result.minX = std::min<int>(result.minX, cell.x);
                        result.maxX = std::max<int>(result.maxX, cell.x);
                        result.minY = std::min<int>(result.minY, cell.y);
                        result.maxY = std::max<int>(result.maxY, cell.y);
                        result.minZ = std::min<int>(result.minZ, cell.z);
                        result.maxZ = std::max<int>(result.maxZ, cell.z);
                    }
                }
                return result;
            }

        private:
            std::unordered_map<std::uint64_t, TreePart> m_parts;
        };
    }

    TreeVoxels Voxelize(const TreeSkeleton& skeleton, std::uint64_t seed)
    {
        Grid grid;
        for (const SkeletonSegment& segment : skeleton.segments)
            grid.Line(segment);
        for (const LeafCluster& cluster : skeleton.clusters)
            grid.Foliage(cluster, seed);
        return grid.Finish();
    }
}

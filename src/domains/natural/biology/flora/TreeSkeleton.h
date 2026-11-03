#pragma once

#include "domains/natural/biology/catalog/Species.h"

#include <cstdint>
#include <vector>

namespace Infinitesia
{
    // What a cell of a plant is (docs/07_BIOLOGY.md, section 6).
    enum class TreePart : std::uint8_t { Trunk, Branch, Twig, Leaves, Root };

    // Where two parts want one cell, the stronger keeps it: Trunk > Branch > Root > Twig > Leaves.
    constexpr int PartStrength(TreePart part) noexcept
    {
        switch (part)
        {
        case TreePart::Trunk:  return 5;
        case TreePart::Branch: return 4;
        case TreePart::Root:   return 3;
        case TreePart::Twig:   return 2;
        case TreePart::Leaves: return 1;
        }
        return 0;
    }

    struct Vec3
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    inline Vec3 operator+(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    inline Vec3 operator*(Vec3 a, double k) { return { a.x * k, a.y * k, a.z * k }; }

    // A thick line of one part. Coordinates are in cells relative to the plant: (0, 0, 0) is the centre
    // of the first cell of the trunk, which stands on the ground; roots lie below (y < 0).
    struct SkeletonSegment
    {
        Vec3 from;
        Vec3 to;
        double radiusFrom = 0.5;
        double radiusTo = 0.5;
        TreePart part = TreePart::Trunk;
    };

    // A bunch of foliage: an ellipsoid with a ragged edge.
    struct LeafCluster
    {
        Vec3 center;
        double radiusHorizontal = 1.0;
        double radiusVertical = 1.0;
    };

    struct TreeSkeleton
    {
        std::vector<SkeletonSegment> segments;
        std::vector<LeafCluster> clusters;
    };

    // Grows the skeleton of one plant of the species: a pure function of both arguments.
    TreeSkeleton GrowSkeleton(const Species& species, std::uint64_t seed);
}

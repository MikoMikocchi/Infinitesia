#include "domains/natural/geology/model/GeologyModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Infinitesia
{
    namespace
    {
        using Profile = GeologyModel::ColumnProfile;

        constexpr int kBasementBottom = std::numeric_limits<int>::min();

        // Rocks above `cut` from `upper` and rocks below it from `lower`.
        Profile Compose(const Profile& upper, const Profile& lower, int cut)
        {
            struct Piece
            {
                MaterialId rock;
                int bottom;
                std::uint8_t formation;
            };
            std::vector<Piece> layers;
            for (int i = 0; i < upper.count; ++i)
            {
                const int bottom = upper.bottom[static_cast<std::size_t>(i)];
                const int top = i == 0 ? std::numeric_limits<int>::max() : upper.bottom[static_cast<std::size_t>(i - 1)];
                if (top <= cut)
                {
                    break;
                }
                layers.push_back({ upper.rock[static_cast<std::size_t>(i)], std::max(bottom, cut), upper.formation[static_cast<std::size_t>(i)] });
                if (bottom <= cut)
                {
                    break;
                }
            }
            for (int j = 0; j < lower.count; ++j)
            {
                const int bottom = lower.bottom[static_cast<std::size_t>(j)];
                if (bottom < cut)
                {
                    layers.push_back({ lower.rock[static_cast<std::size_t>(j)], bottom, lower.formation[static_cast<std::size_t>(j)] });
                }
            }

            // Neighbouring pieces of one rock are one layer.
            for (std::size_t i = 0; i + 1 < layers.size();)
            {
                if (layers[i].rock == layers[i + 1].rock)
                    layers.erase(layers.begin() + static_cast<std::ptrdiff_t>(i));
                else
                    ++i;
            }
            // Too many layers for a profile: drop the thinnest interior ones (never the top or basement).
            while (layers.size() > static_cast<std::size_t>(GeologyModel::kMaxProfileLayers))
            {
                std::size_t thinnest = 1;
                long long smallest = std::numeric_limits<long long>::max();
                for (std::size_t i = 1; i + 1 < layers.size(); ++i)
                {
                    const long long thickness = static_cast<long long>(layers[i - 1].bottom) - layers[i].bottom;
                    if (thickness < smallest)
                    {
                        smallest = thickness;
                        thinnest = i;
                    }
                }
                layers.erase(layers.begin() + static_cast<std::ptrdiff_t>(thinnest));
            }

            Profile result;
            for (const Piece& piece : layers)
            {
                result.rock[static_cast<std::size_t>(result.count)] = piece.rock;
                result.bottom[static_cast<std::size_t>(result.count)] = piece.bottom;
                result.formation[static_cast<std::size_t>(result.count)] = piece.formation;
                ++result.count;
            }
            result.bottom[static_cast<std::size_t>(result.count - 1)] = kBasementBottom;
            return result;
        }
    }

    GeologyModel::ColumnProfile GeologyModel::FaultedProfile(
        ProvinceId province, const std::vector<Fault>& faults, std::size_t count,
        double eastMeters, double northMeters, int pileTopMeters) const
    {
        if (count == 0)
        {
            return BaseProfileAt(province, eastMeters, northMeters, pileTopMeters);
        }
        // The youngest fault of the first `count` cuts everything the older ones made.
        const Fault& fault = faults[count - 1];
        const FaultEffect effect = EffectOf(fault, eastMeters, northMeters, pileTopMeters);
        if (!(effect.amplitude > 0.0))
        {
            return FaultedProfile(province, faults, count - 1, eastMeters, northMeters, pileTopMeters);
        }

        const ColumnProfile lower = FaultedProfile(province, faults, count - 1, eastMeters, northMeters, pileTopMeters);
        ColumnProfile upper = FaultedProfile(
            province, faults, count - 1, eastMeters - effect.shiftEast, northMeters - effect.shiftNorth, pileTopMeters);
        const int lift = static_cast<int>(std::lround(effect.lift));
        for (int i = 0; i + 1 < upper.count; ++i)
        {
            upper.bottom[static_cast<std::size_t>(i)] += lift;
        }
        return Compose(upper, lower, static_cast<int>(std::lround(effect.cutElevation)));
    }

    GeologyModel::ColumnProfile GeologyModel::ProfileAt(
        ProvinceId province, double eastMeters, double northMeters, int pileTopMeters) const
    {
        const std::vector<Fault> faults = FaultsNear(province, eastMeters, northMeters);
        return FaultedProfile(province, faults, faults.size(), eastMeters, northMeters, pileTopMeters);
    }

    std::size_t GeologyModel::FormationAt(const ColumnProfile& profile, int elevationMeters) noexcept
    {
        for (int i = 0; i < profile.count; ++i)
        {
            if (elevationMeters >= profile.bottom[static_cast<std::size_t>(i)])
            {
                return profile.formation[static_cast<std::size_t>(i)];
            }
        }
        return profile.formation[static_cast<std::size_t>(profile.count - 1)];
    }

    MaterialId GeologyModel::RockAt(const ColumnProfile& profile, int elevationMeters) noexcept
    {
        for (int i = 0; i < profile.count; ++i)
        {
            if (elevationMeters >= profile.bottom[static_cast<std::size_t>(i)])
            {
                return profile.rock[static_cast<std::size_t>(i)];
            }
        }
        return profile.rock[static_cast<std::size_t>(profile.count - 1)];
    }

    MaterialId GeologyModel::BedrockAt(ProvinceId province, double eastMeters, double northMeters, int elevationMeters,
                                       int pileTopMeters) const
    {
        return RockAt(ProfileAt(province, eastMeters, northMeters, pileTopMeters), elevationMeters);
    }
}

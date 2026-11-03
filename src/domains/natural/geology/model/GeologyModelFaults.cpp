#include "domains/natural/geology/model/GeologyModel.h"
#include "domains/natural/geology/model/GeologyModelDetail.h"

#include "domains/natural/geology/util/StableRandom.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia
{
    namespace
    {
        using GeologyModelDetail::kDegrees;
        using GeologyModelDetail::Lerp;

        constexpr std::uint64_t kFaultStage = 0x6661'756c'0004ull;     // "faul"

        // Faults of lattice cells up to this many cells away can reach a column.
        constexpr int kFaultCellReach = 2;
        constexpr double kFaultMinDip[3] = { 50.0, 20.0, 80.0 };  // normal, thrust, strike-slip
        constexpr double kFaultMaxDip[3] = { 70.0, 40.0, 88.0 };
    }

    std::vector<GeologyModel::Fault> GeologyModel::FaultsNear(
        ProvinceId province, double eastMeters, double northMeters) const
    {
        const Structure& structure = StructureOf(province);
        const FaultStyle& style = structure.faults;
        std::vector<Fault> faults;
        if (!(style.probability > 0.0) || province == kNoProvince)
        {
            return faults;
        }

        const double spacing = style.spacingMeters;
        const auto cellEast = static_cast<std::int64_t>(std::floor(eastMeters / spacing));
        const auto cellNorth = static_cast<std::int64_t>(std::floor(northMeters / spacing));
        for (int di = -kFaultCellReach; di <= kFaultCellReach; ++di)
        {
            for (int dj = -kFaultCellReach; dj <= kFaultCellReach; ++dj)
            {
                const std::int64_t ci = cellEast + di;
                const std::int64_t cj = cellNorth + dj;
                const std::uint64_t key = GeologyRandom::Stream(
                    structure.seed, kFaultStage, static_cast<std::uint64_t>(ci), static_cast<std::uint64_t>(cj));
                const auto draw = [key](std::uint64_t index)
                {
                    return GeologyRandom::Unit(GeologyRandom::Mix(key + index * 0x9e3779b97f4a7c15ull));
                };
                if (draw(0) >= style.probability)
                {
                    continue;
                }

                Fault fault;
                fault.centerEast = (static_cast<double>(ci) + 0.15 + 0.7 * draw(1)) * spacing;
                fault.centerNorth = (static_cast<double>(cj) + 0.15 + 0.7 * draw(2)) * spacing;
                const double kindDraw = draw(3);
                fault.kind = kindDraw < style.normalShare ? FaultKind::Normal
                    : kindDraw < style.normalShare + style.thrustShare ? FaultKind::Thrust
                    : FaultKind::StrikeSlip;

                // Normal faults and thrusts strike across the axis of plate motion (which stretches
                // or compresses the crust along it); strike-slip faults lie at an angle to it.
                const double axis = structure.compressionAzimuthDegrees;
                const double jitter = 30.0 * (draw(4) - 0.5);
                const double side = draw(5) < 0.5 ? 1.0 : -1.0;
                fault.strikeAzimuthDegrees = fault.kind == FaultKind::StrikeSlip
                    ? axis + side * (25.0 + 20.0 * draw(6))
                    : axis + 90.0 + jitter;
                const auto kindIndex = static_cast<std::size_t>(fault.kind);
                fault.dipDegrees = Lerp(kFaultMinDip[kindIndex], kFaultMaxDip[kindIndex], draw(7));
                fault.dipAzimuthDegrees = fault.strikeAzimuthDegrees + (draw(8) < 0.5 ? 90.0 : -90.0);
                fault.strikeRadiusMeters = spacing * (0.7 + 0.6 * draw(9));
                fault.dipRadiusMeters = spacing * (0.5 + 0.5 * draw(10));
                fault.slipMeters = Lerp(style.minSlipMeters, style.maxSlipMeters, draw(11));
                fault.sense = draw(12) < 0.5 ? 1.0 : -1.0;
                fault.age = draw(13);
                fault.centerOffsetMeters = -(10.0 + 90.0 * draw(14));

                // Close enough to matter: the fault itself, or where a displaced block came from.
                const double reach = std::max(fault.strikeRadiusMeters, fault.dipRadiusMeters) + fault.slipMeters;
                if (std::hypot(fault.centerEast - eastMeters, fault.centerNorth - northMeters) <= reach)
                {
                    faults.push_back(fault);
                }
            }
        }

        if (faults.size() > static_cast<std::size_t>(kMaxFaultsPerColumn))
        {
            std::sort(faults.begin(), faults.end(), [](const Fault& a, const Fault& b) { return a.slipMeters > b.slipMeters; });
            faults.resize(static_cast<std::size_t>(kMaxFaultsPerColumn));
        }
        std::sort(faults.begin(), faults.end(), [](const Fault& a, const Fault& b)
        {
            return a.age != b.age ? a.age < b.age : a.centerEast < b.centerEast;
        });
        return faults;
    }

    GeologyModel::FaultEffect GeologyModel::EffectOf(
        const Fault& fault, double eastMeters, double northMeters, int pileTopMeters) noexcept
    {
        const double beta = fault.dipDegrees * kDegrees;
        const double strike = fault.strikeAzimuthDegrees * kDegrees;
        const double dipAzimuth = fault.dipAzimuthDegrees * kDegrees;
        const double strikeEast = std::sin(strike);
        const double strikeNorth = std::cos(strike);
        const double dipEast = std::sin(dipAzimuth);
        const double dipNorth = std::cos(dipAzimuth);

        const double dx = eastMeters - fault.centerEast;
        const double dy = northMeters - fault.centerNorth;
        const double along = dx * strikeEast + dy * strikeNorth;
        const double downDip = dx * dipEast + dy * dipNorth; // horizontal distance down the dip
        const double slant = downDip / std::cos(beta);       // the same, measured along the plane
        const double ellipse = (along / fault.strikeRadiusMeters) * (along / fault.strikeRadiusMeters) +
                               (slant / fault.dipRadiusMeters) * (slant / fault.dipRadiusMeters);

        FaultEffect effect;
        if (ellipse >= 1.0)
        {
            return effect;
        }
        // Slip dies out smoothly towards the edge of the fault.
        const double amplitude = fault.slipMeters * (1.0 - ellipse) * (1.0 - ellipse);
        effect.amplitude = amplitude;
        effect.cutElevation = static_cast<double>(pileTopMeters) + fault.centerOffsetMeters - std::tan(beta) * downDip;

        // Displacement of the block above the plane.
        switch (fault.kind)
        {
        case FaultKind::Normal: // the hanging wall slides down the plane
            effect.shiftEast = amplitude * std::cos(beta) * dipEast;
            effect.shiftNorth = amplitude * std::cos(beta) * dipNorth;
            effect.lift = -amplitude * std::sin(beta);
            break;
        case FaultKind::Thrust: // and is pushed up it
            effect.shiftEast = -amplitude * std::cos(beta) * dipEast;
            effect.shiftNorth = -amplitude * std::cos(beta) * dipNorth;
            effect.lift = amplitude * std::sin(beta);
            break;
        case FaultKind::StrikeSlip:
            effect.shiftEast = fault.sense * amplitude * strikeEast;
            effect.shiftNorth = fault.sense * amplitude * strikeNorth;
            break;
        }
        return effect;
    }
}

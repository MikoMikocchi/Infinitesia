#include "geology/model/GeologyModel.h"

namespace Infinitesia
{
    const ProvinceHistory& GeologyModel::HistoryOf(ProvinceId province) const noexcept
    {
        if (province == kNoProvince || province > m_histories.size())
        {
            return m_fallbackHistory;
        }
        return m_histories[province - 1];
    }

    EventId GeologyModel::FormationEvent(ProvinceId province, std::size_t pileIndex) const noexcept
    {
        if (province == kNoProvince || province > m_histories.size() || pileIndex >= m_pileEvents[province - 1].size())
        {
            return 0;
        }
        const ProvinceHistory& history = m_histories[province - 1];
        return history.events[static_cast<std::size_t>(m_pileEvents[province - 1][pileIndex])].id;
    }

    std::vector<EventId> GeologyModel::ProvenanceAt(
        ProvinceId province, double eastMeters, double northMeters, int elevationMeters, int pileTopMeters) const
    {
        std::vector<EventId> chain;
        const ProvinceHistory& history = HistoryOf(province);
        if (history.events.empty())
        {
            return chain;
        }

        const ColumnProfile profile = ProfileAt(province, eastMeters, northMeters, pileTopMeters);
        chain.push_back(FormationEvent(province, FormationAt(profile, elevationMeters)));

        // Everything in the province was folded after it formed.
        if (history.foldingEvent >= 0)
            chain.push_back(history.events[static_cast<std::size_t>(history.foldingEvent)].id);

        // A fault displaced the rock if the point lies in the block above a fault plane.
        for (const Fault& fault : FaultsNear(province, eastMeters, northMeters))
        {
            const FaultEffect effect = EffectOf(fault, eastMeters, northMeters, pileTopMeters);
            if (effect.amplitude > 0.0 && elevationMeters > effect.cutElevation && history.faultingEvent >= 0)
            {
                chain.push_back(history.events[static_cast<std::size_t>(history.faultingEvent)].id);
                break;
            }
        }
        if (history.erosionEvent >= 0)
            chain.push_back(history.events[static_cast<std::size_t>(history.erosionEvent)].id);
        return chain;
    }
}

#include "simulation/runtime/SystemScheduler.h"

#include <algorithm>
#include <cstddef>

namespace Infinitesia
{
    SystemScheduler::SystemScheduler(SystemRegistry& registry, SystemExecutor& executor, GameCalendar calendar, float fixedDeltaTime)
        : m_registry(registry),
          m_executor(executor),
          m_calendar(calendar),
          m_fixedDeltaTime(fixedDeltaTime)
    {
    }

    void SystemScheduler::RebuildPlanIfStale()
    {
        if (m_planBuilt && m_planRevision == m_registry.Revision())
        {
            return;
        }

        for (auto& phasePlan : m_plan)
        {
            phasePlan.clear();
        }
        for (const auto& entry : m_registry.Systems())
        {
            m_plan[static_cast<std::size_t>(entry->descriptor.phase)].push_back(entry.get());
        }
        for (auto& phasePlan : m_plan)
        {
            std::sort(phasePlan.begin(), phasePlan.end(), [](const RegisteredSystem* a, const RegisteredSystem* b)
            {
                if (a->descriptor.order != b->descriptor.order)
                {
                    return a->descriptor.order < b->descriptor.order;
                }
                return a->sequence < b->sequence;
            });
        }
        m_planRevision = m_registry.Revision();
        m_planBuilt = true;
    }

    std::vector<SystemId> SystemScheduler::ExecutionOrder(SystemPhase phase)
    {
        RebuildPlanIfStale();
        std::vector<SystemId> ids;
        for (const RegisteredSystem* entry : m_plan[static_cast<std::size_t>(phase)])
        {
            ids.push_back(entry->descriptor.id);
        }
        return ids;
    }

    void SystemScheduler::RunTick(std::uint64_t tick)
    {
        RebuildPlanIfStale();
        m_executor.BeginTick(tick);

        const std::uint64_t endTick = tick + 1;
        const std::uint64_t endSeconds = m_calendar.SecondsAfterTicks(endTick);
        const GameTime endTime = m_calendar.At(endSeconds);

        for (const SystemPhase phase : kSystemPhases)
        {
            // Registering a system mid-tick would only rebuild the plan next tick; the phase
            // vector being walked is not touched.
            for (RegisteredSystem* entry : m_plan[static_cast<std::size_t>(phase)])
            {
                if (!entry->enabled || !IsDue(entry->descriptor.cadence, m_calendar, tick))
                {
                    continue;
                }

                SystemContext context;
                context.tick = tick;
                context.phase = phase;
                context.fixedDeltaTime = m_fixedDeltaTime;
                context.time = endTime;
                context.elapsedTicks = endTick - entry->lastRunEndTick;
                context.elapsedGameSeconds = endSeconds - m_calendar.SecondsAfterTicks(entry->lastRunEndTick);
                m_executor.Execute(*entry, context);
            }
        }

        m_executor.EndTick();
    }
}

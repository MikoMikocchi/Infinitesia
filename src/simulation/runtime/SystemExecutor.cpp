#include "simulation/runtime/SystemExecutor.h"

#include <exception>
#include <stdexcept>

namespace Infinitesia
{
    SystemExecutor::SystemExecutor(SystemErrorPolicy policy)
        : m_policy(policy)
    {
    }

    void SystemExecutor::BeginTick(std::uint64_t tick)
    {
        if (tick != m_completedTicks)
        {
            throw std::logic_error("Simulation tick " + std::to_string(tick) + " begun out of order, expected "
                                   + std::to_string(m_completedTicks) + ".");
        }
        m_inTick = true;
        m_currentTick = tick;
        m_currentPhase = SystemPhase::PreUpdate;
    }

    void SystemExecutor::EndTick()
    {
        if (!m_inTick)
        {
            throw std::logic_error("Simulation tick ended without being begun.");
        }
        m_inTick = false;
        ++m_completedTicks;
    }

    void SystemExecutor::HandleFailure(RegisteredSystem& entry, const SystemContext& context, const char* message)
    {
        m_failures.push_back({ entry.descriptor.id, context.tick, context.phase, message });
        switch (m_policy)
        {
        case SystemErrorPolicy::Disable: entry.enabled = false; break;
        case SystemErrorPolicy::Continue: break;
        case SystemErrorPolicy::Rethrow: throw;
        }
    }

    void SystemExecutor::Execute(RegisteredSystem& entry, const SystemContext& context)
    {
        if (!m_inTick || context.tick != m_currentTick)
        {
            throw std::logic_error("System '" + entry.descriptor.id.Key() + "' executed outside its tick.");
        }
        if (context.phase < m_currentPhase)
        {
            throw std::logic_error(std::string("System '") + entry.descriptor.id.Key() + "' executed after a later phase than "
                                   + SystemPhaseName(context.phase) + ".");
        }
        m_currentPhase = context.phase;

        try
        {
            entry.system->Update(context);
        }
        catch (const std::exception& error)
        {
            HandleFailure(entry, context, error.what());
            return;
        }
        catch (...)
        {
            HandleFailure(entry, context, "unknown exception");
            return;
        }

        ++entry.runCount;
        entry.lastRunEndTick = context.tick + 1;
    }
}

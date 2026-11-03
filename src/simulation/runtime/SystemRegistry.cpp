#include "simulation/runtime/SystemRegistry.h"

#include <stdexcept>
#include <utility>

namespace Infinitesia
{
    const RegisteredSystem& SystemRegistry::Register(SystemDescriptor descriptor, std::unique_ptr<SimulationSystem> system)
    {
        if (descriptor.id.IsEmpty())
        {
            throw std::invalid_argument("A simulation system has no id.");
        }
        if (m_indexByKey.contains(descriptor.id.Key()))
        {
            throw std::invalid_argument("Duplicate simulation system id '" + descriptor.id.Key() + "'.");
        }
        if (!system)
        {
            throw std::invalid_argument("Simulation system '" + descriptor.id.Key() + "' is null.");
        }
        if (!descriptor.cadence.IsValid())
        {
            throw std::invalid_argument("Simulation system '" + descriptor.id.Key() + "' has a zero cadence interval.");
        }

        auto entry = std::make_unique<RegisteredSystem>();
        entry->sequence = m_systems.size();
        entry->enabled = descriptor.enabled;
        entry->descriptor = std::move(descriptor);
        entry->system = std::move(system);

        m_indexByKey.emplace(entry->descriptor.id.Key(), m_systems.size());
        m_systems.push_back(std::move(entry));
        ++m_revision;
        return *m_systems.back();
    }

    bool SystemRegistry::Contains(const SystemId& id) const
    {
        return m_indexByKey.contains(id.Key());
    }

    const RegisteredSystem* SystemRegistry::Find(const SystemId& id) const
    {
        const auto found = m_indexByKey.find(id.Key());
        return found == m_indexByKey.end() ? nullptr : m_systems[found->second].get();
    }

    RegisteredSystem* SystemRegistry::Find(const SystemId& id)
    {
        const auto found = m_indexByKey.find(id.Key());
        return found == m_indexByKey.end() ? nullptr : m_systems[found->second].get();
    }

    RegisteredSystem& SystemRegistry::Get(const SystemId& id) const
    {
        const auto found = m_indexByKey.find(id.Key());
        if (found == m_indexByKey.end())
        {
            throw std::out_of_range("No simulation system '" + id.Key() + "'.");
        }
        return *m_systems[found->second];
    }

    void SystemRegistry::SetEnabled(const SystemId& id, bool enabled)
    {
        Get(id).enabled = enabled;
    }

    bool SystemRegistry::IsEnabled(const SystemId& id) const
    {
        return Get(id).enabled;
    }
}

#pragma once

#include "simulation/api/SimulationSystem.h"
#include "simulation/api/SystemDescriptor.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Infinitesia
{
    // A registered system and what the runtime has seen of it.
    struct RegisteredSystem
    {
        SystemDescriptor descriptor;
        std::unique_ptr<SimulationSystem> system;
        std::size_t sequence = 0;            // registration order
        bool enabled = true;
        std::uint64_t runCount = 0;
        std::uint64_t lastRunEndTick = 0;    // ticks completed when it last ran; 0 before the first run
    };

    // Every system of the simulation, by unique id. Entries never move once added, so pointers
    // to them stay valid for the life of the registry.
    class SystemRegistry
    {
    public:
        // Throws std::invalid_argument for an empty or repeated id, a null system or a cadence
        // with a zero interval.
        const RegisteredSystem& Register(SystemDescriptor descriptor, std::unique_ptr<SimulationSystem> system);

        bool Contains(const SystemId& id) const;
        // nullptr when there is no such system.
        const RegisteredSystem* Find(const SystemId& id) const;
        RegisteredSystem* Find(const SystemId& id);
        std::size_t Size() const { return m_systems.size(); }
        // In registration order. The scheduler and executor update the runtime state of entries.
        const std::vector<std::unique_ptr<RegisteredSystem>>& Systems() const { return m_systems; }

        // Throws std::out_of_range for an unknown id.
        void SetEnabled(const SystemId& id, bool enabled);
        bool IsEnabled(const SystemId& id) const;

        // Grows with every registration; lets the scheduler know its plan is stale.
        std::uint64_t Revision() const { return m_revision; }

    private:
        RegisteredSystem& Get(const SystemId& id) const;

        std::vector<std::unique_ptr<RegisteredSystem>> m_systems;
        std::unordered_map<std::string, std::size_t> m_indexByKey;
        std::uint64_t m_revision = 0;
    };
}

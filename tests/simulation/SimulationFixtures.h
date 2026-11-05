#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simulation/api/SimulationSystem.h"
#include "simulation/runtime/SystemRegistry.h"

namespace InfinitesiaTests
{
using namespace Infinitesia;

// Appends its name to a shared log on every run and remembers the contexts it
// was given.
class RecordingSystem : public SimulationSystem
{
public:
	RecordingSystem(std::string name, std::vector<std::string> *log) : m_name(std::move(name)), m_log(log)
	{
	}

	void Update(const SystemContext &context) override
	{
		if (m_log)
		{
			m_log->push_back(m_name);
		}
		contexts.push_back(context);
		if (onUpdate)
		{
			onUpdate(context);
		}
	}

	std::vector<SystemContext> contexts;
	std::function<void(const SystemContext &)> onUpdate;

private:
	std::string m_name;
	std::vector<std::string> *m_log;
};

inline SystemDescriptor Describe(const char *id, SystemPhase phase = SystemPhase::Update,
                                 Cadence cadence = Cadence::EveryTick(), int order = 0)
{
	SystemDescriptor descriptor;
	descriptor.id = SystemId(id);
	descriptor.phase = phase;
	descriptor.cadence = cadence;
	descriptor.order = order;
	return descriptor;
}

// Registers a RecordingSystem and returns it (the registry owns it).
inline RecordingSystem &AddRecording(SystemRegistry &registry, SystemDescriptor descriptor,
                                     std::vector<std::string> *log = nullptr)
{
	auto system = std::make_unique<RecordingSystem>(descriptor.id.Key(), log);
	RecordingSystem &reference = *system;
	registry.Register(std::move(descriptor), std::move(system));
	return reference;
}
} // namespace InfinitesiaTests

#include "gameplay/LocalGame.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gameplay/commands/PickUpItemCommand.h"
#include "simulation/api/Cadence.h"
#include "simulation/api/SimulationSystem.h"
#include "simulation/api/SystemContext.h"
#include "simulation/api/SystemDescriptor.h"
#include "simulation/api/SystemPhase.h"

namespace Infinitesia
{
namespace
{
std::vector<WorldObjectPlacement> StartingObjects(const GameContent &content, const StartingCells &cells)
{
	return {{&content.GetObjectKind("object.worker"), cells.worker}};
}

// The fluid step stays on the fixed tick, but now it is a normal system instead
// of a manual loop in the game frame. It runs after work so a worker's dig or
// build is seen by the same tick's water.
class FluidSystem final : public SimulationSystem
{
public:
	explicit FluidSystem(GameWorld &world) : m_world(world)
	{
	}

	void Update(const SystemContext &) override
	{
		m_world.terrain.StepFluids();
	}

private:
	GameWorld &m_world;
};
} // namespace

LocalGame::LocalGame(std::shared_ptr<const GameContent> content, const LocalGameConfig &config,
                     std::shared_ptr<const TerrainSource> terrain, SimulationRuntime &runtime,
                     std::string systemPrefix)
    : m_content(std::move(content)), m_grid(config.grid), m_start(FindStartingCells(m_grid, *terrain)),
      m_world(m_grid, StartingObjects(*m_content, m_start), terrain), m_runtime(&runtime),
      m_systemPrefix(std::move(systemPrefix))
{
	// A felled tree leaves its logs as piles of the item object.
	const WoodDrop drop{&m_content->GetObjectKind("object.item"), &m_content->GetItem("item.logs")};
	auto workSystem = std::make_unique<WorkSystem>(config.worker, m_world, m_grid, drop);
	m_workSystem = workSystem.get();
	m_workSystemId = SystemId(m_systemPrefix + ".work");
	const SystemDescriptor workDescriptor{m_workSystemId, SystemPhase::Update, Cadence::EveryTick()};
	m_runtime->Registry().Register(workDescriptor, std::move(workSystem));

	m_fluidSystemId = SystemId(m_systemPrefix + ".fluid");
	const SystemDescriptor fluidDescriptor{m_fluidSystemId, SystemPhase::PostUpdate, Cadence::EveryTick()};
	m_runtime->Registry().Register(fluidDescriptor, std::make_unique<FluidSystem>(m_world));

	m_commands.Register(std::make_unique<PickUpItemCommand>());
}

LocalGame::~LocalGame()
{
	if (m_runtime != nullptr)
	{
		m_runtime->Registry().Unregister(m_workSystemId);
		m_runtime->Registry().Unregister(m_fluidSystemId);
	}
}

void LocalGame::SetEnabled(bool enabled)
{
	if (m_runtime == nullptr)
	{
		return;
	}
	m_runtime->Registry().SetEnabled(m_workSystemId, enabled);
	m_runtime->Registry().SetEnabled(m_fluidSystemId, enabled);
}

bool LocalGame::IsEnabled() const
{
	return m_runtime != nullptr && m_runtime->Registry().IsEnabled(m_workSystemId);
}
} // namespace Infinitesia

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "gameplay/GameWorld.h"
#include "gameplay/StartingCells.h"
#include "gameplay/commands/CommandRegistry.h"
#include "gameplay/content/GameContent.h"
#include "gameplay/work/WorkSystem.h"
#include "simulation/api/SystemId.h"
#include "simulation/runtime/SimulationRuntime.h"
#include "world/Grid.h"
#include "world/TerrainSource.h"

namespace Infinitesia
{
struct LocalGameConfig
{
	GridConfig grid;
	WorkerConfig worker;
};

// One settlement being played: its grid, the world on it, the work being done
// and the commands that can be given. No window, no input; the application
// feeds it orders and enables its systems on the campaign clock. It holds
// references to its own members (commands point at the work system and the
// grid), so it can be neither copied nor moved: build it in place.
class LocalGame
{
public:
	// Puts the first worker on the terrain. The content must define
	// "object.worker" and "object.item" with "item.logs" (what a felled tree
	// leaves). The local systems register on `runtime` under `systemPrefix`;
	// the runtime must outlive this game.
	LocalGame(std::shared_ptr<const GameContent> content, const LocalGameConfig &config,
	          std::shared_ptr<const TerrainSource> terrain, SimulationRuntime &runtime, std::string systemPrefix);
	~LocalGame();

	LocalGame(const LocalGame &) = delete;
	LocalGame &operator=(const LocalGame &) = delete;

	// Enables or disables this site's work and fluid systems on the campaign
	// clock. Only the active site should be enabled.
	void SetEnabled(bool enabled);
	bool IsEnabled() const;

	const Grid &GetGrid() const
	{
		return m_grid;
	}
	Grid &GetGrid()
	{
		return m_grid;
	}
	const GameWorld &GetWorld() const
	{
		return m_world;
	}
	GameWorld &GetWorld()
	{
		return m_world;
	}
	const WorkSystem &GetWorkSystem() const
	{
		return *m_workSystem;
	}
	WorkSystem &GetWorkSystem()
	{
		return *m_workSystem;
	}
	const CommandRegistry &GetCommands() const
	{
		return m_commands;
	}
	// Where the first worker stands.
	const StartingCells &GetStartingCells() const
	{
		return m_start;
	}

private:
	// Declared before the world: objects and items point into it.
	std::shared_ptr<const GameContent> m_content;
	Grid m_grid;
	StartingCells m_start;
	GameWorld m_world;
	// Owned by the campaign runtime's registry, which must outlive this game.
	WorkSystem *m_workSystem = nullptr;
	CommandRegistry m_commands;
	SimulationRuntime *m_runtime = nullptr;
	SystemId m_workSystemId;
	SystemId m_fluidSystemId;
	std::string m_systemPrefix;
};
} // namespace Infinitesia

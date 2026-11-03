#pragma once

#include "foundation/Simulation.h"
#include "gameplay/GameWorld.h"
#include "gameplay/StartingCells.h"
#include "gameplay/commands/CommandRegistry.h"
#include "gameplay/content/GameContent.h"
#include "gameplay/work/WorkSystem.h"
#include "simulation/runtime/SimulationRuntime.h"
#include "world/Grid.h"
#include "world/TerrainSource.h"

#include <cstdint>
#include <memory>

namespace Infinitesia
{
    struct LocalGameConfig
    {
        GridConfig grid;
        WorkerConfig worker;
        float simulationTicksPerSecond = 20.0f;
    };

    // One settlement being played: its grid, the world on it, the work being done and the
    // commands that can be given. No window, no input; the application feeds it time and
    // orders. It holds references to its own members (commands point at the work system and
    // the grid), so it can be neither copied nor moved: build it in place.
    class LocalGame
    {
    public:
        // Puts the first worker (and a tree within his reach) on the terrain. The content
        // must define "object.worker" and "object.tree".
        LocalGame(std::shared_ptr<const GameContent> content,
                  const LocalGameConfig& config,
                  std::shared_ptr<const TerrainSource> terrain);

        LocalGame(const LocalGame&) = delete;
        LocalGame& operator=(const LocalGame&) = delete;

        // Runs one fixed simulation step.
        void Tick();
        // Feeds a frame's time to the simulation and runs the steps that fell due; returns their number.
        std::uint64_t AdvanceFrame(float frameDelta);

        const Grid& GetGrid() const { return m_grid; }
        Grid& GetGrid() { return m_grid; }
        const GameWorld& GetWorld() const { return m_world; }
        GameWorld& GetWorld() { return m_world; }
        const WorkSystem& GetWorkSystem() const { return m_workSystem; }
        WorkSystem& GetWorkSystem() { return m_workSystem; }
        const CommandRegistry& GetCommands() const { return m_commands; }
        const Simulation& GetSimulation() const { return m_simulation; }
        // Systems registered here run on every fixed tick the simulation produces.
        SimulationRuntime& GetRuntime() { return m_runtime; }
        const SimulationRuntime& GetRuntime() const { return m_runtime; }
        // Where the first worker stands (and the first tree, if one was placed).
        const StartingCells& GetStartingCells() const { return m_start; }

    private:
        // Declared before the world: objects and items point into it.
        std::shared_ptr<const GameContent> m_content;
        Grid m_grid;
        StartingCells m_start;
        GameWorld m_world;
        WorkSystem m_workSystem;
        CommandRegistry m_commands;
        Simulation m_simulation;
        // After the simulation, which it listens to.
        SimulationRuntime m_runtime;
    };
}

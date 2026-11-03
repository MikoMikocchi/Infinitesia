#include "gameplay/LocalGame.h"

#include "gameplay/commands/ChopTreeCommand.h"
#include "gameplay/commands/PickUpItemCommand.h"

#include <utility>
#include <vector>

namespace Infinitesia
{
    namespace
    {
        std::vector<WorldObjectPlacement> StartingObjects(const GameContent& content, const StartingCells& cells)
        {
            std::vector<WorldObjectPlacement> objects{ { &content.GetObjectKind("object.worker"), cells.worker } };
            if (cells.tree.has_value())
            {
                objects.push_back({ &content.GetObjectKind("object.tree"), *cells.tree });
            }
            return objects;
        }
    }

    LocalGame::LocalGame(std::shared_ptr<const GameContent> content,
                         const LocalGameConfig& config,
                         std::shared_ptr<const TerrainSource> terrain)
        : m_content(std::move(content)),
          m_grid(config.grid),
          m_start(FindStartingCells(m_grid, *terrain)),
          m_world(m_grid, StartingObjects(*m_content, m_start), terrain),
          m_workSystem(config.worker),
          m_simulation(config.simulationTicksPerSecond),
          m_runtime(m_simulation)
    {
        m_commands.Register(std::make_unique<ChopTreeCommand>(m_workSystem, m_grid));
        m_commands.Register(std::make_unique<PickUpItemCommand>());
    }

    void LocalGame::Tick()
    {
        m_workSystem.Tick(m_world, m_grid, m_simulation.GetFixedDeltaTime());
    }

    std::uint64_t LocalGame::AdvanceFrame(float frameDelta)
    {
        const std::uint64_t ticks = m_simulation.AdvanceFrame(frameDelta);
        for (std::uint64_t tick = 0; tick < ticks; ++tick)
        {
            Tick();
        }
        return ticks;
    }
}

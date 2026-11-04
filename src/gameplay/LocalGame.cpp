#include "gameplay/LocalGame.h"

#include <utility>
#include <vector>

#include "gameplay/commands/PickUpItemCommand.h"

namespace Infinitesia {
namespace {
std::vector<WorldObjectPlacement> StartingObjects(const GameContent& content,
                                                  const StartingCells& cells) {
  return {{&content.GetObjectKind("object.worker"), cells.worker}};
}
}  // namespace

LocalGame::LocalGame(std::shared_ptr<const GameContent> content,
                     const LocalGameConfig& config,
                     std::shared_ptr<const TerrainSource> terrain)
    : m_content(std::move(content)),
      m_grid(config.grid),
      m_start(FindStartingCells(m_grid, *terrain)),
      m_world(m_grid, StartingObjects(*m_content, m_start), terrain),
      m_simulation(config.simulationTicksPerSecond),
      m_runtime(m_simulation) {
  // A felled tree leaves its logs as piles of the item object.
  const WoodDrop drop{&m_content->GetObjectKind("object.item"),
                      &m_content->GetItem("item.logs")};
  auto workSystem =
      std::make_unique<WorkSystem>(config.worker, m_world, m_grid, drop);
  m_workSystem = workSystem.get();
  m_runtime.Registry().Register(WorkSystem::Descriptor(),
                                std::move(workSystem));
  m_commands.Register(std::make_unique<PickUpItemCommand>());
}

std::uint64_t LocalGame::AdvanceFrame(float frameDelta) {
  // The runtime listens to the simulation and runs the due systems on every
  // tick.
  return m_simulation.AdvanceFrame(frameDelta);
}
}  // namespace Infinitesia

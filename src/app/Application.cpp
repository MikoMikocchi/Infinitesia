#include "app/Application.h"
#include "raylib.h"
#include "commands/ChopTreeCommand.h"
#include "commands/PickUpItemCommand.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
    Application::Application(ApplicationConfig config)
        : m_config(std::move(config)),
          m_grid(m_config.grid),
          m_world(m_grid, m_config.initialWorldObjects, m_config.worldSeed),
          m_simulation(m_config.simulationTicksPerSecond),
          m_workSystem(m_config.worker),
          m_worldRenderer(m_config.camera),
          m_cameraController(m_config.cameraControls),
          m_chunkViewCenter{
              m_config.grid.width / 2,
              m_config.grid.height - 1,
              m_config.grid.depth / 2
          },
          m_selectedLevel(m_config.grid.height - 1),
          m_showPathDebug(m_config.showPathDebug)
    {
        Camera3D& camera = m_worldRenderer.GetCamera();
        camera.position.y += m_grid.GetWorldHeight();
        camera.target.y = m_grid.GetWorldHeight();

        m_commands.Register(std::make_unique<ChopTreeCommand>(m_workSystem, m_grid));
        m_commands.Register(std::make_unique<PickUpItemCommand>());

        if (m_config.camera.distance < m_config.cameraControls.minimumDistance ||
            m_config.camera.distance > m_config.cameraControls.maximumDistance ||
            m_config.camera.elevationDegrees < m_config.cameraControls.minimumElevationDegrees ||
            m_config.camera.elevationDegrees > m_config.cameraControls.maximumElevationDegrees)
        {
            throw std::invalid_argument("Initial camera settings must fit within camera control limits.");
        }
    }
}

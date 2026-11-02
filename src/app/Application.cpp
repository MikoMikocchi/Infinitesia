#include "app/Application.h"
#include "raylib.h"

#include <cstdio>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
    namespace
    {
        class WindowGuard
        {
        public:
            explicit WindowGuard(const WindowConfig& config)
            {
                if (config.width <= 0 || config.height <= 0 || config.title.empty() ||
                    config.targetFramesPerSecond < 0)
                {
                    throw std::invalid_argument("Window configuration contains invalid values.");
                }

                InitWindow(config.width, config.height, config.title.c_str());
                if (!IsWindowReady())
                {
                    CloseWindow();
                    throw std::runtime_error("Failed to initialize the application window.");
                }
            }

            ~WindowGuard()
            {
                CloseWindow();
            }

            WindowGuard(const WindowGuard&) = delete;
            WindowGuard& operator=(const WindowGuard&) = delete;
        };
    }

    Application::Application(ApplicationConfig config)
        : m_config(std::move(config)),
          m_grid(m_config.grid),
          m_world(m_grid, m_config.initialWorldObjects),
          m_simulation(m_config.simulationTicksPerSecond),
          m_worldRenderer(m_config.camera),
          m_cameraController(m_config.cameraControls)
    {
        if (m_config.camera.distance < m_config.cameraControls.minimumDistance ||
            m_config.camera.distance > m_config.cameraControls.maximumDistance ||
            m_config.camera.elevationDegrees < m_config.cameraControls.minimumElevationDegrees ||
            m_config.camera.elevationDegrees > m_config.cameraControls.maximumElevationDegrees)
        {
            throw std::invalid_argument("Initial camera settings must fit within camera control limits.");
        }
    }

    void Application::Run()
    {
        WindowGuard window(m_config.window);
        SetTargetFPS(m_config.window.targetFramesPerSecond);

        while (!WindowShouldClose())
        {
            m_time.Advance(GetFrameTime());
            m_simulation.AdvanceFrame(m_time.GetDeltaTime());
            Update();
            Render();
        }
    }

    void Application::Update()
    {
        m_cameraController.Update(m_worldRenderer.GetCamera(), m_time.GetDeltaTime());
        m_hoveredCell = m_gridPointer.PickCell(
            m_grid,
            m_worldRenderer.GetCamera(),
            GetMousePosition());
        m_hoveredObject = m_worldObjectPicker.PickObject(
            m_grid,
            m_world,
            m_worldRenderer.GetCamera(),
            GetMousePosition());

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            m_selectedObject = m_hoveredObject;
        }
    }

    void Application::Render()
    {
        BeginDrawing();
        ClearBackground({ 30, 34, 39, 255 });

        m_worldRenderer.Draw(m_grid, m_world, m_hoveredCell, m_hoveredObject, m_selectedObject);
        DrawText(
            m_config.window.title.c_str(),
            (m_config.window.width - MeasureText(m_config.window.title.c_str(), 24)) / 2,
            12,
            24,
            RAYWHITE);
        DrawDebugOverlay();

        EndDrawing();
    }

    void Application::DrawDebugOverlay()
    {
        const int padding = 8;
        const int fontSize = 16;
        const int lineHeight = 20;
        const int panelWidth = 250;
        const int panelHeight = 5 * lineHeight + padding;

        char buf[128];

        DrawRectangle(padding - 4, padding - 4, panelWidth, panelHeight, Fade(BLACK, 0.4f));

        std::snprintf(buf, sizeof(buf), "FPS: %d", GetFPS());
        DrawText(buf, padding, padding, fontSize, LIME);

        std::snprintf(buf, sizeof(buf), "Time since start: %.3f s", m_time.GetTotalTime());
        DrawText(buf, padding, padding + lineHeight, fontSize, SKYBLUE);

        std::snprintf(buf, sizeof(buf), "Ticks: %llu", static_cast<unsigned long long>(m_simulation.GetTickCount()));
        DrawText(buf, padding, padding + 2 * lineHeight, fontSize, ORANGE);

        std::snprintf(buf, sizeof(buf), "Grid: %d x %d", m_grid.GetColumns(), m_grid.GetRows());
        DrawText(buf, padding, padding + 3 * lineHeight, fontSize, RAYWHITE);

        if (m_hoveredCell.has_value())
        {
            std::snprintf(
                buf,
                sizeof(buf),
                "Cell: %d, %d",
                m_hoveredCell->column,
                m_hoveredCell->row);
            DrawText(buf, padding, padding + 4 * lineHeight, fontSize, GOLD);
        }

        if (m_selectedObject.has_value())
        {
            const WorldObject* selectedObject = m_world.FindObject(*m_selectedObject);
            if (selectedObject != nullptr)
            {
                const char* typeName = selectedObject->type == WorldObjectType::Worker ? "Worker" : "Tree";
                std::snprintf(buf, sizeof(buf), "Selected: %s", typeName);
                DrawText(buf, padding, padding + 5 * lineHeight, fontSize, GREEN);
            }
        }
    }
}

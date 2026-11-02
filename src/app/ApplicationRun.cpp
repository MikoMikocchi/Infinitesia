#include "app/Application.h"
#include "raylib.h"

#include <stdexcept>

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

        class WorldRendererShaderGuard
        {
        public:
            explicit WorldRendererShaderGuard(WorldRenderer& renderer)
                : m_renderer(renderer)
            {
                m_renderer.LoadShaders();
            }

            ~WorldRendererShaderGuard()
            {
                m_renderer.UnloadShaders();
            }

            WorldRendererShaderGuard(const WorldRendererShaderGuard&) = delete;
            WorldRendererShaderGuard& operator=(const WorldRendererShaderGuard&) = delete;

        private:
            WorldRenderer& m_renderer;
        };

        class CharacterProfileAssetsGuard
        {
        public:
            explicit CharacterProfileAssetsGuard(WorldInteractionController& controller)
                : m_controller(controller)
            {
                m_controller.LoadAssets();
            }

            ~CharacterProfileAssetsGuard()
            {
                m_controller.UnloadAssets();
            }

            CharacterProfileAssetsGuard(const CharacterProfileAssetsGuard&) = delete;
            CharacterProfileAssetsGuard& operator=(const CharacterProfileAssetsGuard&) = delete;

        private:
            WorldInteractionController& m_controller;
        };
    }

    void Application::Run()
    {
        WindowGuard window(m_config.window);
        WorldRendererShaderGuard rendererShaders(m_worldRenderer);
        CharacterProfileAssetsGuard profileAssets(m_worldInteraction);
        SetTargetFPS(m_config.window.targetFramesPerSecond);

        while (!WindowShouldClose())
        {
            m_time.Advance(GetFrameTime());
            const std::uint64_t advancedTicks = m_simulation.AdvanceFrame(m_time.GetDeltaTime());
            for (std::uint64_t tick = 0; tick < advancedTicks; ++tick)
            {
                m_workSystem.Tick(m_world, m_grid, m_simulation.GetFixedDeltaTime());
            }
            Update();
            Render();
        }
    }
}

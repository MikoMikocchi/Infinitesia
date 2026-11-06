#include <cstdint>
#include <filesystem>
#include <stdexcept>

#include "app/Application.h"
#include "raylib.h"

namespace Infinitesia
{
namespace
{
class WindowGuard
{
public:
	explicit WindowGuard(const WindowConfig &config)
	{
		if (config.width <= 0 || config.height <= 0 || config.title.empty() || config.targetFramesPerSecond < 0)
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

	WindowGuard(const WindowGuard &) = delete;
	WindowGuard &operator=(const WindowGuard &) = delete;
};
} // namespace

namespace
{
// Loads the world shaders once the window (and so the GL context) exists and
// releases them before it goes away.
class WorldRendererShaderGuard
{
public:
	explicit WorldRendererShaderGuard(WorldRenderer &renderer) : m_renderer(renderer)
	{
		m_renderer.LoadShaders();
	}

	~WorldRendererShaderGuard()
	{
		m_renderer.UnloadShaders();
	}

	WorldRendererShaderGuard(const WorldRendererShaderGuard &) = delete;
	WorldRendererShaderGuard &operator=(const WorldRendererShaderGuard &) = delete;

private:
	WorldRenderer &m_renderer;
};
} // namespace

namespace
{
class TextureAssetsGuard
{
public:
	TextureAssetsGuard(TextureAssetManager &assets, const std::filesystem::path &assetDirectory) : m_assets(assets)
	{
		m_assets.LoadTextures(assetDirectory);
	}

	~TextureAssetsGuard()
	{
		m_assets.UnloadTextures();
	}

	TextureAssetsGuard(const TextureAssetsGuard &) = delete;
	TextureAssetsGuard &operator=(const TextureAssetsGuard &) = delete;

private:
	TextureAssetManager &m_assets;
};
} // namespace

void Application::Run()
{
	WindowGuard window(m_config.window);
	WorldRendererShaderGuard rendererShaders(m_worldRenderer);
	TextureAssetsGuard textureAssets(m_textureAssets, std::filesystem::path(GetApplicationDirectory()) / "assets");
	SetTargetFPS(m_config.window.targetFramesPerSecond);

	StartPlanetLoad();

	try
	{
		while (!WindowShouldClose() && !m_exitRequested)
		{
			m_time.Advance(GetFrameTime());
			// One clock for the whole campaign: the planet keeps ticking while
			// the player is on the map, and the active site adds its local
			// systems to the same tick. Nothing ticks before the planet exists.
			if (m_planetViewer)
			{
				m_campaign.AdvanceFrame(m_time.GetDeltaTime());
			}
			Update();
			Render();
		}
	}
	catch (...)
	{
		if (m_planetViewer)
		{
			m_planetViewer->ReleaseRenderResources();
		}
		throw;
	}
	if (m_planetViewer)
	{
		m_planetViewer->ReleaseRenderResources();
	}
}
} // namespace Infinitesia

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

namespace Infinitesia::Platform
{
struct WindowConfig
{
	int width = 1280;
	int height = 720;
	int targetFramesPerSecond = 60;
	bool vsync = true;
	std::string title = "Infinitesia";
};

// Owns the SDL3 window and its OpenGL 3.3 Core context. This is the only place
// that talks to SDL's video subsystem; the rest of the game sees sizes, events,
// time and the base path.
class Window
{
public:
	explicit Window(WindowConfig config = {});
	~Window();

	Window(const Window &) = delete;
	Window &operator=(const Window &) = delete;
	Window(Window &&) = delete;
	Window &operator=(Window &&) = delete;

	bool ShouldClose() const noexcept
	{
		return m_shouldClose;
	}
	bool IsFocused() const noexcept
	{
		return m_focused;
	}

	// Drains SDL's event queue and updates the window size and focus state.
	void PollEvents();
	// Presents the back buffer and advances the frame clock.
	void SwapBuffers();

	int GetWidth() const noexcept
	{
		return m_width;
	}
	int GetHeight() const noexcept
	{
		return m_height;
	}
	int GetPixelWidth() const noexcept
	{
		return m_pixelWidth;
	}
	int GetPixelHeight() const noexcept
	{
		return m_pixelHeight;
	}

	double GetTimeSeconds() const noexcept;
	float GetDeltaTimeSeconds() const noexcept
	{
		return m_deltaTimeSeconds;
	}

	void SetTargetFramesPerSecond(int framesPerSecond) noexcept
	{
		m_targetFramesPerSecond = framesPerSecond;
	}
	int GetTargetFramesPerSecond() const noexcept
	{
		return m_targetFramesPerSecond;
	}

	// The directory of the executable, with a trailing separator.
	std::filesystem::path GetBasePath() const;

private:
	void UpdateSize();

	SDL_Window *m_window = nullptr;
	SDL_GLContext m_glContext = nullptr;
	bool m_shouldClose = false;
	bool m_focused = true;
	int m_width = 0;
	int m_height = 0;
	int m_pixelWidth = 0;
	int m_pixelHeight = 0;
	int m_targetFramesPerSecond = 0;
	std::uint64_t m_counterFrequency = 1;
	std::uint64_t m_startCounter = 0;
	std::uint64_t m_lastCounter = 0;
	float m_deltaTimeSeconds = 0.0f;
};
} // namespace Infinitesia::Platform

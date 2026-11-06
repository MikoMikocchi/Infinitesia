#include "platform/Window.h"

#include <stdexcept>
#include <string>
#include <utility>

#include "gfx/Gl.h"

namespace Infinitesia::Platform
{
namespace
{
[[noreturn]] void ThrowSdl(const char *what)
{
	throw std::runtime_error(std::string(what) + ": " + SDL_GetError());
}
} // namespace

Window::Window(WindowConfig config)
{
	SDL_SetMainReady();
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		ThrowSdl("SDL_Init");
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#if defined(__APPLE__)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);

	m_window = SDL_CreateWindow(config.title.c_str(), config.width, config.height,
	                            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (m_window == nullptr)
	{
		SDL_Quit();
		ThrowSdl("SDL_CreateWindow");
	}

	m_glContext = SDL_GL_CreateContext(m_window);
	if (m_glContext == nullptr)
	{
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
		SDL_Quit();
		ThrowSdl("SDL_GL_CreateContext");
	}
	if (!SDL_GL_MakeCurrent(m_window, m_glContext))
	{
		SDL_GL_DestroyContext(m_glContext);
		m_glContext = nullptr;
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
		SDL_Quit();
		ThrowSdl("SDL_GL_MakeCurrent");
	}

	SDL_GL_SetSwapInterval(config.vsync ? 1 : 0);

	std::string glError;
	if (!Gfx::Gl::LoadFunctions(&glError))
	{
		SDL_GL_DestroyContext(m_glContext);
		m_glContext = nullptr;
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
		SDL_Quit();
		throw std::runtime_error("Failed to load OpenGL functions: " + glError);
	}

	m_counterFrequency = SDL_GetPerformanceFrequency();
	m_startCounter = m_lastCounter = SDL_GetPerformanceCounter();
	m_targetFramesPerSecond = config.targetFramesPerSecond;
	UpdateSize();
}

Window::~Window()
{
	if (m_glContext != nullptr)
	{
		SDL_GL_DestroyContext(m_glContext);
		m_glContext = nullptr;
	}
	if (m_window != nullptr)
	{
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
	}
	SDL_Quit();
}

void Window::UpdateSize()
{
	if (m_window == nullptr)
	{
		return;
	}
	if (!SDL_GetWindowSize(m_window, &m_width, &m_height))
	{
		ThrowSdl("SDL_GetWindowSize");
	}
	if (!SDL_GetWindowSizeInPixels(m_window, &m_pixelWidth, &m_pixelHeight))
	{
		ThrowSdl("SDL_GetWindowSizeInPixels");
	}
}

void Window::PollEvents()
{
	SDL_Event event{};
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_EVENT_QUIT:
			m_shouldClose = true;
			break;

		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			if (event.window.windowID == SDL_GetWindowID(m_window))
			{
				m_shouldClose = true;
			}
			break;

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			UpdateSize();
			break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			m_focused = true;
			break;

		case SDL_EVENT_WINDOW_FOCUS_LOST:
			m_focused = false;
			break;
		}
	}
}

void Window::SwapBuffers()
{
	const std::uint64_t now = SDL_GetPerformanceCounter();
	m_deltaTimeSeconds =
	    static_cast<float>(static_cast<double>(now - m_lastCounter) / static_cast<double>(m_counterFrequency));
	m_lastCounter = now;

	SDL_GL_SwapWindow(m_window);
}

double Window::GetTimeSeconds() const noexcept
{
	const std::uint64_t now = SDL_GetPerformanceCounter();
	return static_cast<double>(now - m_startCounter) / static_cast<double>(m_counterFrequency);
}

std::filesystem::path Window::GetBasePath() const
{
	const char *basePath = SDL_GetBasePath();
	if (basePath == nullptr)
	{
		return {};
	}
	return std::filesystem::path(basePath);
}
} // namespace Infinitesia::Platform

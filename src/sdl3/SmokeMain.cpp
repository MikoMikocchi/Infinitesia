#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include "gfx/Gl.h"
#include "platform/Window.h"

int main(int argc, char **argv)
{
	try
	{
		int maxFrames = 0;
		if (const char *frames = std::getenv("INFINITESIA_SMOKE_FRAMES"))
		{
			maxFrames = std::atoi(frames);
		}
		if (argc >= 3 && std::string(argv[1]) == "--frames")
		{
			maxFrames = std::atoi(argv[2]);
		}

		Infinitesia::Platform::Window window({1280, 720, 60, true, "Infinitesia - SDL3/OpenGL smoke"});

		const auto *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
		const auto *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
		std::printf("SDL3/OpenGL smoke\n");
		std::printf("  OpenGL: %s\n", version != nullptr ? version : "unknown");
		std::printf("  Renderer: %s\n", renderer != nullptr ? renderer : "unknown");
		std::printf("  Base path: %s\n", window.GetBasePath().string().c_str());
		std::fflush(stdout);

		int frame = 0;
		while (!window.ShouldClose() && (maxFrames <= 0 || frame < maxFrames))
		{
			window.PollEvents();
			if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_ESCAPE])
			{
				break;
			}

			const int width = window.GetPixelWidth();
			const int height = window.GetPixelHeight();
			glViewport(0, 0, width, height);

			const float time = static_cast<float>(window.GetTimeSeconds());
			glClearColor(0.12f + 0.08f * std::sin(time * 1.7f), 0.16f + 0.08f * std::sin(time * 2.1f),
			             0.22f + 0.08f * std::sin(time * 2.7f), 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			window.SwapBuffers();
			++frame;
		}

		std::printf("SDL3 smoke finished cleanly after %d frame(s).\n", frame);
		return 0;
	}
	catch (const std::exception &exception)
	{
		std::fprintf(stderr, "SDL3 smoke failed: %s\n", exception.what());
		return 1;
	}
}

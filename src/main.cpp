#include <cstdio>
#include <exception>

#include "app/Application.h"

int main()
{
	try
	{
		Infinitesia::Application app;
		app.Run();
		return 0;
	}
	catch (const std::exception &exception)
	{
		std::fprintf(stderr, "Infinitesia failed: %s\n", exception.what());
		return 1;
	}
}

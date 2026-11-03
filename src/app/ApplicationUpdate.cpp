#include "app/Application.h"

namespace Infinitesia
{
    void Application::Update()
    {
        PollPlanetLoad();
        if (m_planetViewer)
        {
            m_planetViewer->Update();
        }
    }
}

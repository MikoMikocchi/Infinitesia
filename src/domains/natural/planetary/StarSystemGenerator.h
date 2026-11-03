#pragma once

#include "domains/natural/planetary/StarSystem.h"

namespace Infinitesia
{
    class StarSystemGenerator
    {
    public:
        static StarSystem Generate(const GenerationScenario& scenario);
    };
}

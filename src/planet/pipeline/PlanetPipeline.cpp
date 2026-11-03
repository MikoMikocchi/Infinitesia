#include "planet/pipeline/PlanetPipeline.h"

#include <map>
#include <set>
#include <utility>

namespace Infinitesia
{
    PlanetPipeline& PlanetPipeline::Add(PlanetStage stage)
    {
        m_stages.push_back(std::move(stage));
        return *this;
    }

    std::vector<const PlanetStage*> PlanetPipeline::Plan() const
    {
        const std::size_t count = m_stages.size();

        std::set<std::string> names;
        std::map<std::string, std::size_t> providerOf;
        for (std::size_t index = 0; index < count; ++index)
        {
            const PlanetStage& stage = m_stages[index];
            if (stage.name.empty() || !stage.run)
            {
                throw PipelineError("A planet stage has no name or nothing to run");
            }
            if (!names.insert(stage.name).second)
            {
                throw PipelineError("Two planet stages are named '" + stage.name + "'");
            }
            for (const std::string& product : stage.provides)
            {
                const auto [found, added] = providerOf.emplace(product, index);
                if (!added)
                {
                    throw PipelineError("Stages '" + m_stages[found->second].name + "' and '" + stage.name +
                                        "' both provide '" + product + "'");
                }
            }
        }

        // The stages each stage waits for.
        std::vector<std::set<std::size_t>> waitsFor(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            for (const std::string& product : m_stages[index].needs)
            {
                const auto provider = providerOf.find(product);
                if (provider == providerOf.end())
                {
                    throw PipelineError("Stage '" + m_stages[index].name + "' needs '" + product +
                                        "', which no stage provides");
                }
                if (provider->second != index)
                {
                    waitsFor[index].insert(provider->second);
                }
            }
        }

        // Repeatedly take the first stage (in Add order) whose providers have all run.
        std::vector<const PlanetStage*> plan;
        std::vector<bool> done(count, false);
        while (plan.size() < count)
        {
            bool progressed = false;
            for (std::size_t index = 0; index < count; ++index)
            {
                if (done[index])
                {
                    continue;
                }
                bool ready = true;
                for (const std::size_t dependency : waitsFor[index])
                {
                    ready = ready && done[dependency];
                }
                if (ready)
                {
                    done[index] = true;
                    plan.push_back(&m_stages[index]);
                    progressed = true;
                    break;
                }
            }
            if (!progressed)
            {
                std::string stuck;
                for (std::size_t index = 0; index < count; ++index)
                {
                    if (!done[index])
                    {
                        stuck += (stuck.empty() ? "'" : ", '") + m_stages[index].name + "'";
                    }
                }
                throw PipelineError("Planet stages need each other in a circle: " + stuck);
            }
        }
        return plan;
    }

    void PlanetPipeline::Run(PlanetBuild& build) const
    {
        for (const PlanetStage* stage : Plan())
        {
            if (stage->loadStep != nullptr)
            {
                BeginLoadStep(build.progress, stage->loadStep, stage->label != nullptr ? stage->label : stage->name.c_str());
            }
            stage->run(build);
        }
    }

    std::string PlanetPipeline::Fingerprint() const
    {
        std::string text;
        for (const PlanetStage* stage : Plan())
        {
            if (!text.empty())
            {
                text += ',';
            }
            text += stage->name + ':' + std::to_string(stage->version);
        }
        return text;
    }
}

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "planet/PlanetMap.h"

#include <string>
#include <vector>

namespace
{
    using namespace Infinitesia;

    PlanetStage Stage(const char* name, std::vector<std::string> needs, std::vector<std::string> provides,
                      std::vector<std::string>* log = nullptr)
    {
        std::string label = name;
        return { name, 1, std::move(needs), std::move(provides), nullptr, nullptr,
                 [log, label](PlanetBuild&) { if (log != nullptr) { log->push_back(label); } } };
    }

    std::vector<std::string> PlanNames(const PlanetPipeline& pipeline)
    {
        std::vector<std::string> names;
        for (const PlanetStage* stage : pipeline.Plan())
        {
            names.push_back(stage->name);
        }
        return names;
    }

    PlanetParameters SmallPlanet()
    {
        PlanetParameters parameters;
        parameters.latitudeBands = 45;
        parameters.longitudeSegments = 90;
        return parameters;
    }
}

TEST_CASE("DefaultPipelineKeepsTheEstablishedOrder")
{
    const PlanetPipeline pipeline = MakeDefaultPlanetPipeline();
    const std::vector<std::string> expected{
        "tectonics", "terrain", "geology", "provinces", "climate.continentality", "climate.temperature",
        "climate.precipitation", "hydrology", "soils", "biomes" };
    CHECK(PlanNames(pipeline) == expected);
    CHECK(pipeline.Fingerprint() ==
          "tectonics:1,terrain:1,geology:1,provinces:1,climate.continentality:1,climate.temperature:1,"
          "climate.precipitation:1,hydrology:1,soils:1,biomes:1");
}

TEST_CASE("StagesRunAfterWhatTheyNeedWhateverTheAddOrder")
{
    std::vector<std::string> ran;
    PlanetPipeline pipeline;
    pipeline.Add(Stage("economy", { "population" }, { "economy" }, &ran));
    pipeline.Add(Stage("population", { "biomes" }, { "population" }, &ran));
    pipeline.Add(Stage("biomes", {}, { "biomes" }, &ran));
    pipeline.Add(Stage("independent", {}, { "other" }, &ran));

    // Each time the first stage (in Add order) whose providers have run goes next.
    const std::vector<std::string> expected{ "biomes", "population", "economy", "independent" };
    const std::vector<std::string> plan = PlanNames(pipeline);
    CHECK(plan == expected);

    PlanetData data{ Planet(SmallPlanet()), {}, {}, {}, {}, {}, {} };
    PlanetBuild build(data, nullptr);
    pipeline.Run(build);
    CHECK(ran == plan);
}

TEST_CASE("InconsistentPipelinesAreRejectedBeforeRunning")
{
    std::vector<std::string> ran;

    PlanetPipeline missing;
    missing.Add(Stage("a", { "nothing" }, { "a" }, &ran));
    CHECK_THROWS_WITH_AS(missing.Plan(), doctest::Contains("'nothing'"), PipelineError);

    PlanetPipeline twice;
    twice.Add(Stage("a", {}, { "x" }, &ran));
    twice.Add(Stage("b", {}, { "x" }, &ran));
    CHECK_THROWS_WITH_AS(twice.Plan(), doctest::Contains("both provide 'x'"), PipelineError);

    PlanetPipeline sameName;
    sameName.Add(Stage("a", {}, { "x" }, &ran));
    sameName.Add(Stage("a", {}, { "y" }, &ran));
    CHECK_THROWS_AS(sameName.Plan(), PipelineError);

    PlanetPipeline circle;
    circle.Add(Stage("a", { "y" }, { "x" }, &ran));
    circle.Add(Stage("b", { "x" }, { "y" }, &ran));
    CHECK_THROWS_WITH_AS(circle.Plan(), doctest::Contains("circle"), PipelineError);

    PlanetPipeline empty;
    empty.Add({ "idle", 1, {}, {}, nullptr, nullptr, nullptr });
    CHECK_THROWS_AS(empty.Plan(), PipelineError);

    PlanetData data{ Planet(SmallPlanet()), {}, {}, {}, {}, {}, {} };
    PlanetBuild build(data, nullptr);
    CHECK_THROWS_AS(missing.Run(build), PipelineError);
    CHECK(ran.empty());
}

TEST_CASE("AStageOutsideThePlanetModuleAddsALayer")
{
    // A new domain: vegetation cover from the climate and the soils, as a layer of its own.
    PlanetPipeline pipeline = MakeDefaultPlanetPipeline();
    pipeline.Add({ "vegetation", 1, { "climate", "soils" }, { "biology.vegetation" }, nullptr, nullptr,
                   [](PlanetBuild& build)
                   {
                       Field<float>& cover = build.data.layers.Create<float>(
                           "biology.vegetation", build.LatBands(), build.LonSegs());
                       for (int la = 0; la < build.LatBands(); ++la)
                       {
                           for (int lo = 0; lo < build.LonSegs(); ++lo)
                           {
                               const PlanetRegion& cell = build.Cell(la, lo);
                               cover.At(la, lo) = cell.isOcean ? 0.0f : static_cast<float>(cell.soilFertility);
                           }
                       }
                   } });

    const PlanetMap extended(SmallPlanet(), {}, pipeline);
    const PlanetMap plain(SmallPlanet());

    const Field<float>& cover = extended.Layers().Require<float>("biology.vegetation");
    CHECK(cover.LatitudeBands() == 45);
    CHECK(cover.LongitudeSegments() == 90);
    bool someLand = false;
    for (int la = 0; la < 45; ++la)
    {
        for (int lo = 0; lo < 90; ++lo)
        {
            const PlanetRegion& cell = extended.GetRegion(la, lo);
            CHECK(cover.At(la, lo) == (cell.isOcean ? 0.0f : static_cast<float>(cell.soilFertility)));
            someLand = someLand || !cell.isOcean;
            // The extra stage leaves everything the default stages made as it was.
            const PlanetRegion& same = plain.GetRegion(la, lo);
            CHECK(cell.elevationNormalized == same.elevationNormalized);
            CHECK(cell.temperatureCelsius == same.temperatureCelsius);
            CHECK(cell.biome == same.biome);
        }
    }
    CHECK(someLand);
    CHECK(plain.Layers().Find<float>("biology.vegetation") == nullptr);
}

TEST_CASE("LayersKeepTheirTypeAndNames")
{
    PlanetLayers layers;
    layers.Create<float>("a.b", 2, 3, 1.5f);
    CHECK(layers.Contains("a.b"));
    CHECK(layers.Require<float>("a.b").At(1, 2) == 1.5f);
    CHECK_THROWS_AS(layers.Create<float>("a.b", 2, 3), std::logic_error);
    CHECK_THROWS_AS(layers.Find<int>("a.b"), std::logic_error);
    CHECK_THROWS_AS(layers.Require<float>("missing"), std::logic_error);
    CHECK(layers.Find<float>("missing") == nullptr);
}

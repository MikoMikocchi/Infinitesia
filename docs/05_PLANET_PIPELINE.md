# Planet pipeline

A `PlanetMap` is built by a `PlanetPipeline`: a list of stages, each saying what it needs and
what it provides. Adding a domain (biology, resources, population, ...) means adding stages;
`PlanetMap` does not change.

## Parts (`src/domains/natural/planetary/pipeline/`, `src/domains/natural/planetary/stages/`)

| Part | What it is |
|---|---|
| `PlanetData` | Everything a planet is: the `Planet`, climate inputs, the `PlanetRegion` grid, provinces, epochs, the water balance and the extra `PlanetLayers`. |
| `PlanetBuild` | What a stage gets: the `PlanetData` being filled, the load-progress sink, `Cell(lat, lon)`, `ReportRow`. |
| `PlanetStage` | `name`, `version`, `needs`, `provides`, load-step id and label, and `run(PlanetBuild&)`. |
| `PlanetPipeline` | `Add`, `Plan()` (validated order), `Run`, `Fingerprint()`. |
| `PlanetLayers` / `Field<T>` | Named, typed per-cell arrays for data beyond the fixed `PlanetRegion` fields. |

## Data products

`needs` and `provides` are names of data products, not of stages. The default ones:

```
tectonics -> terrain -> geology -> provinces
                     -> climate -> hydrology -> soils -> biomes
```

Exact declarations are in `stages/DefaultPipeline.cpp`. A new stage names the products it reads
("climate", "soils") and the product it makes ("biology.vegetation").

Between stages, data moves in two ways: products, which order the stages, and layers, which carry
the data itself. `geology` writes the layer `"geology.distance_to_ocean"` (`Field<double>`) and
`provinces` reads it, so `provinces` is the worked example of a layer handoff.

## Rules

- **Order.** `Plan()` runs a stage after every stage that provides something it needs; among
  ready stages the first one added goes first. The default pipeline therefore runs in the order
  it always has, and the result is bit-identical (checked against hashes of three planets when
  the pipeline was introduced).
- **Validation.** Before anything runs, `Plan()` throws `PipelineError` for a need nobody
  provides, a product with two providers, two stages with one name, a stage with nothing to run,
  or a circle.
- **Where data goes.** A stage may write the existing `PlanetRegion` fields it owns, but a new
  domain creates a layer (`build.data.layers.Create<T>("domain.name", ...)`) instead of adding
  fields to `PlanetRegion`. Readers get it from `PlanetMap::Layers()`. Layer names are
  `domain.name`.
- **Versions.** Raise a stage's `version` whenever its output changes. `Fingerprint()` lists
  names and versions in run order (`"tectonics:1,terrain:1,..."`); saves record it next to the
  seed. `kPlanetGeologyDataVersion` is the older, geology-only counterpart and stays until saves
  use the fingerprint.
- **Progress.** A stage with a `loadStep` shows a bar on the loading screen; the id is a key of
  the measured-duration profile (`PlanetLoadSteps.h`).

## Adding a stage

```cpp
PlanetPipeline pipeline = MakeDefaultPlanetPipeline();
pipeline.Add({ "vegetation", 1, { "climate", "soils" }, { "biology.vegetation" },
               LoadStep::kVegetation, "Growing vegetation",
               [](PlanetBuild& build) { /* read build.Cell(...), create a layer */ } });
const PlanetMap map(parameters, climate, pipeline, progress);
```

A domain library lives above `planet` (it may include it, not the other way round) and exports a
function that adds its stages, so the game builds one pipeline from the default stages plus the
domains it wants. `tests/PlanetPipelineTests.cpp` has a working example.

## Not done yet

- The default stages still read and write `PlanetRegion` fields directly; their reads and writes
  are declared only as coarse products. Moving individual fields into layers can happen stage by
  stage, whenever a stage is rewritten.
- No caching of stage results between runs (the fingerprint is the key to build it on).

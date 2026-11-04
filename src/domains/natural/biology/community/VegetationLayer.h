#pragma once

#include <memory>
#include <optional>

#include "domains/natural/biology/catalog/SpeciesCatalog.h"
#include "domains/natural/biology/community/VegetationSample.h"
#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/pipeline/PlanetPipeline.h"

namespace Infinitesia {
// The planet layer written by the vegetation stage: a Field<VegetationCell>.
inline constexpr const char* kVegetationLayer = "biology.vegetation";

// Adds the stage `vegetation` (needs "terrain", "climate", "soils"; provides
// "biology.vegetation") to the pipeline. The layer refers to species by their
// index in `species`, so whoever reads it must use the same catalog; raise the
// stage's version when the catalog changes what it grows.
void AddVegetationStages(PlanetPipeline& pipeline,
                         std::shared_ptr<const SpeciesCatalog> species);

// The vegetation at a direction on the sphere, blended bilinearly over the four
// nearest cells of the layer. Empty when the map was built without the stage.
std::optional<VegetationSample> SampleVegetation(const PlanetMap& map,
                                                 CartesianPoint3D direction);
}  // namespace Infinitesia

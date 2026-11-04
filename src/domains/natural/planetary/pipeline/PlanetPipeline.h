#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "domains/natural/planetary/pipeline/PlanetData.h"
#include "core/LoadProgress.h"

namespace Infinitesia {
// The pipeline is inconsistent: a stage needs something nothing provides, two
// stages provide the same thing, or the stages need each other in a circle.
class PipelineError : public std::logic_error {
 public:
  using std::logic_error::logic_error;
};

// What a stage works on: the planet being built and the progress sink of the
// load.
class PlanetBuild {
 public:
  PlanetBuild(PlanetData& data, LoadProgress* progress)
      : data(data), progress(progress) {}

  PlanetData& data;
  LoadProgress* progress;

  int LatBands() const noexcept {
    return data.planet.GetParameters().latitudeBands;
  }
  int LonSegs() const noexcept {
    return data.planet.GetParameters().longitudeSegments;
  }

  PlanetRegion& Cell(int latitude, int longitude) {
    return data.grid[static_cast<std::size_t>(latitude) *
                         static_cast<std::size_t>(LonSegs()) +
                     static_cast<std::size_t>(longitude)];
  }

  const PlanetRegion& Cell(int latitude, int longitude) const {
    return data.grid[static_cast<std::size_t>(latitude) *
                         static_cast<std::size_t>(LonSegs()) +
                     static_cast<std::size_t>(longitude)];
  }

  // Reports how many rows of the current step are done.
  void ReportRow(int row, int rowCount) const {
    if (progress != nullptr && rowCount > 0) {
      progress->SetStepFraction(static_cast<double>(row + 1) /
                                static_cast<double>(rowCount));
    }
  }
};

// One step of building a planet. A stage names what it needs (`needs`) and what
// it produces (`provides`) as data products ("terrain", "climate",
// "biology.vegetation"); the pipeline orders stages by those, so a stage does
// not have to know who runs before it. Adding a domain means adding stages,
// never editing PlanetMap.
struct PlanetStage {
  std::string name;                // "tectonics"
  int version = 1;                 // raise whenever the stage's output changes
  std::vector<std::string> needs;  // products that must exist first
  std::vector<std::string>
      provides;  // products this stage makes; each has one provider
  const char* loadStep =
      nullptr;  // key in the load-time profile (LoadStep::*), or null
  const char* label = nullptr;  // shown on the loading screen
  std::function<void(PlanetBuild&)> run;
};

class PlanetPipeline {
 public:
  // Stages with equal standing run in the order they were added.
  PlanetPipeline& Add(PlanetStage stage);

  const std::vector<PlanetStage>& Stages() const noexcept { return m_stages; }

  // The stages in the order they run: every stage after all that provide what
  // it requires, ties broken by the order of Add. Throws PipelineError.
  std::vector<const PlanetStage*> Plan() const;

  // Runs the plan on `build`. Throws PipelineError before running anything when
  // the pipeline is inconsistent.
  void Run(PlanetBuild& build) const;

  // Names and versions of the stages in run order, e.g.
  // "tectonics:1,terrain:1,...". Saved next to the seed to know what generated
  // a planet.
  std::string Fingerprint() const;

 private:
  std::vector<PlanetStage> m_stages;
};

// The stages that make the planet the game uses.
PlanetPipeline MakeDefaultPlanetPipeline();
}  // namespace Infinitesia

#include "planet/PlanetMap.h"
#include "planet/stages/PlanetStages.h"
#include "planet/internal/GenerationMath.h"
#include "planet/stages/tectonics/TectonicsInternal.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia
{
    using namespace GenerationDetail;
    using namespace TectonicsDetail;

// ==========================================================================
// Stage 1 — Tectonics
//
// Key changes from previous version:
//   • Plates use Euler-pole kinematics instead of flat lat/lon velocity.
//     Each plate rotates about a random axis; local velocity is computed via
//     v = ω × r and projected onto the lat/lon tangent frame.
//   • Boundary classification uses relative speed and normal/tangential ratio
//     instead of hard-coded 0.3 deg/My thresholds that were unreachable.
//   • crustAge for oceanic cells is derived from distance to the nearest
//     Divergent boundary (actual MOR/rift location), not distance to the
//     plate centre.
//   • Continental crust age is assigned regionally via fBm, not per-cell random.
// ==========================================================================
void RunTectonics(PlanetBuild& build)
{
    const std::uint32_t seed = build.data.planet.GetSeed();

    // -----------------------------------------------------------------------
    // Number of plates from internal heat production.
    // More massive / younger planet → more heat → more convection cells.
    // Earth (~6e24 kg, 4.5 Gyr) → 12–15 major plates.
    // -----------------------------------------------------------------------
    const double massFrac  = build.data.planet.GetParameters().massKilograms / 5.972e24;
    const double ageFrac   = std::clamp(build.data.climate.planetAgeYears / 4.5e9, 0.0, 1.0);
    // Older planets lose internal heat → fewer, larger, slower plates
    const double heatScale = std::sqrt(massFrac) * (1.0 - 0.4 * ageFrac);
    const int numPlates    = static_cast<int>(std::clamp(5.0 + heatScale * 11.0, 5.0, 16.0));


    const CrustField crust = BuildCrustField(build, seed);
    const std::vector<TectonicPlate> plates = BuildPlates(seed, numPlates, heatScale, crust);
    const std::vector<CellAssignment> assignments = AssignCellsToPlates(build, plates);

    const std::vector<bool> isDivergentBoundary =
        ClassifyBoundaries(build, seed, plates, assignments, crust);
    AssignOceanicCrustAge(build, assignments, crust, isDivergentBoundary);
    SmoothTectonicRelief(build);
    AssignAbsoluteCrustAge(build, crust);
}

} // namespace Infinitesia

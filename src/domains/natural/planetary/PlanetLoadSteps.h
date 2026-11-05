#pragma once

namespace Infinitesia::LoadStep
{
// Stable ids of the planet-loading steps, in execution order. They key the
// measured-duration profile, so renaming one only costs a single run of an
// unproportional bar. Keep in sync with the BeginLoadStep calls.
inline constexpr const char *kStarSystem = "star_system";
inline constexpr const char *kTectonics = "tectonics";
inline constexpr const char *kTerrain = "terrain";
inline constexpr const char *kGeology = "geology";
inline constexpr const char *kClimateContinentality = "climate_continentality";
inline constexpr const char *kClimateTemperature = "climate_temperature";
inline constexpr const char *kClimatePrecipitation = "climate_precipitation";
inline constexpr const char *kHydrology = "hydrology";
inline constexpr const char *kSoils = "soils";
inline constexpr const char *kBiomes = "biomes";
inline constexpr const char *kVegetation = "vegetation";
inline constexpr const char *kViewerColors = "viewer_colors";
inline constexpr const char *kViewerSmoothing = "viewer_smoothing";
} // namespace Infinitesia::LoadStep

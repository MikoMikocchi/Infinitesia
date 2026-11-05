#pragma once

namespace Infinitesia
{
// How the world is lit: the one place to tune the look by eye. The directional
// light, the fog and the colours go to the world shader as uniforms; the
// ambient occlusion and the brightness of covered ground are baked into the
// vertex colours of the terrain meshes (changing those two needs no shader
// reload, only a rebuild of the meshes, i.e. a restart).
namespace LightingStyle
{
// Direction towards the sun (the shader normalises it) and the colours of the
// light.
inline constexpr float kSunDirection[3]{-0.45f, 0.82f, 0.35f};
inline constexpr float kSunColor[3]{0.85f, 0.78f, 0.65f};       // warm
inline constexpr float kAmbientSky[3]{0.20f, 0.24f, 0.32f};     // cool, for faces turned up
inline constexpr float kAmbientGround[3]{0.09f, 0.085f, 0.08f}; // for faces turned down

// Haze with distance from the camera. The distances are multiples of the
// distance from the camera to its target, so the haze looks the same at any
// zoom: nothing nearer than the start ratio is touched, the effect is strongest
// from the end ratio on.
inline constexpr float kFogColor[3]{0.62f, 0.72f, 0.84f};
inline constexpr float kFogStartRatio = 1.1f;
inline constexpr float kFogEndRatio = 2.6f;
inline constexpr float kFogStrength = 0.40f;     // most of the colour replaced by the fog colour
inline constexpr float kFogDesaturation = 0.30f; // most of the colour drained at full fog

// Brightness of ground that no sky reaches (under a crown, a roof): dimmer than
// open ground, never black.
inline constexpr float kEnclosedBrightness = 0.55f;
// The top of a cut in such ground, which shows the section rather than a dark
// place.
inline constexpr float kEnclosedCutFaceBrightness = 0.75f;

// Ambient occlusion: the brightness of a face corner that touches 0, 1, 2 or 3
// "units" of neighbouring blocks (3 = open corner, 0 = a corner closed on both
// sides).
inline constexpr float kAmbientOcclusion[4]{0.58f, 0.72f, 0.87f, 1.0f};
} // namespace LightingStyle
} // namespace Infinitesia

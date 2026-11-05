#include <doctest/doctest.h>

#include "domains/natural/planetary/internal/PlateKinematics.h"
#include "support/Require.h"

// --------------------------------------------------------------------------
// Unit tests for ClassifyBoundaryFromVelocity sign convention.
//
// These tests anchor the geometric contract: given that the boundary normal
// points from plate A toward plate B, a relative velocity that moves A toward
// B must be Convergent and one that moves A away from B must be Divergent.
// They also verify that pure strike-slip motion is correctly classified as
// Transform regardless of the normal direction.
//
// Having explicit unit tests here means that any future refactor of the
// classification function will immediately break a specific, readable
// assertion rather than silently inverting mountains and trenches planet-wide.
// --------------------------------------------------------------------------
TEST_CASE("BoundaryClassificationSignConvention")
{
	using namespace Infinitesia::GenerationDetail;

	// -----------------------------------------------------------------------
	// Setup: plate A is west of plate B.
	// Boundary normal points east (A → B), i.e. (lat=0, lon=+1).
	// -----------------------------------------------------------------------
	constexpr double normalLat = 0.0;
	constexpr double normalLon = 1.0; // eastward unit normal (A → B)

	// --- Test 1: head-on collision (A moves east, B moves west) ---
	// A moves toward B ⟹ Convergent
	Require(ClassifyBoundaryFromVelocity(0.0, 0.1,  // A: moving east
	                                     0.0, -0.1, // B: moving west
	                                     normalLat, normalLon) == BoundaryType::Convergent,
	        "Head-on collision (A east, B west) must classify as Convergent.");

	// --- Test 2: pure divergence (A moves west, B moves east) ---
	// A moves away from B ⟹ Divergent
	Require(ClassifyBoundaryFromVelocity(0.0, -0.1, // A: moving west
	                                     0.0, 0.1,  // B: moving east
	                                     normalLat, normalLon) == BoundaryType::Divergent,
	        "Plates spreading apart (A west, B east) must classify as Divergent.");

	// --- Test 3: pure strike-slip (both moving north at same speed) ---
	// No relative motion ⟹ Transform
	Require(ClassifyBoundaryFromVelocity(0.1, 0.0, // A: moving north
	                                     0.0, 0.0, // B: stationary
	                                     normalLat, normalLon) == BoundaryType::Transform,
	        "Pure northward motion along an east-west boundary must be Transform.");

	// --- Test 4: symmetric strike-slip (A north, B south) ---
	// Motion is entirely tangential to the east-west normal ⟹ Transform
	Require(ClassifyBoundaryFromVelocity(0.1, 0.0,  // A: moving north
	                                     -0.1, 0.0, // B: moving south
	                                     normalLat, normalLon) == BoundaryType::Transform,
	        "Symmetric northward/southward strike-slip must classify as Transform.");

	// --- Test 5: oblique convergence with dominant normal component ---
	// Normal component (A toward B) > tangential ⟹ Convergent
	Require(ClassifyBoundaryFromVelocity(0.01, 0.12,  // A: mostly east, slight north
	                                     0.01, -0.03, // B: slightly west
	                                     normalLat, normalLon) == BoundaryType::Convergent,
	        "Oblique motion with dominant convergent component must be Convergent.");

	// --- Test 6: oblique divergence with dominant normal component ---
	Require(ClassifyBoundaryFromVelocity(0.01, -0.12, // A: mostly west
	                                     0.01, 0.03,  // B: slightly east
	                                     normalLat, normalLon) == BoundaryType::Divergent,
	        "Oblique motion with dominant divergent component must be Divergent.");

	// --- Test 7: below noise floor ⟹ always Transform ---
	Require(ClassifyBoundaryFromVelocity(0.0, 0.001,  // A: very slow east
	                                     0.0, -0.001, // B: very slow west
	                                     normalLat, normalLon) == BoundaryType::Transform,
	        "Sub-noise-floor relative motion must be classified as Transform.");

	// --- Test 8: northward normal (different orientation) ---
	// Normal points north (A → B); A moves north, B stationary.
	// A is advancing in the A→B direction → Convergent.
	Require(ClassifyBoundaryFromVelocity(0.1, 0.0, // A: moving north
	                                     0.0, 0.0, // B: stationary
	                                     1.0, 0.0) // normal points north
	            == BoundaryType::Convergent,
	        "Northward motion against northward A→B normal must be Convergent.");

	// --- Test 9: retreating along northward normal → Divergent ---
	// Normal points north (A → B); A moves south, B stationary.
	// A retreats from B → Divergent.
	Require(ClassifyBoundaryFromVelocity(-0.1, 0.0, // A: moving south (away from B)
	                                     0.0, 0.0,  // B: stationary
	                                     1.0, 0.0)  // normal points north
	            == BoundaryType::Divergent,
	        "Southward motion against northward A→B normal must be Divergent.");
}

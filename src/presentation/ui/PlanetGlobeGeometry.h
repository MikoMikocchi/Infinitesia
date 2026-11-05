#pragma once

#include "core/PlanetaryCoordinates.h"
#include "raylib.h"

// Placing the planet's globe: vertices on the sphere and the facing of a chosen
// place.
namespace Infinitesia
{

// A point of the globe mesh grid (row 0 is the north pole) on a sphere of
// `radius`.
Vector3 GetSphereVertex(int row, int column, int rows, int columns, float radius);
// Orientation that puts a place at the centre of the view with north up.
Matrix MakeFacingMatrix(GeographicCoordinates place);
} // namespace Infinitesia

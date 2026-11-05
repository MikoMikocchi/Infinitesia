#include "presentation/ui/PlanetGlobeGeometry.h"

#include "core/PlanetaryCoordinates.h"
#include "raymath.h"

namespace Infinitesia
{
Vector3 GetSphereVertex(int row, int column, int rows, int columns, float radius)
{
	const GeographicCoordinates coordinates{90.0 - static_cast<double>(row) / rows * 180.0,
	                                        -static_cast<double>(column) / columns * 360.0};
	const CartesianPoint3D point = PlanetaryCoordinates::ToCartesian(coordinates);
	return {static_cast<float>(point.x) * radius, static_cast<float>(point.y) * radius,
	        static_cast<float>(point.z) * radius};
}

// Orientation that puts a point at the centre of the view with north up.
// Mesh longitude runs opposite to geographic longitude, hence the -z; the
// point that faces the camera sits at world (-1, 0, 0).
Matrix MakeFacingMatrix(GeographicCoordinates place)
{
	const CartesianPoint3D unit = PlanetaryCoordinates::ToCartesian(place);
	const Vector3 toward{static_cast<float>(unit.x), static_cast<float>(unit.y), -static_cast<float>(unit.z)};

	Vector3 up = Vector3Subtract({0.0f, 1.0f, 0.0f}, Vector3Scale(toward, toward.y));
	if (Vector3Length(up) < 1e-4f)
	{
		// Looking straight down a pole: any horizontal direction will do.
		up = Vector3Subtract({0.0f, 0.0f, 1.0f}, Vector3Scale(toward, toward.z));
	}
	up = Vector3Normalize(up);
	const Vector3 side = Vector3CrossProduct(toward, up);

	Matrix facing = MatrixIdentity();
	facing.m0 = -toward.x;
	facing.m4 = -toward.y;
	facing.m8 = -toward.z;
	facing.m1 = up.x;
	facing.m5 = up.y;
	facing.m9 = up.z;
	facing.m2 = -side.x;
	facing.m6 = -side.y;
	facing.m10 = -side.z;
	return facing;
}
} // namespace Infinitesia

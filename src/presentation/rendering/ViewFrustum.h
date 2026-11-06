#pragma once

#include "raylib.h" // Vector2/Vector3/Matrix, before raymath.h defines its own
#include "raymath.h"
#include "rlgl.h"

namespace Infinitesia
{
namespace WorldGeometryRenderer
{
// The six planes of the view volume, from the projection and view the draw runs
// under. A chunk outside it cannot contribute a pixel, so it is neither drawn
// nor rebuilt.
struct Frustum
{
	float planes[6][4];

	static Frustum Current()
	{
		const Matrix m = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
		const float row[4][4] = {{m.m0, m.m4, m.m8, m.m12},
		                         {m.m1, m.m5, m.m9, m.m13},
		                         {m.m2, m.m6, m.m10, m.m14},
		                         {m.m3, m.m7, m.m11, m.m15}};
		Frustum frustum{};
		for (int axis = 0; axis < 3; ++axis)
		{
			for (int i = 0; i < 4; ++i)
			{
				frustum.planes[axis * 2][i] = row[3][i] + row[axis][i];
				frustum.planes[axis * 2 + 1][i] = row[3][i] - row[axis][i];
			}
		}
		return frustum;
	}

	bool Intersects(const Vector3 &min, const Vector3 &max) const
	{
		for (const auto &plane : planes)
		{
			const float x = plane[0] >= 0.0f ? max.x : min.x;
			const float y = plane[1] >= 0.0f ? max.y : min.y;
			const float z = plane[2] >= 0.0f ? max.z : min.z;
			if (plane[0] * x + plane[1] * y + plane[2] * z + plane[3] < 0.0f)
			{
				return false;
			}
		}
		return true;
	}
};
} // namespace WorldGeometryRenderer
} // namespace Infinitesia

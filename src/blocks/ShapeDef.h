#pragma once

#include <cstdint>
#include <string>

#include "blocks/BlockState.h"

namespace Infinitesia
{
// Sides of a cell, as bits of a face mask.
enum Face : std::uint8_t
{
	kFaceTop = 1 << 0,
	kFaceBottom = 1 << 1,
	kFaceNorth = 1 << 2,
	kFaceSouth = 1 << 3,
	kFaceEast = 1 << 4,
	kFaceWest = 1 << 5,
	kFaceAll = 0x3F,
};

// A shape: what is built of a material (assets/blocks/shapes.json).
struct ShapeDef
{
	ShapeId id = kShapeFull;
	std::string key = "shape.full";
	std::uint8_t faceMask = kFaceAll; // sides of the cell the shape covers completely
	bool walkable = true;             // a worker can stand on it
	bool passesLight = false;         // lets light through whatever the material is (a
	                                  // grating, a floor)
	std::string mesh = "cube";        // how the geometry is built; only "cube" exists
};
} // namespace Infinitesia

#pragma once

#include <array>
#include <functional>
#include <vector>

#include "presentation/rendering/terrain/BlockTextureAtlas.h"
#include "raylib.h"
#include "world/Grid.h"
#include "world/VoxelWorld.h"

namespace Infinitesia
{
namespace WorldGeometryRenderer
{
namespace TerrainMeshGeometry
{
// Which side of a block a face is: turf has a different top and side.
enum class FaceKind
{
	Top,
	Side
};

// How one face is drawn: a tile of the block atlas, multiplied by a tint (white
// for a textured face; the old flat block colour when the atlas has no such
// tile).
struct FaceLook
{
	Color tint;
	Rectangle uv; // normalised atlas coordinates
};

// Whether a cell blocks light at a corner of a face (ambient occlusion). Cells
// that are not loaded, outside the grid, or hidden by the cut count as open.
using SolidQuery = std::function<bool(GridCell)>;

// Brightness (0..1) of the four corners of a face of the block in `cell`, from
// the blocks around the corner (the classic scheme: two sides and the diagonal
// between them). `normal` is an axis; `blockCenter` is the world position of
// the middle of the block.
std::array<float, 4> ComputeCornerBrightness(const SolidQuery &isSolid, GridCell cell, Vector3 normal,
                                             Vector3 blockCenter, const std::array<Vector3, 4> &corners);

struct MeshData
{
	std::vector<float> vertices;
	std::vector<float> texcoords;
	std::vector<float> normals;
	std::vector<unsigned char> colors;
	std::vector<unsigned short> indices;

	// `cornerBrightness` multiplies the tint per corner; the quad is split along
	// the diagonal whose corners differ least, so the shading does not show a
	// seam.
	void AppendFace(const std::array<Vector3, 4> &corners, Vector3 normal, const FaceLook &look,
	                const std::array<float, 4> &cornerBrightness = {1.0f, 1.0f, 1.0f, 1.0f});
	// The side faces of the block in the cell that no solid neighbour covers.
	void AppendExposedSideFaces(const VoxelWorld &world, const SolidQuery &isSolid, GridCell cell, float left,
	                            float right, float front, float back, float bottomY, float topY, const FaceLook &look);
};

// The colour of a block: its material's colour in the registry.
Color GetBlockColor(const Block &block, const BlockRegistry &registry);

// The tile and tint of a block face. The tile is found by the material's key:
// the face's own (rock.granite_top) first, then the plain one (rock.granite). A
// material with no tile gets a flat tint of its colour. With no atlas the uv
// stays zero and the default white texture leaves just the tint.
FaceLook GetFaceLook(const Block &block, const BlockRegistry &registry, const BlockTextureAtlas *atlas, FaceKind kind);
} // namespace TerrainMeshGeometry
} // namespace WorldGeometryRenderer
} // namespace Infinitesia

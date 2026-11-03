#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "world/VoxelWorld.h"
#include "presentation/rendering/terrain/BlockTextureAtlas.h"

#include <array>
#include <vector>

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

            // How one face is drawn: a tile of the block atlas, multiplied by a tint (white for
            // a textured face; the old flat block colour when the atlas has no such tile).
            struct FaceLook
            {
                Color tint;
                Rectangle uv;   // normalised atlas coordinates
            };

            struct MeshData
            {
                std::vector<float> vertices;
                std::vector<float> texcoords;
                std::vector<float> normals;
                std::vector<unsigned char> colors;
                std::vector<unsigned short> indices;

                void AppendFace(const std::array<Vector3, 4>& corners, Vector3 normal, const FaceLook& look);
                // The side faces of the block in the cell that no solid neighbour covers.
                void AppendExposedSideFaces(
                    const VoxelWorld& world,
                    GridCell cell,
                    float left,
                    float right,
                    float front,
                    float back,
                    float bottomY,
                    float topY,
                    const FaceLook& look);
            };

            // The colour of a block: its material's colour in the registry.
            Color GetBlockColor(const Block& block, const BlockRegistry& registry);

            // The tile and tint of a block face. The tile is found by the material's key: the face's
            // own (rock.granite_top) first, then the plain one (rock.granite). A material with no tile
            // gets a flat tint of its colour. With no atlas the uv stays zero and the default white
            // texture leaves just the tint.
            FaceLook GetFaceLook(
                const Block& block,
                const BlockRegistry& registry,
                const BlockTextureAtlas* atlas,
                FaceKind kind);
        }
    }
}

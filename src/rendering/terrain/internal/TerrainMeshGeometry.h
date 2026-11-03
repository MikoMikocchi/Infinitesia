#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "world/World.h"
#include "rendering/terrain/BlockTextureAtlas.h"

#include <array>
#include <vector>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        namespace TerrainMeshGeometry
        {
            // Which side of a block a face is: grass has a different top and side.
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
                void AppendExposedSideFaces(
                    const Grid& grid,
                    const World& world,
                    GridCell cell,
                    int level,
                    float left,
                    float right,
                    float front,
                    float back,
                    float bottomY,
                    float topY,
                    const FaceLook& look);
            };

            Color GetBlockColor(BlockType type);

            // The colour of a block: the rock's own colour for a stone block whose material the
            // catalog knows, otherwise the colour of its block type.
            Color GetBlockColor(const Block& block, const GeologyCatalog* catalog);

            // The tile and tint of a block face. The tile is the material's own (rock.granite)
            // when the block has one, else that of its block type (block.grass_top, ...). With
            // no atlas the uv stays zero and the default white texture leaves just the tint.
            FaceLook GetFaceLook(
                const Block& block,
                const GeologyCatalog* catalog,
                const BlockTextureAtlas* atlas,
                FaceKind kind);
        }
    }
}

#include "presentation/rendering/terrain/internal/TerrainMeshGeometry.h"

#include "raymath.h"

#include <stdexcept>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        namespace TerrainMeshGeometry
        {
            namespace
            {
                void AppendFaceIndices(
                    const std::array<Vector3, 4>& corners,
                    Vector3 normal,
                    unsigned short firstVertex,
                    std::vector<unsigned short>& indices)
                {
                    const Vector3 firstEdge = Vector3Subtract(corners[1], corners[0]);
                    const Vector3 secondEdge = Vector3Subtract(corners[2], corners[0]);
                    const bool reverseWinding =
                        Vector3DotProduct(Vector3CrossProduct(firstEdge, secondEdge), normal) < 0.0f;
                    if (reverseWinding)
                    {
                        indices.insert(indices.end(), {
                            firstVertex,
                            static_cast<unsigned short>(firstVertex + 2),
                            static_cast<unsigned short>(firstVertex + 1),
                            firstVertex,
                            static_cast<unsigned short>(firstVertex + 3),
                            static_cast<unsigned short>(firstVertex + 2)
                        });
                        return;
                    }

                    indices.insert(indices.end(), {
                        firstVertex,
                        static_cast<unsigned short>(firstVertex + 1),
                        static_cast<unsigned short>(firstVertex + 2),
                        firstVertex,
                        static_cast<unsigned short>(firstVertex + 2),
                        static_cast<unsigned short>(firstVertex + 3)
                    });
                }
            }

            Color GetBlockColor(BlockType type)
            {
                switch (type)
                {
                case BlockType::Grass:
                    return { 82, 145, 66, 255 };
                case BlockType::Dirt:
                    return { 139, 94, 60, 255 };
                case BlockType::Stone:
                    return { 132, 137, 142, 255 };
                case BlockType::Sand:
                    return { 219, 203, 140, 255 };
                case BlockType::Water:
                    return { 52, 110, 170, 255 };
                case BlockType::Air:
                    break;
                }

                throw std::invalid_argument("Block has an unsupported type.");
            }

            Color GetBlockColor(const Block& block, const GeologyCatalog* catalog)
            {
                if (catalog != nullptr && block.material != kUnspecifiedMaterial)
                {
                    const MaterialDef* material = catalog->GetMaterial(block.material);
                    if (material != nullptr && material->blockClass == BlockClass::Rock)
                    {
                        return { material->color.r, material->color.g, material->color.b, 255 };
                    }
                }
                return GetBlockColor(block.type);
            }

            FaceLook GetFaceLook(
                const Block& block,
                const GeologyCatalog* catalog,
                const BlockTextureAtlas* atlas,
                FaceKind kind)
            {
                if (atlas != nullptr && atlas->IsLoaded())
                {
                    const Rectangle* tile = nullptr;
                    if (catalog != nullptr && block.material != kUnspecifiedMaterial)
                    {
                        const MaterialDef* material = catalog->GetMaterial(block.material);
                        if (material != nullptr)
                        {
                            tile = atlas->Find(material->key);
                        }
                    }
                    if (tile == nullptr)
                    {
                        switch (block.type)
                        {
                        case BlockType::Grass:
                            tile = atlas->Find(kind == FaceKind::Top ? "block.grass_top" : "block.grass_side");
                            break;
                        case BlockType::Dirt:  tile = atlas->Find("block.dirt");  break;
                        case BlockType::Stone: tile = atlas->Find("block.stone"); break;
                        case BlockType::Sand:  tile = atlas->Find("block.sand");  break;
                        case BlockType::Water: tile = atlas->Find("block.water"); break;
                        case BlockType::Air:   break;
                        }
                    }
                    if (tile != nullptr)
                    {
                        return { WHITE, *tile };
                    }
                    return { GetBlockColor(block, catalog), atlas->WhiteTile() };
                }
                return { GetBlockColor(block, catalog), Rectangle{} };
            }

            void MeshData::AppendFace(
                const std::array<Vector3, 4>& corners,
                Vector3 normal,
                const FaceLook& look)
            {
                const unsigned short firstVertex =
                    static_cast<unsigned short>(vertices.size() / 3);
                const float u0 = look.uv.x;
                const float u1 = look.uv.x + look.uv.width;
                const float v0 = look.uv.y;
                const float v1 = look.uv.y + look.uv.height;
                // Top faces list their corners front-left, front-right, back-right, back-left;
                // side faces list the two bottom corners and then the two top ones, so the
                // picture's top edge lands on the upper corners.
                const std::array<float, 8> uv = normal.y != 0.0f
                    ? std::array<float, 8>{ u0, v0, u1, v0, u1, v1, u0, v1 }
                    : std::array<float, 8>{ u0, v1, u1, v1, u1, v0, u0, v0 };
                std::size_t cornerIndex = 0;
                for (const Vector3 corner : corners)
                {
                    vertices.insert(vertices.end(), { corner.x, corner.y, corner.z });
                    texcoords.insert(texcoords.end(), { uv[cornerIndex * 2], uv[cornerIndex * 2 + 1] });
                    normals.insert(normals.end(), { normal.x, normal.y, normal.z });
                    colors.insert(colors.end(), { look.tint.r, look.tint.g, look.tint.b, look.tint.a });
                    ++cornerIndex;
                }

                AppendFaceIndices(corners, normal, firstVertex, indices);
            }

            void MeshData::AppendExposedSideFaces(
                const Grid& grid,
                const VoxelWorld& world,
                GridCell cell,
                int level,
                float left,
                float right,
                float front,
                float back,
                float bottomY,
                float topY,
                const FaceLook& look)
            {
                const std::array<GridCell, 4> neighbors{
                    GridCell{ cell.x - 1, level, cell.z },
                    GridCell{ cell.x + 1, level, cell.z },
                    GridCell{ cell.x, level, cell.z - 1 },
                    GridCell{ cell.x, level, cell.z + 1 }
                };
                const std::array<std::array<Vector3, 4>, 4> sideCorners{
                    std::array<Vector3, 4>{
                        Vector3{ left, bottomY, front },
                        Vector3{ left, bottomY, back },
                        Vector3{ left, topY, back },
                        Vector3{ left, topY, front }
                    },
                    std::array<Vector3, 4>{
                        Vector3{ right, bottomY, back },
                        Vector3{ right, bottomY, front },
                        Vector3{ right, topY, front },
                        Vector3{ right, topY, back }
                    },
                    std::array<Vector3, 4>{
                        Vector3{ right, bottomY, front },
                        Vector3{ left, bottomY, front },
                        Vector3{ left, topY, front },
                        Vector3{ right, topY, front }
                    },
                    std::array<Vector3, 4>{
                        Vector3{ left, bottomY, back },
                        Vector3{ right, bottomY, back },
                        Vector3{ right, topY, back },
                        Vector3{ left, topY, back }
                    }
                };
                const std::array<Vector3, 4> sideNormals{
                    Vector3{ -1.0f, 0.0f, 0.0f },
                    Vector3{ 1.0f, 0.0f, 0.0f },
                    Vector3{ 0.0f, 0.0f, -1.0f },
                    Vector3{ 0.0f, 0.0f, 1.0f }
                };

                for (std::size_t side = 0; side < neighbors.size(); ++side)
                {
                    if (grid.Contains(neighbors[side]))
                    {
                        const Block* neighbor = world.GetBlock(neighbors[side]);
                        if (neighbor == nullptr || neighbor->type != BlockType::Air)
                        {
                            continue;
                        }
                    }

                    AppendFace(sideCorners[side], sideNormals[side], look);
                }
            }
        }
    }
}

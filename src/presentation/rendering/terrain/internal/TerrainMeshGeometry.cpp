#include "presentation/rendering/terrain/internal/TerrainMeshGeometry.h"

#include "raymath.h"

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

            Color GetBlockColor(const Block& block, const BlockRegistry& registry)
            {
                const RgbColor color = registry.ColorOf(block.state.material);
                return { color.r, color.g, color.b, 255 };
            }

            FaceLook GetFaceLook(
                const Block& block,
                const BlockRegistry& registry,
                const BlockTextureAtlas* atlas,
                FaceKind kind)
            {
                if (atlas != nullptr && atlas->IsLoaded())
                {
                    const Rectangle* tile = nullptr;
                    const MaterialDef* material = registry.GetMaterial(block.state.material);
                    if (material != nullptr)
                    {
                        tile = atlas->Find(material->key + (kind == FaceKind::Top ? "_top" : "_side"));
                        if (tile == nullptr)
                        {
                            tile = atlas->Find(material->key);
                        }
                    }
                    if (tile != nullptr)
                    {
                        return { WHITE, *tile };
                    }
                    return { GetBlockColor(block, registry), atlas->WhiteTile() };
                }
                return { GetBlockColor(block, registry), Rectangle{} };
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
                        if (neighbor == nullptr || world.Registry().Occupied(neighbor->state))
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

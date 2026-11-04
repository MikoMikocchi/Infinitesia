#include "presentation/rendering/terrain/internal/TerrainMeshGeometry.h"

#include "presentation/rendering/LightingStyle.h"
#include "raymath.h"

namespace Infinitesia {
namespace WorldGeometryRenderer {
namespace TerrainMeshGeometry {
namespace {
void AppendFaceIndices(const std::array<Vector3, 4>& corners, Vector3 normal,
                       unsigned short firstVertex, bool splitAlongOddDiagonal,
                       std::vector<unsigned short>& indices) {
  const Vector3 firstEdge = Vector3Subtract(corners[1], corners[0]);
  const Vector3 secondEdge = Vector3Subtract(corners[2], corners[0]);
  const bool reverseWinding =
      Vector3DotProduct(Vector3CrossProduct(firstEdge, secondEdge), normal) <
      0.0f;
  // Starting the fan one corner further splits the quad along the other
  // diagonal (corner 1 to corner 3) with the same winding.
  const unsigned short rotation = splitAlongOddDiagonal ? 1 : 0;
  const auto vertex = [&](unsigned short corner) {
    return static_cast<unsigned short>(firstVertex + (corner + rotation) % 4);
  };
  if (reverseWinding) {
    indices.insert(indices.end(), {vertex(0), vertex(2), vertex(1), vertex(0),
                                   vertex(3), vertex(2)});
    return;
  }

  indices.insert(indices.end(), {vertex(0), vertex(1), vertex(2), vertex(0),
                                 vertex(2), vertex(3)});
}

float AxisOf(const Vector3& vector, int axis) {
  return axis == 0 ? vector.x : (axis == 1 ? vector.y : vector.z);
}

// Open corner 3, a corner closed on both sides 0 (the classic 0fps scheme).
int OcclusionLevel(bool side1, bool side2, bool diagonal) {
  if (side1 && side2) {
    return 0;
  }
  return 3 - (static_cast<int>(side1) + static_cast<int>(side2) +
              static_cast<int>(diagonal));
}
}  // namespace

std::array<float, 4> ComputeCornerBrightness(
    const SolidQuery& isSolid, GridCell cell, Vector3 normal,
    Vector3 blockCenter, const std::array<Vector3, 4>& corners) {
  int normalAxis = 0;
  for (int axis = 0; axis < 3; ++axis) {
    if (AxisOf(normal, axis) != 0.0f) {
      normalAxis = axis;
    }
  }
  const int normalSign = AxisOf(normal, normalAxis) > 0.0f ? 1 : -1;
  const int firstAxis = normalAxis == 0 ? 1 : 0;
  const int secondAxis = normalAxis == 2 ? 1 : 2;

  const auto offset = [&](int axis0, int step0, int axis1, int step1, int axis2,
                          int step2) {
    int delta[3]{0, 0, 0};
    delta[axis0] += step0;
    delta[axis1] += step1;
    delta[axis2] += step2;
    return GridCell{cell.x + delta[0], cell.y + delta[1], cell.z + delta[2]};
  };

  std::array<float, 4> brightness{};
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    const int firstSign =
        AxisOf(corners[corner], firstAxis) > AxisOf(blockCenter, firstAxis)
            ? 1
            : -1;
    const int secondSign =
        AxisOf(corners[corner], secondAxis) > AxisOf(blockCenter, secondAxis)
            ? 1
            : -1;
    const bool side1 = isSolid(
        offset(normalAxis, normalSign, firstAxis, firstSign, normalAxis, 0));
    const bool side2 = isSolid(
        offset(normalAxis, normalSign, secondAxis, secondSign, normalAxis, 0));
    const bool diagonal = isSolid(offset(normalAxis, normalSign, firstAxis,
                                         firstSign, secondAxis, secondSign));
    brightness[corner] = LightingStyle::kAmbientOcclusion[OcclusionLevel(
        side1, side2, diagonal)];
  }
  return brightness;
}

Color GetBlockColor(const Block& block, const BlockRegistry& registry) {
  const RgbColor color = registry.ColorOf(block.state.material);
  return {color.r, color.g, color.b, 255};
}

FaceLook GetFaceLook(const Block& block, const BlockRegistry& registry,
                     const BlockTextureAtlas* atlas, FaceKind kind) {
  if (atlas != nullptr && atlas->IsLoaded()) {
    const Rectangle* tile = nullptr;
    const MaterialDef* material = registry.GetMaterial(block.state.material);
    if (material != nullptr) {
      tile = atlas->Find(material->key +
                         (kind == FaceKind::Top ? "_top" : "_side"));
      if (tile == nullptr) {
        tile = atlas->Find(material->key);
      }
    }
    if (tile != nullptr) {
      return {WHITE, *tile};
    }
    return {GetBlockColor(block, registry), atlas->WhiteTile()};
  }
  return {GetBlockColor(block, registry), Rectangle{}};
}

void MeshData::AppendFace(const std::array<Vector3, 4>& corners, Vector3 normal,
                          const FaceLook& look,
                          const std::array<float, 4>& cornerBrightness) {
  const unsigned short firstVertex =
      static_cast<unsigned short>(vertices.size() / 3);
  const float u0 = look.uv.x;
  const float u1 = look.uv.x + look.uv.width;
  const float v0 = look.uv.y;
  const float v1 = look.uv.y + look.uv.height;
  // Top faces list their corners front-left, front-right, back-right,
  // back-left; side faces list the two bottom corners and then the two top
  // ones, so the picture's top edge lands on the upper corners.
  const std::array<float, 8> uv =
      normal.y != 0.0f ? std::array<float, 8>{u0, v0, u1, v0, u1, v1, u0, v1}
                       : std::array<float, 8>{u0, v1, u1, v1, u1, v0, u0, v0};
  std::size_t cornerIndex = 0;
  for (const Vector3 corner : corners) {
    vertices.insert(vertices.end(), {corner.x, corner.y, corner.z});
    texcoords.insert(texcoords.end(),
                     {uv[cornerIndex * 2], uv[cornerIndex * 2 + 1]});
    normals.insert(normals.end(), {normal.x, normal.y, normal.z});
    const float factor = cornerBrightness[cornerIndex];
    colors.insert(colors.end(),
                  {static_cast<unsigned char>(
                       static_cast<float>(look.tint.r) * factor + 0.5f),
                   static_cast<unsigned char>(
                       static_cast<float>(look.tint.g) * factor + 0.5f),
                   static_cast<unsigned char>(
                       static_cast<float>(look.tint.b) * factor + 0.5f),
                   look.tint.a});
    ++cornerIndex;
  }

  // Split along the diagonal of the brighter pair, so a dark corner stays in
  // one triangle.
  const bool splitAlongOddDiagonal = cornerBrightness[1] + cornerBrightness[3] >
                                     cornerBrightness[0] + cornerBrightness[2];
  AppendFaceIndices(corners, normal, firstVertex, splitAlongOddDiagonal,
                    indices);
}

void MeshData::AppendExposedSideFaces(const VoxelWorld& world,
                                      const SolidQuery& isSolid, GridCell cell,
                                      float left, float right, float front,
                                      float back, float bottomY, float topY,
                                      const FaceLook& look) {
  const std::array<GridCell, 4> neighbors{GridCell{cell.x - 1, cell.y, cell.z},
                                          GridCell{cell.x + 1, cell.y, cell.z},
                                          GridCell{cell.x, cell.y, cell.z - 1},
                                          GridCell{cell.x, cell.y, cell.z + 1}};
  const std::array<std::array<Vector3, 4>, 4> sideCorners{
      std::array<Vector3, 4>{
          Vector3{left, bottomY, front}, Vector3{left, bottomY, back},
          Vector3{left, topY, back}, Vector3{left, topY, front}},
      std::array<Vector3, 4>{
          Vector3{right, bottomY, back}, Vector3{right, bottomY, front},
          Vector3{right, topY, front}, Vector3{right, topY, back}},
      std::array<Vector3, 4>{
          Vector3{right, bottomY, front}, Vector3{left, bottomY, front},
          Vector3{left, topY, front}, Vector3{right, topY, front}},
      std::array<Vector3, 4>{
          Vector3{left, bottomY, back}, Vector3{right, bottomY, back},
          Vector3{right, topY, back}, Vector3{left, topY, back}}};
  const std::array<Vector3, 4> sideNormals{
      Vector3{-1.0f, 0.0f, 0.0f}, Vector3{1.0f, 0.0f, 0.0f},
      Vector3{0.0f, 0.0f, -1.0f}, Vector3{0.0f, 0.0f, 1.0f}};

  for (std::size_t side = 0; side < neighbors.size(); ++side) {
    // A neighbour that is not loaded yet counts as open: a face hidden behind
    // it later costs nothing, a missing one would leave a hole in the ground.
    const Block* neighbor = world.GetBlock(neighbors[side]);
    if (neighbor != nullptr && world.Registry().Occupied(neighbor->state)) {
      continue;
    }

    const Vector3 blockCenter{(left + right) * 0.5f, (bottomY + topY) * 0.5f,
                              (front + back) * 0.5f};
    AppendFace(sideCorners[side], sideNormals[side], look,
               ComputeCornerBrightness(isSolid, cell, sideNormals[side],
                                       blockCenter, sideCorners[side]));
  }
}
}  // namespace TerrainMeshGeometry
}  // namespace WorldGeometryRenderer
}  // namespace Infinitesia

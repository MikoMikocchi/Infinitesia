#include "blocks/BlockLoaders.h"

#include <set>

#include "matter/CatalogJson.h"

namespace Infinitesia {
using namespace CatalogJson;

namespace {
std::uint8_t FacesField(const Json& entry, const Where& where) {
  const Json& faces = Field(entry, "faces", where);
  if (faces.is_string() && faces.get<std::string>() == "all") return kFaceAll;
  if (!faces.is_array())
    Fail(where, "field 'faces' must be \"all\" or an array of face names");

  static constexpr std::pair<const char*, std::uint8_t> kNames[] = {
      {"top", kFaceTop},     {"bottom", kFaceBottom}, {"north", kFaceNorth},
      {"south", kFaceSouth}, {"east", kFaceEast},     {"west", kFaceWest},
  };
  std::uint8_t mask = 0;
  for (const Json& face : faces) {
    bool known = false;
    for (const auto& [name, bit] : kNames) {
      if (face.is_string() && face.get<std::string>() == name) {
        mask = static_cast<std::uint8_t>(mask | bit);
        known = true;
      }
    }
    if (!known)
      Fail(where,
           "unknown face in 'faces' (top, bottom, north, south, east, west)");
  }
  return mask;
}
}  // namespace

void LoadShapes(const std::filesystem::path& path,
                std::vector<ShapeDef>& shapes) {
  const Json root = ReadJson(path);
  const Where where{path.string(), ""};
  std::size_t index = 0;
  for (const Json& entry : ArrayField(root, "shapes", where)) {
    Where record{path.string(), "shapes[" + std::to_string(index++) + "]"};
    ShapeDef shape;
    shape.key = StringField(entry, "id", record);
    record.record = shape.key;
    if (!StartsWith(shape.key, "shape."))
      Fail(record, "id must start with 'shape.'");
    const int numericId = IntField(entry, "numericId", record);
    if (numericId < 0 || numericId >= kShapeCapacity)
      Fail(record, "numericId must be in 0-255");
    shape.id = static_cast<ShapeId>(numericId);
    shape.faceMask = FacesField(entry, record);
    shape.walkable = BoolField(entry, "walkable", record, true);
    shape.passesLight = BoolField(entry, "passesLight", record, false);
    shape.mesh = StringField(entry, "mesh", record);
    if (shape.mesh != "cube")
      Fail(record, "mesh must be 'cube' (the only geometry that exists)");
    shapes.push_back(std::move(shape));
  }
}
}  // namespace Infinitesia

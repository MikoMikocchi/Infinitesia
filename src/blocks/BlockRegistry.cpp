#include "blocks/BlockRegistry.h"

#include <algorithm>
#include <set>

#include "blocks/BlockLoaders.h"

namespace Infinitesia {
namespace {
MatterDef MakeBuiltInMaterial(MatterId id, const char* key,
                                const char* name, BlockClass blockClass,
                                double density, int digClass, RgbColor color,
                                std::vector<std::string> extraTags = {}) {
  MatterDef material;
  material.id = id;
  material.key = key;
  material.name = name;
  material.blockClass = blockClass;
  material.density = density;
  material.digClass = digClass;
  material.color = color;
  material.tags.push_back(ClassTag(blockClass));
  for (std::string& tag : extraTags) material.tags.push_back(std::move(tag));
  material.opacity = DefaultOpacity(blockClass);
  material.walkable = material.diggable = blockClass != BlockClass::Air;
  return material;
}
}  // namespace

BlockRegistry BlockRegistry::Build(
    std::shared_ptr<const MatterCatalog> materials,
    const std::filesystem::path& shapesFile) {
  std::vector<ShapeDef> shapes;
  LoadShapes(shapesFile, shapes);
  return Assemble(std::move(materials), std::move(shapes));
}

BlockRegistry BlockRegistry::BuiltIn() {
  std::vector<MatterDef> materials;
  materials.push_back(MakeBuiltInMaterial(kAirMatter, "air", "air",
                                          BlockClass::Air, 1.2, 0, {0, 0, 0}));
  materials.push_back(MakeBuiltInMaterial(kWaterMatter, "fluid.water",
                                          "water", BlockClass::Fluid, 1000.0, 0,
                                          {0x34, 0x6e, 0xaa}, {"water"}));
  materials.push_back(MakeBuiltInMaterial(2, "fluid.seawater", "seawater",
                                          BlockClass::Fluid, 1000.0, 0,
                                          {0x34, 0x6e, 0xaa}, {"water"}));
  materials.push_back(MakeBuiltInMaterial(130, "rock.bedrock", "bedrock",
                                          BlockClass::Rock, 2700.0, 4,
                                          {0x84, 0x89, 0x8e}));
  materials.push_back(MakeBuiltInMaterial(513, "soil.subsoil", "subsoil",
                                          BlockClass::Loose, 1600.0, 1,
                                          {0x7a, 0x5a, 0x3a}, {"soil"}));
  materials.push_back(MakeBuiltInMaterial(516, "soil.sand", "sand",
                                          BlockClass::Loose, 1600.0, 1,
                                          {0xdb, 0xcb, 0x8c}, {"soil"}));
  materials.push_back(MakeBuiltInMaterial(527, "soil.turf", "turf",
                                          BlockClass::Loose, 1300.0, 1,
                                          {0x52, 0x91, 0x42}, {"soil"}));
  auto catalog = std::make_shared<const MatterCatalog>(
      MatterCatalog::Assemble(std::move(materials), {}, {}));
  return Assemble(std::move(catalog), {ShapeDef{}});
}

BlockRegistry BlockRegistry::Assemble(
    std::shared_ptr<const MatterCatalog> materials,
    std::vector<ShapeDef> shapes) {
  BlockRegistry registry;
  registry.m_catalog = std::move(materials);
  registry.m_props.assign(kMatterCapacity, MaterialProps{});

  for (const MatterDef& material : registry.m_catalog->Matter()) {
    MaterialProps& props = registry.m_props[material.id];
    const bool occupied = material.blockClass != BlockClass::Air;
    props.flags = static_cast<std::uint8_t>(
        (occupied ? kOccupied : 0) |
        (occupied && material.walkable ? kWalkable : 0) |
        (occupied && material.diggable ? kDiggable : 0));
    props.digClass = static_cast<std::uint8_t>(material.digClass);
    props.opacity = material.opacity;
    props.emission = material.emission;
    for (const std::string& name : material.tags) {
      if (const std::optional<Tag> tag = TagFromName(name))
        props.tags |= 1u << static_cast<unsigned>(*tag);
    }
    if (material.emission != LightRgb{})
      props.tags |= 1u << static_cast<unsigned>(Tag::LightSource);
  }

  std::sort(shapes.begin(), shapes.end(),
            [](const ShapeDef& a, const ShapeDef& b) { return a.id < b.id; });
  registry.m_shapes = std::move(shapes);
  std::set<std::string> shapeKeys;
  for (std::size_t i = 0; i < registry.m_shapes.size(); ++i) {
    const ShapeDef& shape = registry.m_shapes[i];
    if (registry.m_shapeKnown[shape.id])
      throw CatalogError("blocks: shape numeric id " +
                         std::to_string(shape.id) + " is used twice");
    if (!shapeKeys.insert(shape.key).second)
      throw CatalogError("blocks: duplicate shape id '" + shape.key + "'");
    registry.m_shapeKnown[shape.id] = true;
    registry.m_shapeProps[shape.id] = {shape.faceMask, shape.walkable,
                                       shape.passesLight};
    registry.m_shapeByKey.emplace(shape.key, static_cast<int>(i));
  }
  if (!registry.m_shapeKnown[kShapeFull] ||
      registry.m_shapes.front().key != "shape.full")
    throw CatalogError("blocks: shape 0 must be shape.full");
  return registry;
}

const ShapeDef* BlockRegistry::FindShape(std::string_view key) const noexcept {
  const auto found = m_shapeByKey.find(std::string(key));
  return found == m_shapeByKey.end()
             ? nullptr
             : &m_shapes[static_cast<std::size_t>(found->second)];
}

const ShapeDef* BlockRegistry::GetShape(ShapeId id) const noexcept {
  for (const ShapeDef& shape : m_shapes) {
    if (shape.id == id) return &shape;
  }
  return nullptr;
}

bool BlockRegistry::HasTag(MatterId material,
                           std::string_view tag) const noexcept {
  const MatterDef* def = GetMatter(material);
  return def != nullptr &&
         std::find(def->tags.begin(), def->tags.end(), tag) != def->tags.end();
}
}  // namespace Infinitesia

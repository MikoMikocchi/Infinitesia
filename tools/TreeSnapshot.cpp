// TreeSnapshot: a contact sheet of the plants the game would grow, written as a
// PNG without opening a window. One row per species, one column per seed, all
// at the same scale, so shapes and sizes can be compared and tuned in
// assets/biology/species.json without starting the game.
//
// Every cell is drawn as an isometric cube in the colour of the material the
// species gives its part (assets/materials/materials.json). Roots (below the
// ground) are darkened.
//
//   TreeSnapshot                                    every species, 6 seeds,
//   trees.png TreeSnapshot --species oak --seeds 8            one species (the
//   key may omit "species.") TreeSnapshot --species oak --species spruce
//   several species TreeSnapshot --first-seed 100 --seeds 4 --scale 10 --view 1
//   --out sheet.png
//
// See --help for all options.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "domains/natural/biology/catalog/SpeciesCatalog.h"
#include "domains/natural/biology/flora/Tree.h"
#include "materials/MaterialCatalog.h"
#include "terrain_snapshot/Image.h"

using namespace Infinitesia;
using SnapshotTool::Image;
using SnapshotTool::Point;
using SnapshotTool::Rgb;

namespace {
constexpr double kHalfCubeWidth = 0.8660254037844386;  // cos 30 degrees

struct Options {
  std::vector<std::string> species;
  int seeds = 6;
  std::uint64_t firstSeed = 1;
  double scale = 6.0;  // pixels per cell edge
  int view = 0;        // quarter turns of the plant around its axis
  std::string out = "trees.png";
};

void PrintUsage() {
  std::printf(
      "TreeSnapshot: a contact sheet of generated plants (row per species, "
      "column per seed)\n\n"
      "  --species <key>    species to draw, repeatable; \"oak\" means "
      "species.oak (default: all)\n"
      "  --seeds <n>        plants per species (default 6)\n"
      "  --first-seed <n>   seed of the first column (default 1)\n"
      "  --scale <px>       pixels per cell edge (default 6)\n"
      "  --view <0-3>       quarter turns around the trunk (default 0)\n"
      "  --out <file.png>   output (default trees.png)\n"
      "  --help\n");
}

bool ParseOptions(int argc, char** argv, Options& options) {
  for (int i = 1; i < argc; ++i) {
    const std::string flag = argv[i];
    const auto value = [&]() -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "%s needs a value\n", flag.c_str());
        std::exit(2);
      }
      return argv[++i];
    };
    if (flag == "--help" || flag == "-h")
      return false;
    else if (flag == "--species")
      options.species.emplace_back(value());
    else if (flag == "--seeds")
      options.seeds = std::atoi(value());
    else if (flag == "--first-seed")
      options.firstSeed = std::strtoull(value(), nullptr, 10);
    else if (flag == "--scale")
      options.scale = std::atof(value());
    else if (flag == "--view")
      options.view = std::atoi(value());
    else if (flag == "--out")
      options.out = value();
    else {
      std::fprintf(stderr, "unknown option %s\n", flag.c_str());
      std::exit(2);
    }
  }
  if (options.seeds < 1 || options.seeds > 32 || options.scale < 1.0 ||
      options.scale > 40.0 || options.view < 0 || options.view > 3) {
    std::fprintf(stderr, "--seeds must be 1-32, --scale 1-40, --view 0-3\n");
    std::exit(2);
  }
  return true;
}

// Quarter turns of the horizontal plane.
void Turn(int quarter, int& x, int& z) {
  for (int i = 0; i < quarter; ++i) {
    const int turned = -z;
    z = x;
    x = turned;
  }
}

// Screen position of a point of the plant (in cells, y up) relative to its
// base.
Point Project(double x, double y, double z, double scale) {
  return {(x - z) * kHalfCubeWidth * scale, (x + z) * 0.5 * scale - y * scale};
}

struct Placed {
  int x, y, z;
  TreePart part;
};

std::vector<Placed> Orient(const TreeVoxels& tree, int view) {
  std::vector<Placed> placed;
  placed.reserve(tree.cells.size());
  for (const TreeVoxel& voxel : tree.cells) {
    int x = voxel.x;
    int z = voxel.z;
    Turn(view, x, z);
    placed.push_back({x, voxel.y, z, voxel.part});
  }
  // Far to near: the cell nearest the viewer (large x + z, high y) is drawn
  // last.
  std::sort(placed.begin(), placed.end(), [](const Placed& a, const Placed& b) {
    if (a.x + a.z != b.x + b.z) return a.x + a.z < b.x + b.z;
    if (a.y != b.y) return a.y < b.y;
    return a.x < b.x;
  });
  return placed;
}

struct Extent {
  double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0;
  void Add(Point p) {
    minX = std::min(minX, p.x);
    maxX = std::max(maxX, p.x);
    minY = std::min(minY, p.y);
    maxY = std::max(maxY, p.y);
  }
};

void Grow(Extent& extent, const std::vector<Placed>& cells, double scale) {
  for (const Placed& cell : cells) {
    for (const double dx : {-0.5, 0.5}) {
      for (const double dy : {-0.5, 0.5}) {
        for (const double dz : {-0.5, 0.5})
          extent.Add(Project(cell.x + dx, cell.y + dy, cell.z + dz, scale));
      }
    }
  }
}

void DrawCell(Image& image, const Placed& cell, Rgb color, Point origin,
              double scale) {
  if (cell.part == TreePart::Root) color = SnapshotTool::Shade(color, 0.6);
  const auto at = [&](double dx, double dy, double dz) {
    const Point p = Project(cell.x + dx, cell.y + dy, cell.z + dz, scale);
    return Point{origin.x + p.x, origin.y + p.y};
  };
  image.FillQuad({at(-0.5, 0.5, -0.5), at(0.5, 0.5, -0.5), at(0.5, 0.5, 0.5),
                  at(-0.5, 0.5, 0.5)},
                 color);
  image.FillQuad({at(-0.5, 0.5, 0.5), at(0.5, 0.5, 0.5), at(0.5, -0.5, 0.5),
                  at(-0.5, -0.5, 0.5)},
                 SnapshotTool::Shade(color, 0.72));
  image.FillQuad({at(0.5, 0.5, 0.5), at(0.5, 0.5, -0.5), at(0.5, -0.5, -0.5),
                  at(0.5, -0.5, 0.5)},
                 SnapshotTool::Shade(color, 0.86));
}

std::vector<const Species*> ChooseSpecies(
    const SpeciesCatalog& catalog, const std::vector<std::string>& wanted) {
  std::vector<const Species*> chosen;
  if (wanted.empty()) {
    for (const Species& species : catalog.All()) chosen.push_back(&species);
    return chosen;
  }
  for (const std::string& name : wanted) {
    const Species* species = catalog.Find(name);
    if (species == nullptr) species = catalog.Find("species." + name);
    if (species == nullptr) {
      std::fprintf(stderr, "unknown species '%s'\n", name.c_str());
      std::exit(2);
    }
    chosen.push_back(species);
  }
  return chosen;
}

int Run(const Options& options) {
  const std::string assets = INFINITESIA_ASSET_DIR;
  const MaterialCatalog materials =
      MaterialCatalog::LoadFromFile(assets + "/materials/materials.json");
  const SpeciesCatalog catalog =
      SpeciesCatalog::Load(assets + "/biology/species.json", materials);
  const std::vector<const Species*> species =
      ChooseSpecies(catalog, options.species);

  // Grow everything first: the tiles share one size so the sheet is to scale.
  std::vector<std::vector<std::vector<Placed>>> sheet;
  Extent extent;
  for (const Species* kind : species) {
    sheet.emplace_back();
    std::printf("%s\n", kind->key.c_str());
    for (int column = 0; column < options.seeds; ++column) {
      const std::uint64_t seed =
          options.firstSeed + static_cast<std::uint64_t>(column);
      const TreeVoxels tree = GrowTree(*kind, seed);
      int wood = 0;
      for (const TreeVoxel& voxel : tree.cells)
        wood += IsWood(voxel.part) ? 1 : 0;
      std::printf(
          "  seed %-6llu %5zu cells (%d wood), %d..%d high, %d x %d wide\n",
          static_cast<unsigned long long>(seed), tree.cells.size(), wood,
          tree.minY, tree.maxY, tree.maxX - tree.minX + 1,
          tree.maxZ - tree.minZ + 1);
      sheet.back().push_back(Orient(tree, options.view));
      Grow(extent, sheet.back().back(), options.scale);
    }
  }

  constexpr int kPadding = 12;
  const int tileWidth =
      static_cast<int>(std::ceil(extent.maxX - extent.minX)) + 2 * kPadding;
  const int tileHeight =
      static_cast<int>(std::ceil(extent.maxY - extent.minY)) + 2 * kPadding;
  Image image(tileWidth * options.seeds,
              tileHeight * static_cast<int>(species.size()), {214, 226, 236});

  for (std::size_t row = 0; row < species.size(); ++row) {
    for (int column = 0; column < options.seeds; ++column) {
      const Point origin{
          column * tileWidth + kPadding - extent.minX,
          static_cast<double>(row) * tileHeight + kPadding - extent.minY};
      const PlantMaterials& parts = species[row]->materials;
      for (const Placed& cell : sheet[row][static_cast<std::size_t>(column)]) {
        const MaterialDef* material =
            materials.Get(PartMaterial(parts, cell.part));
        const Rgb color =
            material != nullptr
                ? Rgb{material->color.r, material->color.g, material->color.b}
                : Rgb{255, 0, 255};
        DrawCell(image, cell, color, origin, options.scale);
      }
    }
  }

  if (!SnapshotTool::WritePng(options.out, image)) {
    std::fprintf(stderr, "cannot write %s\n", options.out.c_str());
    return 1;
  }
  std::printf("wrote %s (%d x %d)\n", options.out.c_str(), image.Width(),
              image.Height());
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  try {
    Options options;
    if (!ParseOptions(argc, argv, options)) {
      PrintUsage();
      return 0;
    }
    return Run(options);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "Error: %s\n", error.what());
    return 1;
  }
}

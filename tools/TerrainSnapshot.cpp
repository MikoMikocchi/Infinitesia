// TerrainSnapshot: an isometric "block diagram" of the terrain the game would build,
// written as a PNG without opening a window.
//
// It shows the ground from above together with the cut faces of the block, so the
// layers under the surface can be read next to the relief. The columns come from the
// same SettlementTerrain and layering the game uses, so what you see is what the game
// generates.
//
//   TerrainSnapshot --lat 46.5 --lon 198.5 --out snap.png          256 m window at a place
//   TerrainSnapshot --lat 46.5 --lon 198.5 --chunks 8              8 x 8 chunks (128 m)
//   TerrainSnapshot --lat 46.5 --lon 198.5 --region 1,0            the whole 4 km region east of the anchor
//   TerrainSnapshot ... --cut 0.5                                  keep the far half: the cut shows the inside
//
// See --help for all options.

#include "planet/PlanetMap.h"
#include "planet/StarSystemGenerator.h"
#include "planet/region/RegionGenerator.h"
#include "planet/region/SettlementTerrain.h"
#include "world/PlanetTerrainSource.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Infinitesia;

namespace
{
    // ------------------------------------------------------------------ options

    struct Options
    {
        double latitude = 0.0;
        double longitude = 0.0;
        bool hasPlace = false;
        double extentMeters = 256.0;
        double stepMeters = 0.0;       // 0: pick automatically
        double originEast = 0.0;       // window centre, metres east / north of the anchor
        double originNorth = 0.0;
        double depthMeters = 0.0;      // ground shown below the lowest surface; 0: automatic
        double cutU = 1.0;             // fraction of the window kept from the far side
        double cutV = 1.0;
        double scale = 0.0;            // pixels per metre; 0: fit about 1600 px
        double verticalExaggeration = 0.0; // 0: automatic
        std::string view = "se";
        std::string output = "snapshot.png";
        std::uint64_t seed = 1;
        int planetBands = 0;           // 0: the game's resolution
    };

    void PrintUsage()
    {
        std::puts(
            "TerrainSnapshot --lat <deg> --lon <deg> [options]\n"
            "\n"
            "Place (required): the settlement anchor on the planet; the window is centred on it\n"
            "  --lat, --lon <deg>     latitude -90..90, longitude 0..360 (east positive)\n"
            "  --seed <n>             planet seed (the game uses 1)\n"
            "  --planet-bands <n>     coarser planet grid for a quick look (not the game's terrain)\n"
            "\n"
            "Window:\n"
            "  --extent <m>           side of the square window in metres (default 256)\n"
            "  --chunks <n>           window of n x n chunks (16 m each); same as --extent 16n\n"
            "  --region <i,j>         the whole 4096 m region i east, j north of the anchor\n"
            "  --east <m>, --north <m>  move the window centre away from the anchor\n"
            "  --step <m>             metres per column (default: 1 up to 256 m, then extent / 256)\n"
            "\n"
            "Picture:\n"
            "  --view se|sw|ne|nw     which corner to look from (default se)\n"
            "  --cut <f>              keep only the far fraction f (0..1) of the window along both\n"
            "                         sides, so the cut faces show the inside; --cut-u / --cut-v for one\n"
            "  --depth <m>            ground drawn below the lowest surface (default extent / 8)\n"
            "  --scale <px>           pixels per metre (default: about 1600 px wide)\n"
            "  --vexag <f>            vertical exaggeration (default grows with the window)\n"
            "  --out <file.png>       output (default snapshot.png)\n");
    }

    bool ParseOptions(int argc, char** argv, Options& options)
    {
        for (int index = 1; index < argc; ++index)
        {
            const std::string key = argv[index];
            const auto value = [&]() -> const char*
            {
                if (index + 1 >= argc)
                {
                    throw std::invalid_argument("Missing a value after " + key);
                }
                return argv[++index];
            };

            if (key == "--help" || key == "-h")
            {
                return false;
            }
            else if (key == "--lat") { options.latitude = std::atof(value()); options.hasPlace = true; }
            else if (key == "--lon") { options.longitude = std::atof(value()); options.hasPlace = true; }
            else if (key == "--seed") { options.seed = std::strtoull(value(), nullptr, 10); }
            else if (key == "--planet-bands") { options.planetBands = std::atoi(value()); }
            else if (key == "--extent") { options.extentMeters = std::atof(value()); }
            else if (key == "--chunks") { options.extentMeters = 16.0 * std::atof(value()); }
            else if (key == "--region")
            {
                int i = 0;
                int j = 0;
                if (std::sscanf(value(), "%d,%d", &i, &j) != 2)
                {
                    throw std::invalid_argument("--region needs i,j");
                }
                options.extentMeters = LocalGrid::kRegionSizeMeters;
                options.originEast = i * LocalGrid::kRegionSizeMeters;
                options.originNorth = j * LocalGrid::kRegionSizeMeters;
            }
            else if (key == "--east") { options.originEast = std::atof(value()); }
            else if (key == "--north") { options.originNorth = std::atof(value()); }
            else if (key == "--step") { options.stepMeters = std::atof(value()); }
            else if (key == "--view") { options.view = value(); }
            else if (key == "--cut") { options.cutU = options.cutV = std::atof(value()); }
            else if (key == "--cut-u") { options.cutU = std::atof(value()); }
            else if (key == "--cut-v") { options.cutV = std::atof(value()); }
            else if (key == "--depth") { options.depthMeters = std::atof(value()); }
            else if (key == "--scale") { options.scale = std::atof(value()); }
            else if (key == "--vexag") { options.verticalExaggeration = std::atof(value()); }
            else if (key == "--out") { options.output = value(); }
            else
            {
                throw std::invalid_argument("Unknown option " + key);
            }
        }
        return true;
    }

    // ------------------------------------------------------------------ image

    struct Rgb
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
    };

    Rgb Mix(Rgb a, Rgb b, double t)
    {
        t = std::clamp(t, 0.0, 1.0);
        return {
            static_cast<std::uint8_t>(a.r + (b.r - a.r) * t),
            static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<std::uint8_t>(a.b + (b.b - a.b) * t)
        };
    }

    Rgb Shade(Rgb color, double factor)
    {
        const auto scale = [factor](int channel)
        {
            return static_cast<std::uint8_t>(std::clamp(channel * factor, 0.0, 255.0));
        };
        return { scale(color.r), scale(color.g), scale(color.b) };
    }

    struct Point
    {
        double x = 0.0;
        double y = 0.0;
    };

    class Image
    {
    public:
        Image(int width, int height, Rgb background)
            : m_width(width), m_height(height)
            , m_pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3)
        {
            for (std::size_t index = 0; index < m_pixels.size(); index += 3)
            {
                m_pixels[index] = background.r;
                m_pixels[index + 1] = background.g;
                m_pixels[index + 2] = background.b;
            }
        }

        int Width() const { return m_width; }
        int Height() const { return m_height; }
        const std::vector<std::uint8_t>& Pixels() const { return m_pixels; }

        // Edges are inclusive, so quads that share an edge leave no seam.
        void FillTriangle(Point a, Point b, Point c, Rgb color)
        {
            const double area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            if (std::abs(area) < 1e-12)
            {
                return;
            }
            const int minX = std::max(0, static_cast<int>(std::floor(std::min({ a.x, b.x, c.x }))));
            const int maxX = std::min(m_width - 1, static_cast<int>(std::ceil(std::max({ a.x, b.x, c.x }))));
            const int minY = std::max(0, static_cast<int>(std::floor(std::min({ a.y, b.y, c.y }))));
            const int maxY = std::min(m_height - 1, static_cast<int>(std::ceil(std::max({ a.y, b.y, c.y }))));
            const double sign = area > 0.0 ? 1.0 : -1.0;
            constexpr double kTolerance = -1e-9;
            for (int y = minY; y <= maxY; ++y)
            {
                for (int x = minX; x <= maxX; ++x)
                {
                    const double px = x + 0.5;
                    const double py = y + 0.5;
                    const double w0 = ((b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x)) * sign;
                    const double w1 = ((c.x - b.x) * (py - b.y) - (c.y - b.y) * (px - b.x)) * sign;
                    const double w2 = ((a.x - c.x) * (py - c.y) - (a.y - c.y) * (px - c.x)) * sign;
                    if (w0 >= kTolerance && w1 >= kTolerance && w2 >= kTolerance)
                    {
                        std::uint8_t* pixel = &m_pixels[(static_cast<std::size_t>(y) * m_width + x) * 3];
                        pixel[0] = color.r;
                        pixel[1] = color.g;
                        pixel[2] = color.b;
                    }
                }
            }
        }

        void FillQuad(const std::array<Point, 4>& q, Rgb color)
        {
            FillTriangle(q[0], q[1], q[2], color);
            FillTriangle(q[0], q[2], q[3], color);
        }

    private:
        int m_width;
        int m_height;
        std::vector<std::uint8_t> m_pixels;
    };

    // Uncompressed PNG: stored deflate blocks, so no compression library is needed.
    std::uint32_t Crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0)
    {
        static const std::array<std::uint32_t, 256> table = []
        {
            std::array<std::uint32_t, 256> t{};
            for (std::uint32_t n = 0; n < 256; ++n)
            {
                std::uint32_t c = n;
                for (int k = 0; k < 8; ++k)
                {
                    c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                }
                t[n] = c;
            }
            return t;
        }();
        crc = ~crc;
        for (std::size_t i = 0; i < size; ++i)
        {
            crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
        }
        return ~crc;
    }

    void WriteBigEndian(std::vector<std::uint8_t>& out, std::uint32_t value)
    {
        for (int shift = 24; shift >= 0; shift -= 8)
        {
            out.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    }

    void AppendChunk(std::vector<std::uint8_t>& file, const char* type, const std::vector<std::uint8_t>& data)
    {
        WriteBigEndian(file, static_cast<std::uint32_t>(data.size()));
        std::vector<std::uint8_t> body(type, type + 4);
        body.insert(body.end(), data.begin(), data.end());
        file.insert(file.end(), body.begin(), body.end());
        WriteBigEndian(file, Crc32(body.data(), body.size()));
    }

    bool WritePng(const std::string& path, const Image& image)
    {
        std::vector<std::uint8_t> raw;
        raw.reserve(static_cast<std::size_t>(image.Height()) * (static_cast<std::size_t>(image.Width()) * 3 + 1));
        for (int y = 0; y < image.Height(); ++y)
        {
            raw.push_back(0); // filter: none
            const std::uint8_t* row = &image.Pixels()[static_cast<std::size_t>(y) * image.Width() * 3];
            raw.insert(raw.end(), row, row + static_cast<std::size_t>(image.Width()) * 3);
        }

        std::vector<std::uint8_t> zlib{ 0x78, 0x01 };
        for (std::size_t offset = 0; offset < raw.size();)
        {
            const std::size_t length = std::min<std::size_t>(65535, raw.size() - offset);
            zlib.push_back(offset + length == raw.size() ? 1 : 0);
            zlib.push_back(static_cast<std::uint8_t>(length & 0xFF));
            zlib.push_back(static_cast<std::uint8_t>(length >> 8));
            zlib.push_back(static_cast<std::uint8_t>(~length & 0xFF));
            zlib.push_back(static_cast<std::uint8_t>((~length >> 8) & 0xFF));
            zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                        raw.begin() + static_cast<std::ptrdiff_t>(offset + length));
            offset += length;
        }
        std::uint32_t a = 1;
        std::uint32_t b = 0;
        for (std::uint8_t byte : raw)
        {
            a = (a + byte) % 65521u;
            b = (b + a) % 65521u;
        }
        WriteBigEndian(zlib, (b << 16) | a);

        std::vector<std::uint8_t> file{ 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
        std::vector<std::uint8_t> header;
        WriteBigEndian(header, static_cast<std::uint32_t>(image.Width()));
        WriteBigEndian(header, static_cast<std::uint32_t>(image.Height()));
        header.insert(header.end(), { 8, 2, 0, 0, 0 });
        AppendChunk(file, "IHDR", header);
        AppendChunk(file, "IDAT", zlib);
        AppendChunk(file, "IEND", {});

        std::FILE* out = std::fopen(path.c_str(), "wb");
        if (out == nullptr)
        {
            return false;
        }
        const bool written = std::fwrite(file.data(), 1, file.size(), out) == file.size();
        std::fclose(out);
        return written;
    }

    // ------------------------------------------------------------------ terrain

    struct Column
    {
        int ground = 0;          // top solid block level, relative to the window centre ground
        int top = 0;             // highest filled level (the sea surface over drowned ground)
        SurfaceKind kind = SurfaceKind::Grass;
    };

    Rgb BlockColor(BlockType type)
    {
        switch (type)
        {
        case BlockType::Grass: return { 82, 145, 66 };
        case BlockType::Dirt:  return { 139, 94, 60 };
        case BlockType::Stone: return { 132, 137, 142 };
        case BlockType::Sand:  return { 219, 203, 140 };
        case BlockType::Water: return { 52, 110, 170 };
        case BlockType::Air:   break;
        }
        return { 255, 0, 255 };
    }

    // ------------------------------------------------------------------ rendering

    struct View
    {
        int columns = 0;          // columns along each side of the full window
        int keptU = 0;            // columns kept along the screen-right-down axis
        int keptV = 0;
        double stepMeters = 1.0;
        double scale = 1.0;       // pixels per metre
        double exaggeration = 1.0;
        double offsetX = 0.0;
        double offsetY = 0.0;

        Point Project(double u, double v, double level) const
        {
            constexpr double kCos30 = 0.86602540378;
            return {
                offsetX + (u - v) * stepMeters * kCos30 * scale,
                offsetY + (u + v) * stepMeters * 0.5 * scale - level * scale * exaggeration
            };
        }
    };

    // View axes u (towards the lower right) and v (towards the lower left) as window
    // indices: east index e grows eastward, south index s grows southward.
    void ViewToWindow(const std::string& view, int n, int u, int v, int& east, int& south)
    {
        if (view == "sw")      { east = n - 1 - v; south = u; }
        else if (view == "nw") { east = n - 1 - u; south = n - 1 - v; }
        else if (view == "ne") { east = v;         south = n - 1 - u; }
        else                   { east = u;         south = v; }
    }

    int Run(const Options& options)
    {
        if (!options.hasPlace)
        {
            std::fprintf(stderr, "Give a place with --lat and --lon (see --help).\n");
            return 2;
        }
        if (options.view != "se" && options.view != "sw" && options.view != "ne" && options.view != "nw")
        {
            std::fprintf(stderr, "--view must be se, sw, ne or nw.\n");
            return 2;
        }
        if (!(options.extentMeters >= 2.0) || options.cutU <= 0.0 || options.cutU > 1.0 ||
            options.cutV <= 0.0 || options.cutV > 1.0)
        {
            std::fprintf(stderr, "Extent must be at least 2 m and cuts must be in (0, 1].\n");
            return 2;
        }

        const double extent = options.extentMeters;
        double step = options.stepMeters;
        if (step <= 0.0)
        {
            step = std::max(1.0, std::ceil(extent / 256.0));
        }
        const int columns = std::max(1, static_cast<int>(std::lround(extent / step)));
        if (static_cast<long long>(columns) * columns > (1LL << 21))
        {
            std::fprintf(stderr, "%d x %d columns is too many; use a larger --step or a smaller --extent.\n", columns, columns);
            return 2;
        }

        // ---- the planet, built the way the game builds it
        const auto started = std::chrono::steady_clock::now();
        std::fprintf(stderr, "Generating the planet (seed %llu)...\n", static_cast<unsigned long long>(options.seed));
        const StarSystem system = StarSystemGenerator::Generate(GenerationScenario{ .seed = options.seed });
        PlanetParameters parameters = system.planet;
        if (options.planetBands > 0)
        {
            parameters.latitudeBands = options.planetBands;
            parameters.longitudeSegments = options.planetBands * 2;
        }
        const PlanetMap map(parameters, ClimateInputsFor(system));
        const RegionGenerator generator(map);

        const CartesianPoint3D anchor = PlanetaryCoordinates::ToCartesian({ options.latitude, options.longitude });
        const LocalGrid grid = generator.MakeGrid(anchor);
        const SettlementTerrain terrain(generator.GetTerrain(), grid, options.originEast, options.originNorth);
        std::fprintf(stderr, "Planet ready in %.1f s.\n",
                     std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());

        // ---- columns in view coordinates, far corner first
        View view;
        view.columns = columns;
        view.keptU = std::clamp(static_cast<int>(std::ceil(options.cutU * columns)), 1, columns);
        view.keptV = std::clamp(static_cast<int>(std::ceil(options.cutV * columns)), 1, columns);
        view.stepMeters = step;

        const int sea = terrain.SeaLevelBlocks();
        std::vector<Column> cells(static_cast<std::size_t>(view.keptU) * static_cast<std::size_t>(view.keptV));
        const auto at = [&](int u, int v) -> Column& { return cells[static_cast<std::size_t>(v) * view.keptU + u]; };

        constexpr int kMaxWaterDrawn = 60;
        int lowestGround = 1 << 30;
        int highestTop = -(1 << 30);
        int drowned = 0;
        bool waterCapped = false;
        for (int v = 0; v < view.keptV; ++v)
        {
            for (int u = 0; u < view.keptU; ++u)
            {
                int east = 0;
                int south = 0;
                ViewToWindow(options.view, columns, u, v, east, south);
                const int eastMeters = static_cast<int>(std::lround(-extent * 0.5 + (east + 0.5) * step));
                const int northMeters = static_cast<int>(std::lround(extent * 0.5 - (south + 0.5) * step));
                const SurfaceColumn column = terrain.Column(eastMeters, northMeters);

                Column& cell = at(u, v);
                cell.ground = column.heightBlocks;
                cell.kind = column.underSea ? SurfaceKind::Sand : column.kind;
                // Over drowned ground the filled column reaches the sea surface, but no more
                // than kMaxWaterDrawn above the ground: a 3 km deep ocean would swamp the picture.
                const int seaTop = sea - 1;
                cell.top = column.underSea ? std::max(column.heightBlocks, std::min(seaTop, column.heightBlocks + kMaxWaterDrawn))
                                           : column.heightBlocks;
                waterCapped = waterCapped || (column.underSea && seaTop > cell.top);
                drowned += column.underSea ? 1 : 0;
                lowestGround = std::min(lowestGround, cell.ground);
                highestTop = std::max(highestTop, cell.top);
            }
        }

        const double depth = options.depthMeters > 0.0 ? options.depthMeters : std::clamp(extent * 0.125, 16.0, 600.0);
        const int bottom = lowestGround - static_cast<int>(std::ceil(depth));

        view.exaggeration = options.verticalExaggeration > 0.0
            ? options.verticalExaggeration
            : std::clamp(extent / 512.0, 1.0, 12.0);
        view.scale = options.scale > 0.0 ? options.scale : 1600.0 / ((columns + columns) * step * 0.86602540378);

        // ---- canvas
        constexpr int kMargin = 24;
        const Point farCorner = view.Project(0, 0, highestTop + 1);
        const Point leftNear = view.Project(0, view.keptV, bottom);
        const Point rightNear = view.Project(view.keptU, 0, bottom);
        const Point nearCorner = view.Project(view.keptU, view.keptV, bottom);
        const double minX = std::min({ farCorner.x, leftNear.x, rightNear.x, nearCorner.x });
        const double maxX = std::max({ farCorner.x, leftNear.x, rightNear.x, nearCorner.x });
        const double minY = std::min({ farCorner.y, leftNear.y, rightNear.y });
        const double maxY = nearCorner.y;
        view.offsetX = kMargin - minX;
        view.offsetY = kMargin - minY;
        const int width = static_cast<int>(std::ceil(maxX - minX)) + 2 * kMargin;
        const int height = static_cast<int>(std::ceil(maxY - minY)) + 2 * kMargin;
        if (width > 12000 || height > 12000)
        {
            std::fprintf(stderr,
                         "The picture would be %d x %d px (surface %d..%d m, sea level %+d, depth %.0f m, "
                         "vertical x%.1f); lower --scale or --vexag.\n",
                         width, height, lowestGround, highestTop, sea, depth, view.exaggeration);
            return 2;
        }
        Image image(width, height, { 24, 28, 34 });

        // ---- paint far to near
        const auto layerColor = [&](const Column& cell, int level) -> Rgb
        {
            // Block at `level` of a column: water above the ground, layers below it.
            const BlockType type = level > cell.ground ? BlockType::Water
                                                       : PlanetTerrainSource::LayerAt(cell.kind, cell.ground - level);
            Rgb color = BlockColor(type);
            if (type == BlockType::Water)
            {
                color = Mix({ 96, 170, 205 }, { 22, 56, 116 }, (cell.top - level) / 40.0);
            }
            // The deeper the cut, the darker, so the depth reads at a glance.
            const double below = static_cast<double>(cell.top - level);
            return Shade(color, std::max(0.5, 1.0 - below * (extent <= 512.0 ? 0.006 : 0.0015)));
        };

        const auto drawFace = [&](const Column& cell, int u, int v, bool rightFront, int downTo)
        {
            // The face of this column towards the camera, from its top down to `downTo` (exclusive).
            const double faceShade = rightFront ? 0.62 : 0.8;
            int level = cell.top;
            while (level > downTo)
            {
                const BlockType type = level > cell.ground ? BlockType::Water
                                                           : PlanetTerrainSource::LayerAt(cell.kind, cell.ground - level);
                // Merge blocks of one kind into one quad (colour varies slowly with depth).
                int last = level;
                while (last - 1 > downTo)
                {
                    const int next = last - 1;
                    const BlockType nextType = next > cell.ground ? BlockType::Water
                                                                   : PlanetTerrainSource::LayerAt(cell.kind, cell.ground - next);
                    if (nextType != type || (cell.top - level) / 8 != (cell.top - next) / 8)
                    {
                        break;
                    }
                    last = next;
                }
                const Rgb color = Shade(layerColor(cell, level), faceShade);
                const double upper = level + 1;
                const double lower = last;
                if (rightFront)
                {
                    image.FillQuad({ view.Project(u + 1, v, upper), view.Project(u + 1, v + 1, upper),
                                     view.Project(u + 1, v + 1, lower), view.Project(u + 1, v, lower) }, color);
                }
                else
                {
                    image.FillQuad({ view.Project(u, v + 1, upper), view.Project(u + 1, v + 1, upper),
                                     view.Project(u + 1, v + 1, lower), view.Project(u, v + 1, lower) }, color);
                }
                level = last - 1;
            }
        };

        const double heightSpan = std::max(1, highestTop - lowestGround);
        for (int v = 0; v < view.keptV; ++v)
        {
            for (int u = 0; u < view.keptU; ++u)
            {
                const Column& cell = at(u, v);

                // Towards the lower right, then the lower left; the outer sides go to the bottom.
                const int rightTop = (u + 1 < view.keptU) ? at(u + 1, v).top : bottom - 1;
                const int leftTop = (v + 1 < view.keptV) ? at(u, v + 1).top : bottom - 1;
                if (rightTop < cell.top)
                {
                    drawFace(cell, u, v, true, rightTop);
                }
                if (leftTop < cell.top)
                {
                    drawFace(cell, u, v, false, leftTop);
                }

                // Top: the surface block, or the sea over drowned ground; a slight tint by height.
                Rgb top;
                if (cell.top > cell.ground)
                {
                    top = Mix({ 96, 170, 205 }, { 22, 56, 116 }, (cell.top - cell.ground) / 40.0);
                }
                else
                {
                    top = BlockColor(PlanetTerrainSource::LayerAt(cell.kind, 0));
                }
                // Hillshade: the light comes from the far corner, so slopes that fall towards the
                // camera are lit and slopes that rise towards it are dark. Slopes are stretched
                // by the vertical exaggeration so gentle ground still shows.
                const auto topAt = [&](int uu, int vv)
                {
                    return at(std::clamp(uu, 0, view.keptU - 1), std::clamp(vv, 0, view.keptV - 1)).top;
                };
                // Measured over about 8 m, not one column: block steps are already drawn as
                // side faces, and a one-column slope would turn every 1 m step into a 45 degree
                // slope and light its edge column up.
                const int reach = std::clamp(static_cast<int>(std::lround(8.0 / step)), 2, 8);
                const double slope =
                    ((topAt(u + reach, v) - topAt(u - reach, v)) + (topAt(u, v + reach) - topAt(u, v - reach))) /
                    (4.0 * reach * step);
                const double lit = std::clamp(1.0 - 2.0 * view.exaggeration * slope, 0.7, 1.3);
                const double tint = (0.92 + 0.16 * (cell.top - lowestGround) / heightSpan) * lit;
                image.FillQuad({ view.Project(u, v, cell.top + 1), view.Project(u + 1, v, cell.top + 1),
                                 view.Project(u + 1, v + 1, cell.top + 1), view.Project(u, v + 1, cell.top + 1) },
                               Shade(top, tint));
            }
        }

        if (!WritePng(options.output, image))
        {
            std::fprintf(stderr, "Could not write %s.\n", options.output.c_str());
            return 1;
        }

        std::printf("%s: %d x %d px\n", options.output.c_str(), width, height);
        std::printf("place %.4f, %.4f  seed %llu  view %s\n", options.latitude, options.longitude,
                    static_cast<unsigned long long>(options.seed), options.view.c_str());
        std::printf("window %.0f m (%d x %d columns of %.2f m), kept %d x %d\n", extent, columns, columns, step,
                    view.keptU, view.keptV);
        std::printf("centre ground %d m above sea level; sea level at %+d from it\n",
                    terrain.OriginElevationMeters(), sea);
        std::printf("surface %d..%d m around the centre (relief %d m), %.0f%% under water\n", lowestGround, highestTop,
                    highestTop - lowestGround, 100.0 * drowned / static_cast<double>(cells.size()));
        std::printf("picture: %.3f px/m, vertical x%.1f, ground shown %.0f m below the lowest surface\n", view.scale,
                    view.exaggeration, depth);
        if (waterCapped)
        {
            std::printf("note: the sea here is deeper than %d m; water is drawn %d m thick over the sea floor\n",
                        kMaxWaterDrawn, kMaxWaterDrawn);
        }

        // ---- geological setting of the planet cell under the window centre
        {
            const int bands = map.GetLatitudeBandCount();
            const int segments = map.GetLongitudeSegmentCount();
            const double longitude = options.longitude - 360.0 * std::floor(options.longitude / 360.0);
            const int row = std::clamp(static_cast<int>((options.latitude + 90.0) / 180.0 * bands), 0, bands - 1);
            const int column = std::clamp(static_cast<int>(longitude / 360.0 * segments), 0, segments - 1);
            const PlanetRegion& cell = map.GetRegion(row, column);
            const Province* province = map.FindProvince(cell);
            if (province != nullptr)
            {
                std::printf("province %s #%u (%.0f%% land, %d cells); main event %.0f Myr ago, erosion %s (%.1f km)\n",
                            std::string(ProvinceTypeName(province->type)).c_str(), province->id,
                            100.0 * province->landFraction, province->cellCount, province->eventAgeMyr,
                            std::string(ErosionLevelName(province->erosionLevel)).c_str(), province->erosionDepthKm);
                const PlanetEpochTable& epochs = map.GetEpochs();
                const Epoch* era = epochs.EpochAt(province->eventAgeMyr, EpochRank::Era);
                const Epoch* period = epochs.EpochAt(province->eventAgeMyr, EpochRank::Period);
                std::printf("main event in the %s era%s%s\n", era != nullptr ? std::string(era->name).c_str() : "?",
                            period != nullptr ? ", " : "",
                            period != nullptr ? std::string(period->name).c_str() : "");
            }
            std::printf("crust %.0f Myr old, sediment %.2f km, heat flow %.0f mW/m2, hotspot %.2f; plate %.1f cm/yr\n",
                        cell.crustAgeMyr, cell.sedimentThicknessKm, cell.heatFlowMwPerM2, cell.hotspotInfluence,
                        std::hypot(cell.plateVelocityEastCmPerYear, cell.plateVelocityNorthCmPerYear));
            if (cell.boundarySubtype != PlateBoundaryKind::None)
            {
                static constexpr const char* kKinds[] = { "none", "mid-ocean ridge", "continental rift",
                                                          "oceanic subduction", "continental subduction",
                                                          "continental collision", "transform fault" };
                std::printf("nearest plate boundary: %s, %.0f km away, relative speed %.1f cm/yr\n",
                            kKinds[static_cast<int>(cell.boundarySubtype)], cell.distanceToBoundaryKm,
                            std::hypot(cell.boundaryRelativeVelocityEastCmPerYear,
                                       cell.boundaryRelativeVelocityNorthCmPerYear));
            }
        }
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        Options options;
        if (!ParseOptions(argc, argv, options))
        {
            PrintUsage();
            return 0;
        }
        return Run(options);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
}

// GeologyInspector: asks the geology what lies at a point of the planet and why.
//
//   GeologyInspector --lat 46.5 --lon 198.5                       the province and the rock 20 m down
//   GeologyInspector --lat ... --lon ... --depth 150              the rock 150 m below the ground
//   GeologyInspector --lat ... --lon ... --east 300 --north -80   a point 300 m east, 80 m south of the anchor
//   GeologyInspector --lat ... --lon ... --column                 the whole column under the point
//   GeologyInspector --lat ... --lon ... --history                every event of the province
//   GeologyInspector --lat ... --lon ... --faults                 the faults near the point
//
// It builds the planet and the geology exactly as the game does, so what it prints is what the
// game's blocks are made of.

#include "geology/model/GeologyModel.h"
#include "planet/PlanetMap.h"
#include "planet/StarSystemGenerator.h"
#include "planet/region/RegionGenerator.h"
#include "planet/region/SettlementTerrain.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Infinitesia;

namespace
{
    struct Options
    {
        double latitude = 0.0;
        double longitude = 0.0;
        bool hasPlace = false;
        double east = 0.0;      // metres east of the anchor
        double north = 0.0;
        double depth = 20.0;    // metres below the ground
        bool hasElevation = false;
        double elevation = 0.0; // metres above sea level, instead of --depth
        bool column = false;
        bool history = false;
        bool faults = false;
        std::uint64_t seed = 1;
        int planetBands = 0;
        std::string assets = INFINITESIA_ASSET_DIR; // the assets folder
    };

    void PrintUsage()
    {
        std::puts(
            "GeologyInspector --lat <deg> --lon <deg> [options]\n"
            "\n"
            "  --lat, --lon <deg>     the settlement anchor on the planet (required)\n"
            "  --east, --north <m>    the point, metres east and north of the anchor (default 0, 0)\n"
            "  --depth <m>            how far below the ground to look (default 20)\n"
            "  --elevation <m>        look at this elevation above sea level instead of --depth\n"
            "  --column               list the formations under the point from the top down\n"
            "  --history              list every event of the province\n"
            "  --faults               list the faults near the point and what they do there\n"
            "  --seed <n>             planet seed (the game uses 1)\n"
            "  --planet-bands <n>     coarser planet grid for a quick look (not the game's terrain)\n"
            "  --assets <dir>         geology catalogs (default: the source tree's assets/geology)\n");
    }

    bool ParseOptions(int argc, char** argv, Options& options)
    {
        for (int index = 1; index < argc; ++index)
        {
            const std::string key = argv[index];
            const auto value = [&]() -> const char*
            {
                if (index + 1 >= argc)
                    throw std::invalid_argument("Missing a value after " + key);
                return argv[++index];
            };
            if (key == "--help" || key == "-h") return false;
            else if (key == "--lat") { options.latitude = std::atof(value()); options.hasPlace = true; }
            else if (key == "--lon") { options.longitude = std::atof(value()); options.hasPlace = true; }
            else if (key == "--east") { options.east = std::atof(value()); }
            else if (key == "--north") { options.north = std::atof(value()); }
            else if (key == "--depth") { options.depth = std::atof(value()); }
            else if (key == "--elevation") { options.elevation = std::atof(value()); options.hasElevation = true; }
            else if (key == "--column") { options.column = true; }
            else if (key == "--history") { options.history = true; }
            else if (key == "--faults") { options.faults = true; }
            else if (key == "--seed") { options.seed = std::strtoull(value(), nullptr, 10); }
            else if (key == "--planet-bands") { options.planetBands = std::atoi(value()); }
            else if (key == "--assets") { options.assets = value(); }
            else throw std::invalid_argument("Unknown option " + key);
        }
        return true;
    }

    std::string RockName(const MaterialCatalog& catalog, MaterialId id)
    {
        const MaterialDef* material = catalog.Get(id);
        return material == nullptr ? "?" : material->key;
    }

    // "Cenozoic / Paleogene, 66 Myr ago"
    std::string AgeText(const PlanetEpochTable& epochs, double ageMyr)
    {
        const Epoch* era = epochs.EpochAt(ageMyr, EpochRank::Era);
        const Epoch* period = epochs.EpochAt(ageMyr, EpochRank::Period);
        char text[160];
        std::snprintf(text, sizeof text, "%s%s%s, %.0f Myr ago", era != nullptr ? std::string(era->name).c_str() : "?",
                      period != nullptr ? " / " : "", period != nullptr ? std::string(period->name).c_str() : "", ageMyr);
        return text;
    }

    void PrintEvent(const MaterialCatalog& catalog, const PlanetEpochTable& epochs, const GeologicEvent& event,
                    const char* prefix)
    {
        std::printf("%s%-22s %7.0f .. %-7.0f Myr", prefix, std::string(EventTypeName(event.type)).c_str(), event.startMyr,
                    event.endMyr);
        if (event.rock != kUnspecifiedMaterial)
        {
            std::printf("  %s (%.0f m%s)", RockName(catalog, event.rock).c_str(), event.magnitude,
                        event.erased ? ", eroded away" : "");
        }
        else if (event.type == EventType::Folding)
        {
            std::printf("  dip %.0f deg", event.magnitude);
        }
        else if (event.type == EventType::Faulting)
        {
            std::printf("  slip up to %.0f m", event.magnitude);
        }
        else if (event.type == EventType::Erosion)
        {
            std::printf("  %.1f km removed", event.magnitude);
        }
        std::printf("  [%s]\n", AgeText(epochs, 0.5 * (event.startMyr + event.endMyr)).c_str());
    }

    int Run(const Options& options)
    {
        if (!options.hasPlace)
        {
            std::fprintf(stderr, "Give a place with --lat and --lon (see --help).\n");
            return 2;
        }

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
        const std::filesystem::path assets = options.assets;
        const auto materials = std::make_shared<const MaterialCatalog>(
            MaterialCatalog::LoadFromFile(assets / "materials" / "materials.json"));
        const auto catalog = std::make_shared<const GeologyCatalog>(
            GeologyCatalog::Load(materials, assets / "geology" / "province_columns.json"));
        const GeologyModel geology(catalog, map);
        const PlanetEpochTable& epochs = map.GetEpochs();

        const CartesianPoint3D anchor = PlanetaryCoordinates::ToCartesian({ options.latitude, options.longitude });
        const LocalGrid grid = generator.MakeGrid(anchor);
        const SettlementTerrain terrain(generator.GetTerrain(), grid, 0.0, 0.0);

        const int eastBlock = static_cast<int>(std::lround(options.east));
        const int northBlock = static_cast<int>(std::lround(options.north));
        const SurfaceColumn column = terrain.Column(eastBlock, northBlock);
        const double eastMeters = eastBlock + 0.5;
        const double northMeters = northBlock + 0.5;
        const int ground = terrain.OriginElevationMeters() + column.heightBlocks;
        const int elevation = options.hasElevation ? static_cast<int>(std::lround(options.elevation))
                                                   : ground - static_cast<int>(std::lround(options.depth));

        // ---- the province
        const Province* province = map.FindProvince(column.province);
        std::printf("place %.4f, %.4f  seed %llu  point %+d m east, %+d m north of the anchor\n", options.latitude,
                    options.longitude, static_cast<unsigned long long>(options.seed), eastBlock, northBlock);
        std::printf("ground %d m above sea level%s; looking at %d m (%d m %s the ground)\n", ground,
                    column.underSea ? " (under the sea)" : "", elevation, std::abs(ground - elevation),
                    elevation <= ground ? "below" : "above");
        if (province == nullptr)
        {
            std::printf("no province here\n");
            return 0;
        }
        std::printf("province %s #%u: %d cells, %.0f%% land, crust %.0f Myr old\n",
                    std::string(ProvinceTypeName(province->type)).c_str(), province->id, province->cellCount,
                    100.0 * province->landFraction, province->meanCrustAgeMyr);
        std::printf("main event %s; erosion %s (%.1f km); plate motion axis %.0f deg\n",
                    AgeText(epochs, province->eventAgeMyr).c_str(), std::string(ErosionLevelName(province->erosionLevel)).c_str(),
                    province->erosionDepthKm, province->compressionAzimuthDegrees);

        const GeologyModel::Structure& structure = geology.StructureOf(province->id);
        std::printf("structure: layers dip %.0f deg towards %.0f deg, %zu folds", structure.dipDegrees,
                    structure.dipAzimuthDegrees, structure.folds.size());
        if (!structure.folds.empty())
        {
            std::printf(" (wavelengths");
            for (const GeologyModel::Fold& fold : structure.folds)
                std::printf(" %.0f", fold.wavelengthMeters);
            std::printf(" m)");
        }
        std::printf("\n");

        // ---- the point
        const GeologyModel::ColumnProfile profile =
            geology.ProfileAt(province->id, eastMeters, northMeters, column.pileTopMeters);
        const MaterialId rock = GeologyModel::RockAt(profile, elevation);
        const MaterialDef* material = materials->Get(rock);
        std::printf("\n");
        if (elevation > ground)
        {
            std::printf("the point is above the ground: %s\n", column.underSea && elevation <= 0 ? "sea water or air" : "air");
        }
        else if (ground - elevation < 4)
        {
            std::printf("topsoil (the top 3 m under the ground), over:\n");
        }
        if (material != nullptr && elevation <= ground)
        {
            std::printf("rock %s (%s)", material->key.c_str(), material->name.c_str());
            std::printf(", %.0f kg/m3, dig class %d", material->density, material->digClass);
            std::printf(", colour #%02x%02x%02x\n", material->color.r, material->color.g, material->color.b);

            const std::vector<EventId> chain =
                geology.ProvenanceAt(province->id, eastMeters, northMeters, elevation, column.pileTopMeters);
            const ProvinceHistory& history = geology.HistoryOf(province->id);
            std::printf("it came about like this (oldest first):\n");
            for (EventId id : chain)
            {
                if (const GeologicEvent* event = history.Find(id))
                    PrintEvent(*materials, epochs, *event, "  ");
            }
        }

        // ---- the column under the point, top down
        if (options.column)
        {
            std::printf("\ncolumn under the point (top formation first; elevations in metres above sea level):\n");
            const ProvinceHistory& history = geology.HistoryOf(province->id);
            for (int i = 0; i < profile.count; ++i)
            {
                const int top = i == 0 ? 0 : profile.bottom[static_cast<std::size_t>(i - 1)];
                const int bottom = profile.bottom[static_cast<std::size_t>(i)];
                char range[64];
                if (i == 0 && i + 1 < profile.count)
                    std::snprintf(range, sizeof range, "above .. %d", bottom);
                else if (i + 1 >= profile.count)
                    std::snprintf(range, sizeof range, "%d .. below", top);
                else
                    std::snprintf(range, sizeof range, "%d .. %d (%d m)", top, bottom, top - bottom);
                const GeologicEvent* event = history.Find(
                    geology.FormationEvent(province->id, profile.formation[static_cast<std::size_t>(i)]));
                std::printf("  %-26s %-24s %s\n", range, RockName(*materials, profile.rock[static_cast<std::size_t>(i)]).c_str(),
                            event != nullptr ? AgeText(epochs, 0.5 * (event->startMyr + event->endMyr)).c_str() : "");
            }
        }

        // ---- the faults near the point
        if (options.faults)
        {
            std::printf("\nfaults near the point (oldest first):\n");
            static const char* const kKinds[] = { "normal", "thrust", "strike-slip" };
            const std::vector<GeologyModel::Fault> faults = geology.FaultsNear(province->id, eastMeters, northMeters);
            if (faults.empty())
                std::printf("  none\n");
            for (const GeologyModel::Fault& fault : faults)
            {
                const GeologyModel::FaultEffect effect =
                    GeologyModel::EffectOf(fault, eastMeters, northMeters, column.pileTopMeters);
                const double distance = std::hypot(fault.centerEast - eastMeters, fault.centerNorth - northMeters);
                std::printf("  %-11s strike %3.0f dip %2.0f, slip %3.0f m, centre %.0f m away", kKinds[static_cast<int>(fault.kind)],
                            std::fmod(fault.strikeAzimuthDegrees + 720.0, 360.0), fault.dipDegrees, fault.slipMeters, distance);
                if (effect.amplitude > 0.0)
                {
                    std::printf("; here it moves the block above %.0f m (plane at %.0f m)", effect.amplitude, effect.cutElevation);
                }
                else
                {
                    std::printf("; does not reach this column");
                }
                std::printf("\n");
            }
        }

        // ---- the history of the province
        if (options.history)
        {
            std::printf("\nhistory of province #%u, oldest event first:\n", province->id);
            for (const GeologicEvent& event : geology.HistoryOf(province->id).events)
                PrintEvent(*materials, epochs, event, "  ");
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

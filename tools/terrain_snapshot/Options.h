#pragma once

#include <cstdint>
#include <string>

namespace SnapshotTool
{
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
        std::string assets = INFINITESIA_ASSET_DIR; // the assets folder: materials, geology, blocks
        bool map = false;              // a geological map from above instead of the block picture
    };

    // Prints the command-line help.
    void PrintUsage();

    // Reads the command line. Returns false when help was asked for; throws on a bad option.
    bool ParseOptions(int argc, char** argv, Options& options);

    // Checks what needs no planet; prints the problem and returns false if the options are wrong.
    bool ValidateOptions(const Options& options);
}

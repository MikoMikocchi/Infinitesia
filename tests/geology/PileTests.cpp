#include <doctest/doctest.h>

#include "geology/model/GeologyModel.h"
#include "support/GeologyFixtures.h"
#include "support/Require.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

using namespace Infinitesia;
using namespace InfinitesiaTests;

TEST_CASE("EveryProvinceGetsAPileOfFormations")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);

    for (const Province& province : map.GetProvinces())
    {
        const std::vector<GeologyModel::Layer>& pile = model.PileOf(province.id);
        REQUIRE(!pile.empty());
        const ProvinceColumn& column = catalog->ColumnFor(province.type);
        Require(pile.size() <= column.bottomUp.size(), "a pile has more layers than its column");
        Require(pile.back().rock == column.bottomUp.front().rock, "the basement is not the column's first layer");
        for (std::size_t i = 0; i + 1 < pile.size(); ++i)
        {
            Require(pile[i].thicknessMeters >= 1, "a formation has no thickness");
        }
        Require(model.ExposedRock(province.id) == pile.front().rock, "ExposedRock is not the top of the pile");
        for (const GeologyModel::Layer& layer : pile)
        {
            Require(catalog->GetRock(layer.rock) != nullptr, "a layer is not a rock");
        }

        // Erosion strips the top: the deeper the cut the fewer formations remain.
        if (province.erosionLevel == ErosionLevel::Deep)
        {
            Require(pile.size() == 1, "a deeply eroded province keeps more than the basement");
        }
    }
    // Unknown provinces fall back to granite basement.
    CHECK(model.BedrockAt(kNoProvince, 0.0, 0.0, -100, 0) == RockNamed("rock.granite").material);
}

TEST_CASE("ProfilesFollowThePileOrderFromTopToBasement")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);

    for (const Province& province : map.GetProvinces())
    {
        const std::vector<GeologyModel::Layer>& pile = model.PileOf(province.id);
        for (double east = -2500.0; east <= 2500.0; east += 830.0)
        {
            for (double north = -2500.0; north <= 2500.0; north += 710.0)
            {
                const GeologyModel::ColumnProfile profile = model.BaseProfileAt(province.id, east, north, 300);
                Require(profile.count >= 1 && profile.count <= static_cast<int>(pile.size()), "a profile has the wrong number of layers");

                // The rocks of a profile are the pile's, in the pile's order, with some layers absent.
                std::size_t next = 0;
                for (int i = 0; i < profile.count; ++i)
                {
                    while (next < pile.size() && pile[next].rock != profile.rock[static_cast<std::size_t>(i)])
                        ++next;
                    Require(next < pile.size(), "a profile layer is not in the pile, or out of order");
                    ++next;
                }
                Require(profile.rock[static_cast<std::size_t>(profile.count - 1)] == pile.back().rock, "the profile does not end in basement");
                for (int i = 1; i < profile.count; ++i)
                {
                    Require(profile.bottom[static_cast<std::size_t>(i)] <= profile.bottom[static_cast<std::size_t>(i - 1)],
                            "a lower layer's bottom is above the layer over it");
                }

                // Above everything is the top layer of the column, below everything the basement.
                Require(GeologyModel::RockAt(profile, 100000) == profile.rock[0], "high above a column is not its top layer");
                Require(GeologyModel::RockAt(profile, -100000000) == pile.back().rock, "far below a column is not basement");
                for (int i = 0; i + 1 < profile.count; ++i)
                {
                    // (A layer thinner than a metre collapses onto the one above it.)
                    const bool collapsed = i > 0 && profile.bottom[static_cast<std::size_t>(i)] == profile.bottom[static_cast<std::size_t>(i - 1)];
                    Require(collapsed || GeologyModel::RockAt(profile, profile.bottom[static_cast<std::size_t>(i)]) == profile.rock[static_cast<std::size_t>(i)],
                            "the bottom elevation of a layer is not in the layer");
                }
            }
        }
    }
}

TEST_CASE("LayersDipAlongTheCompressionAxisAndFoldWithinTheFlankLimit")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);
    const double pi = 3.14159265358979323846;

    int steepProvinces = 0;
    for (const Province& province : map.GetProvinces())
    {
        const GeologyModel::Structure& structure = model.StructureOf(province.id);
        const StructureStyle& style = catalog->ColumnFor(province.type).style;

        Require(structure.dipDegrees >= style.minDipDegrees && structure.dipDegrees <= style.maxDipDegrees, "dip is outside the range of the style");
        const double slope = std::tan(structure.dipDegrees * pi / 180.0);
        Require(std::abs(std::hypot(structure.slopeEast, structure.slopeNorth) - slope) < 1e-9, "the slope does not match the dip");
        // The elevation of the layers falls along the dip azimuth.
        const double azimuth = structure.dipAzimuthDegrees * pi / 180.0;
        Require(structure.slopeEast * std::sin(azimuth) + structure.slopeNorth * std::cos(azimuth) < 1e-9, "the layers rise along their dip");

        if (style.maxDipDegrees >= 15.0)
        {
            // Strongly deformed provinces dip along the axis of the plate motion (either way).
            double difference = std::abs(std::fmod(structure.dipAzimuthDegrees - province.compressionAzimuthDegrees + 360.0, 180.0));
            difference = std::min(difference, 180.0 - difference);
            Require(difference < 1e-6, "a compressed province does not dip along its compression axis");
            steepProvinces += structure.dipDegrees > 10.0 ? 1 : 0;
        }

        Require(static_cast<int>(structure.folds.size()) == style.foldCount, "the number of folds does not match the style");
        double previousWavelength = 1e18;
        for (const GeologyModel::Fold& fold : structure.folds)
        {
            Require(fold.wavelengthMeters < previousWavelength && fold.wavelengthMeters >= 40.0 && fold.wavelengthMeters <= 6000.0,
                    "fold wavelengths must fall from kilometres to tens of metres");
            previousWavelength = fold.wavelengthMeters;
            Require(fold.amplitudeMeters <= 1.7320508075688772 * fold.wavelengthMeters / (2.0 * pi) + 1e-9, "a fold flank is steeper than 60 degrees");
            Require(std::abs(std::hypot(fold.directionEast, fold.directionNorth) - 1.0) < 1e-9, "the fold direction is not a unit vector");
        }

        // The deformation is bounded by the dip over the distance and the fold amplitudes.
        double bound = 0.0;
        for (const GeologyModel::Fold& fold : structure.folds)
            bound += fold.amplitudeMeters;
        for (double distance : { 0.0, 150.0, 900.0 })
        {
            const double lift = model.Displacement(province.id, distance * 0.6, distance * 0.8);
            Require(std::abs(lift) <= slope * distance + bound + 1e-6, "the displacement exceeds its bound");
        }
    }
    CHECK(steepProvinces > 0);
}

TEST_CASE("FormationsPinchOutIntoLenses")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel model(catalog, map);

    // A layer that thins to nothing leaves a profile with fewer layers than the pile; it is
    // present elsewhere in the same province.
    int provincesWithLenses = 0;
    for (const Province& province : map.GetProvinces())
    {
        const std::size_t layers = model.PileOf(province.id).size();
        int missingSomewhere = 0;
        int fullSomewhere = 0;
        for (double east = -3000.0; east <= 3000.0; east += 97.0)
        {
            for (double north = -3000.0; north <= 3000.0; north += 103.0)
            {
                const int count = model.ProfileAt(province.id, east, north, 0).count;
                missingSomewhere += count < static_cast<int>(layers) ? 1 : 0;
                fullSomewhere += count == static_cast<int>(layers) ? 1 : 0;
            }
        }
        if (missingSomewhere > 0 && fullSomewhere > 0)
            ++provincesWithLenses;
    }
    CHECK(provincesWithLenses > 0);
}

TEST_CASE("ProfilesAreDeterministicAndDependOnThePosition")
{
    const PlanetMap& map = SharedMap();
    const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
    const GeologyModel first(catalog, map);
    const GeologyModel second(catalog, map);

    int movedProfiles = 0;
    for (const Province& province : map.GetProvinces())
    {
        const auto a = first.ProfileAt(province.id, 123.5, -456.5, 200);
        const auto b = second.ProfileAt(province.id, 123.5, -456.5, 200);
        REQUIRE(a.count == b.count);
        for (int i = 0; i < a.count; ++i)
        {
            Require(a.rock[static_cast<std::size_t>(i)] == b.rock[static_cast<std::size_t>(i)] &&
                    a.bottom[static_cast<std::size_t>(i)] == b.bottom[static_cast<std::size_t>(i)],
                    "a profile changed between builds");
        }
        const auto elsewhere = first.ProfileAt(province.id, 4000.0, 3000.0, 200);
        if (elsewhere.count != a.count || (a.count > 1 && elsewhere.bottom[0] != a.bottom[0]))
            ++movedProfiles;
    }
    CHECK(movedProfiles > 0);
}

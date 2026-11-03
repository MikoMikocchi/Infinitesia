#pragma once

#include "geology/catalog/GeologyCatalog.h"
#include "geology/history/ProvinceHistory.h"
#include "geology/model/GeologyPlanetView.h"

#include <array>
#include <memory>
#include <vector>

namespace Infinitesia
{
    // Game-scale factors of the geology, applied in one place and recorded in saves
    // (docs/03_GEOLOGY.md, section 18).
    struct GeologyConfig
    {
        // Real strata are 10-1000 m thick and the world window is 192 m high.
        double strataThicknessScale = 0.2;
    };

    // What the ground is made of below the soil.
    //
    // Every province has a pile of formations taken from the catalog's typical column for
    // its type, cut by erosion to the level the province's erosion class says. The pile is
    // then deformed in plan view (docs/03_GEOLOGY.md, section 8.1): the layers dip, are
    // folded along the direction plate motion compresses the province, and change thickness
    // from place to place, pinching out into lenses. Positions are metres east and north of
    // the settlement anchor and elevations are metres above sea level, so the model does
    // not know about chunks, regions or the window.
    //
    // Faults cut the deformed pile: planes with a slip that dies out towards the edge of the
    // fault, placed on a lattice so that any place knows its faults without a global list
    // (normal faults where the crust is stretched, thrusts where it is compressed, strike-slip
    // faults beside them). The block above a fault plane moves along it; what lies there is
    // what the undeformed pile had at the place it came from.
    //
    // Not yet modelled (later steps of G2): intrusions, and events behind the structures.
    // The deformation is a function of the position only; far from the anchor
    // (many kilometres, with a steep dip) the pile slides out of reach and only the top
    // formation or the basement is left.
    class GeologyModel
    {
    public:
        static constexpr int kMaxProfileLayers = 32;

        // The rocks under one column, top formation first. Layer i is the rock between its
        // `bottom` elevation and the bottom of layer i - 1 (the first has nothing above it, the
        // last is basement with nothing below). Layers that pinch out at the column are absent.
        struct ColumnProfile
        {
            int count = 0;
            std::array<MaterialId, kMaxProfileLayers> rock{};
            std::array<int, kMaxProfileLayers> bottom{}; // metres above sea level, rounded
            // The formation each layer belongs to: its place in the province's pile, top first.
            // A fault can cut one formation into two layers; a formation can also be one layer
            // for two pieces of one rock whose separating layer pinched out (the upper one counts).
            std::array<std::uint8_t, kMaxProfileLayers> formation{};
        };

        // One fold: a sine wave of the layer surfaces along `direction`.
        struct Fold
        {
            double wavelengthMeters = 0.0;
            double amplitudeMeters = 0.0;
            double directionEast = 0.0; // unit vector the wave runs along (across the fold axis)
            double directionNorth = 1.0;
            double phase = 0.0;
        };

        // How the layers of one formation vary sideways.
        struct LayerShape
        {
            double variability = 0.0;      // thickness * (1 + variability * noise)
            double noiseWavelengthMeters = 600.0;
            std::uint64_t noiseSeed = 0;
        };

        enum class FaultKind : std::uint8_t { Normal, Thrust, StrikeSlip };

        // A fault plane. Positions are metres east and north of the anchor; the centre elevation
        // is given relative to the top of the pile of the column asked about.
        struct Fault
        {
            FaultKind kind = FaultKind::Normal;
            double centerEast = 0.0;
            double centerNorth = 0.0;
            double centerOffsetMeters = 0.0; // elevation of the centre minus the pile top
            double strikeAzimuthDegrees = 0.0;
            double dipDegrees = 60.0;
            double dipAzimuthDegrees = 0.0;  // horizontal direction the plane falls towards
            double strikeRadiusMeters = 0.0;
            double dipRadiusMeters = 0.0;    // measured along the plane
            double slipMeters = 0.0;         // displacement along the slip direction at the centre
            double sense = 1.0;              // +1 or -1: the side a strike-slip fault moves towards
            double age = 0.0;                // 0..1, a younger fault (larger) cuts older ones
        };

        // What a fault does to the column at one place: the plane crosses it at `cutElevation`
        // and the block above came from (east - shiftEast, north - shiftNorth), raised by `lift`.
        struct FaultEffect
        {
            double amplitude = 0.0; // 0 where the fault does not reach the column
            double cutElevation = 0.0;
            double shiftEast = 0.0;
            double shiftNorth = 0.0;
            double lift = 0.0;
        };

        // The deformation of a province's pile.
        struct Structure
        {
            FaultStyle faults;
            double dipDegrees = 0.0;
            double dipAzimuthDegrees = 0.0; // direction the layers fall towards, clockwise from north
            double slopeEast = 0.0;         // change of layer elevation per metre east
            double slopeNorth = 0.0;
            std::vector<Fold> folds;
            double warpWavelengthMeters = 700.0; // folds are bent by noise of this scale
            double warpRadians = 1.5;
            double compressionAzimuthDegrees = 0.0; // axis of the plate motion
            std::uint64_t seed = 0;
            std::vector<LayerShape> layers;      // one per formation of the pile
        };

        // `planet` converts implicitly from a PlanetMap; it is only read during construction.
        GeologyModel(std::shared_ptr<const GeologyCatalog> catalog, const GeologyPlanetView& planet,
                     GeologyConfig config = {});

        const GeologyCatalog& Catalog() const noexcept { return *m_catalog; }
        std::shared_ptr<const GeologyCatalog> CatalogPointer() const noexcept { return m_catalog; }
        const GeologyConfig& Config() const noexcept { return m_config; }

        // The rocks under the point (east, north) of a province whose undeformed pile has its
        // top `pileTopMeters` above sea level. kNoProvince gives granite basement.
        ColumnProfile ProfileAt(ProvinceId province, double eastMeters, double northMeters, int pileTopMeters) const;

        // Where the rock at a point came from: the events that made it and changed it, oldest
        // first (its formation, the folding, the faulting if a fault displaced it there, the
        // erosion that cut the present surface). Every point has at least its formation.
        std::vector<EventId> ProvenanceAt(ProvinceId province, double eastMeters, double northMeters,
                                          int elevationMeters, int pileTopMeters) const;

        // The history of a province: all its events in order, and which belongs to which formation.
        const ProvinceHistory& HistoryOf(ProvinceId province) const noexcept;
        // The event that made the formation at a place in the province's pile (top first).
        EventId FormationEvent(ProvinceId province, std::size_t pileIndex) const noexcept;

        // The same column without faults (folds, dip and thickness changes only).
        ColumnProfile BaseProfileAt(ProvinceId province, double eastMeters, double northMeters, int pileTopMeters) const;

        // The faults whose reach may include the column, oldest first (at most kMaxFaultsPerColumn,
        // the largest slips when there are more).
        static constexpr int kMaxFaultsPerColumn = 10;
        std::vector<Fault> FaultsNear(ProvinceId province, double eastMeters, double northMeters) const;

        // How a fault displaces the column at (east, north); amplitude 0 where it does not.
        static FaultEffect EffectOf(const Fault& fault, double eastMeters, double northMeters, int pileTopMeters) noexcept;

        // The rock at an elevation in a profile: the layer whose bottom lies at or below it.
        static MaterialId RockAt(const ColumnProfile& profile, int elevationMeters) noexcept;
        // The formation (place in the pile, top first) of that layer.
        static std::size_t FormationAt(const ColumnProfile& profile, int elevationMeters) noexcept;

        // Shorthand for building the profile and asking one elevation.
        MaterialId BedrockAt(ProvinceId province, double eastMeters, double northMeters, int elevationMeters,
                             int pileTopMeters) const;

        // Elevation change of every layer surface at a point: dip plus folds. Positive lifts.
        double Displacement(ProvinceId province, double eastMeters, double northMeters) const noexcept;

        // The formation exposed at the top of the undeformed pile.
        MaterialId ExposedRock(ProvinceId province) const noexcept;

        // Formations of a province from the exposed one down to the basement, with the
        // thickness each has after scaling (the basement's is zero: it has no bottom).
        struct Layer
        {
            MaterialId rock = 0;
            int thicknessMeters = 0;
        };
        const std::vector<Layer>& PileOf(ProvinceId province) const noexcept;
        const Structure& StructureOf(ProvinceId province) const noexcept;

    private:
        static double Displacement(const Structure& structure, double eastMeters, double northMeters) noexcept;
        ColumnProfile FaultedProfile(ProvinceId province, const std::vector<Fault>& faults, std::size_t count,
                                     double eastMeters, double northMeters, int pileTopMeters) const;

        std::shared_ptr<const GeologyCatalog> m_catalog;
        GeologyConfig m_config;
        std::vector<std::vector<Layer>> m_piles;   // by ProvinceId - 1
        std::vector<Structure> m_structures;       // by ProvinceId - 1
        std::vector<ProvinceHistory> m_histories;      // by ProvinceId - 1
        std::vector<std::vector<int>> m_pileEvents;    // per province, per pile index: index in the history
        ProvinceHistory m_fallbackHistory;
        std::vector<Layer> m_fallbackPile;
        Structure m_fallbackStructure;
    };
}

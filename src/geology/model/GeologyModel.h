#pragma once

#include "geology/catalog/GeologyCatalog.h"
#include "planet/PlanetMap.h"

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
    // Interim model (stage G1): every province has a flat pile of formations taken from
    // the catalog's typical column for its type. The pile is cut by erosion to the level
    // the province's erosion class says, and its layers lie at fixed elevations, so hills
    // expose younger layers and valleys older ones. Stage G2 replaces the flat pile with
    // structures (dip, folds, faults) derived from the province history.
    class GeologyModel
    {
    public:
        GeologyModel(std::shared_ptr<const GeologyCatalog> catalog, const PlanetMap& map, GeologyConfig config = {});

        const GeologyCatalog& Catalog() const noexcept { return *m_catalog; }
        std::shared_ptr<const GeologyCatalog> CatalogPointer() const noexcept { return m_catalog; }
        const GeologyConfig& Config() const noexcept { return m_config; }

        // The rock at `elevationMeters` (above sea level) in a province whose pile of
        // formations has its top at `pileTopMeters`. Anything higher than the top is the
        // top formation; below the last layer is basement. kNoProvince gives basement of
        // a generic crust (granite).
        MaterialId BedrockAt(ProvinceId province, int elevationMeters, int pileTopMeters) const noexcept;

        // The formation exposed at the top of the province's pile.
        MaterialId ExposedRock(ProvinceId province) const noexcept;

        // Formations of a province from the exposed one down to the basement, with the
        // thickness each has after scaling (the basement's is zero: it has no bottom).
        struct Layer
        {
            MaterialId rock = 0;
            int thicknessMeters = 0;
        };
        const std::vector<Layer>& PileOf(ProvinceId province) const noexcept;

    private:
        std::shared_ptr<const GeologyCatalog> m_catalog;
        GeologyConfig m_config;
        std::vector<std::vector<Layer>> m_piles; // by ProvinceId - 1
        std::vector<Layer> m_fallback;
    };
}

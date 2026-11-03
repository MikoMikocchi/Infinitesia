#pragma once

#include "foundation/LoadProgress.h"
#include "planet/PlanetRegion.h"
#include "planet/pipeline/PlanetData.h"
#include "planet/pipeline/PlanetPipeline.h"

#include <vector>

namespace Infinitesia
{
    // -----------------------------------------------------------------------
    // PlanetMap — caches a fully-generated grid; GetRegion is O(1)
    //
    // The map is produced by a PlanetPipeline: a list of stages, each declaring what it
    // needs and what it provides (docs/05_PLANET_PIPELINE.md). The default pipeline makes
    // the planet the game uses; a pipeline with extra stages adds more data, which lives
    // in Layers().
    // -----------------------------------------------------------------------
    class PlanetMap
    {
    public:
        explicit PlanetMap(PlanetParameters parameters = {}, ClimateInputs climate = {},
                           LoadProgress* progress = nullptr);
        PlanetMap(PlanetParameters parameters, ClimateInputs climate, const PlanetPipeline& pipeline,
                  LoadProgress* progress = nullptr);

        const Planet&        GetPlanet()        const noexcept;
        const PlanetParameters& GetParameters() const noexcept;
        const ClimateInputs& GetClimateInputs() const noexcept;
        int GetLatitudeBandCount()    const noexcept;
        int GetLongitudeSegmentCount() const noexcept;

        const PlanetRegion& GetRegion(int latitudeIndex, int longitudeIndex) const noexcept;
        double SampleElevation(int latitudeIndex, int longitudeIndex) const;
        bool   IsOcean(int latitudeIndex, int longitudeIndex) const;
        const PlanetWaterBalance& GetWaterBalance() const noexcept { return m_data.waterBalance; }

        // Geological provinces of the planet, indexed by ProvinceId - 1.
        const std::vector<Province>& GetProvinces() const noexcept { return m_data.provinces; }
        // nullptr for kNoProvince or an unknown id.
        const Province* FindProvince(ProvinceId id) const noexcept;
        const Province* FindProvince(const PlanetRegion& region) const noexcept { return FindProvince(region.provinceId); }
        const PlanetEpochTable& GetEpochs() const noexcept { return m_data.epochs; }

        // Data added by stages beyond the default ones, by layer name.
        const PlanetLayers& Layers() const noexcept { return m_data.layers; }

    private:
        PlanetData m_data;
    };
}

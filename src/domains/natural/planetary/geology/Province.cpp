#include "domains/natural/planetary/geology/Province.h"

namespace Infinitesia
{
    std::string_view ProvinceTypeName(ProvinceType type) noexcept
    {
        switch (type)
        {
        case ProvinceType::OceanicRidge: return "oceanic_ridge";
        case ProvinceType::AbyssalPlain: return "abyssal_plain";
        case ProvinceType::Trench: return "trench";
        case ProvinceType::IslandArc: return "island_arc";
        case ProvinceType::ContinentalArc: return "continental_arc";
        case ProvinceType::CollisionOrogen: return "collision_orogen";
        case ProvinceType::AncientOrogen: return "ancient_orogen";
        case ProvinceType::RiftSystem: return "rift_system";
        case ProvinceType::LargeIgneousProvince: return "large_igneous_province";
        case ProvinceType::Craton: return "craton";
        case ProvinceType::Platform: return "platform";
        case ProvinceType::SedimentaryBasin: return "sedimentary_basin";
        case ProvinceType::PassiveMargin: return "passive_margin";
        case ProvinceType::MetamorphicComplex: return "metamorphic_complex";
        }
        return "unknown";
    }

    ErosionLevel ClassifyErosionDepth(double depthKm) noexcept
    {
        if (depthKm < 1.0) return ErosionLevel::Surface;
        if (depthKm < 5.0) return ErosionLevel::Shallow;
        if (depthKm < 15.0) return ErosionLevel::Intermediate;
        return ErosionLevel::Deep;
    }

    std::string_view ErosionLevelName(ErosionLevel level) noexcept
    {
        switch (level)
        {
        case ErosionLevel::Surface: return "surface";
        case ErosionLevel::Shallow: return "shallow";
        case ErosionLevel::Intermediate: return "intermediate";
        case ErosionLevel::Deep: return "deep";
        }
        return "unknown";
    }
}

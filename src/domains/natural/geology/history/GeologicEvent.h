#pragma once

#include "materials/MaterialId.h"
#include "domains/natural/planetary/geology/Province.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Infinitesia
{
    // Stable identifier of an event: a hash of the planet seed, the province and the place of
    // the event in the province's history, so it does not depend on what else was generated.
    using EventId = std::uint64_t;

    // What happened (docs/03_GEOLOGY.md, section 7.1). Only the types the model produces so
    // far; intrusions and mineralization join with their stages.
    enum class EventType : std::uint8_t
    {
        BasementFormation,    // the crust of the province forms (magmatic crystallisation)
        Sedimentation,        // a sedimentary formation is deposited
        VolcanicEpisode,      // lava and ash build a volcanic formation
        Intrusion,            // magma crystallises as an intrusive body
        RegionalMetamorphism, // a formation is recrystallised under pressure and heat
        Folding,              // the pile is tilted and folded (orogeny, compression)
        Faulting,             // the folded pile breaks along faults
        Erosion,              // the top of the pile is stripped off, down to the present level
    };

    std::string_view EventTypeName(EventType type) noexcept;

    // A thing an event made or changed.
    struct ObjectRef
    {
        enum class Kind : std::uint8_t { Formation, Structure, FaultSet };

        Kind kind = Kind::Formation;
        std::uint32_t index = 0; // for a formation: its place in the province's column, bottom first

        friend bool operator==(const ObjectRef&, const ObjectRef&) = default;
    };

    struct GeologicEvent
    {
        EventId id = 0;
        EventType type = EventType::Sedimentation;
        ProvinceId province = kNoProvince;
        // Million years before the present; startMyr > endMyr. Never earlier than a parent.
        double startMyr = 0.0;
        double endMyr = 0.0;
        // Formation events: the rock that resulted. kUnspecifiedMaterial for the others.
        MaterialId rock = kUnspecifiedMaterial;
        // Type-dependent size: real thickness in metres for a formation, dip in degrees for
        // folding, the largest slip in metres for faulting, depth in kilometres for erosion.
        double magnitude = 0.0;
        // A formation event whose rock has been eroded away from this province's pile.
        bool erased = false;
        std::vector<EventId> parents;  // what caused it: the layer below, the deformation before it
        std::vector<ObjectRef> products;
    };
}

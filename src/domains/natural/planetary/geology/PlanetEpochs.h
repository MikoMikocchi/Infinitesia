#pragma once

#include <span>
#include <string_view>
#include <vector>

namespace Infinitesia
{
    enum class EpochRank : unsigned char { Eon, Era, Period };

    // One unit of the planet's geological time scale. Ages are million years before the
    // present on the planet's own clock (start > end).
    struct Epoch
    {
        std::string_view name;
        EpochRank rank = EpochRank::Era;
        double startMyr = 0.0;
        double endMyr = 0.0;
    };

    // What the surface environment allowed at a given time. Deposit models and rock
    // types are gated on these (no coal before land plants, no red beds before free
    // oxygen, banded iron formations only in their era, ...).
    struct BiosphereState
    {
        bool freeOxygen = false;                 // oxidised weathering, red beds, no dissolved iron
        bool bandedIronFormations = false;       // Algoma and Superior-type iron deposition
        bool landPlants = false;                 // peat and coal swamps
        bool organicRichMarineSediments = false; // plankton-fed black shales, petroleum source rocks
        bool calcareousPlankton = false;         // chalk and pelagic limestone
    };

    // The geological time scale of a planet. Only the Earth-like timeline exists: the
    // ICS divisions and Earth's biosphere milestones, with every age scaled by
    // planetAge / 4540 Myr so the same sequence of eons and events fits a younger or
    // older planet (docs/03_GEOLOGY.md, section 6.3). A scenario without a biosphere
    // keeps the divisions but none of the biosphere states.
    class PlanetEpochTable
    {
    public:
        PlanetEpochTable();
        explicit PlanetEpochTable(double planetAgeMyr, bool biosphere = true);

        double PlanetAgeMyr() const noexcept { return m_planetAgeMyr; }
        bool HasBiosphere() const noexcept { return m_biosphere; }

        // All divisions, eons first, then eras, then periods; within a rank oldest first.
        std::span<const Epoch> Epochs() const noexcept { return m_epochs; }

        // The division of the given rank containing the age; nullptr outside the planet's
        // history or where the rank has no division (periods before the Siderian).
        const Epoch* EpochAt(double ageMyr, EpochRank rank) const noexcept;

        BiosphereState BiosphereAt(double ageMyr) const noexcept;

        // Conversion between this planet's clock and the Earth timeline it is scaled from.
        double ToEarthMyr(double planetMyr) const noexcept { return planetMyr / m_scale; }
        double FromEarthMyr(double earthMyr) const noexcept { return earthMyr * m_scale; }

    private:
        double m_planetAgeMyr = 0.0;
        double m_scale = 1.0;
        bool m_biosphere = true;
        std::vector<Epoch> m_epochs;
    };
}

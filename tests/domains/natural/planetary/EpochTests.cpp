#include <doctest/doctest.h>

#include "domains/natural/planetary/geology/PlanetEpochs.h"
#include "support/PlanetGeologyFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

TEST_CASE("EpochTableCoversThePlanetsHistoryWithoutGaps")
{
	for (const double ageMyr : {4540.0, 2270.0, 9000.0})
	{
		const PlanetEpochTable table(ageMyr);
		CHECK(table.PlanetAgeMyr() == doctest::Approx(ageMyr));
		for (const EpochRank rank : {EpochRank::Eon, EpochRank::Era})
		{
			// Eras begin with the Eoarchean; the Hadean has no subdivisions.
			double expectedStart = rank == EpochRank::Eon ? ageMyr : ageMyr * 4000.0 / 4540.0;
			int count = 0;
			for (const Epoch &epoch : table.Epochs())
			{
				if (epoch.rank != rank)
					continue;
				CHECK(epoch.startMyr == doctest::Approx(expectedStart));
				CHECK(epoch.endMyr < epoch.startMyr);
				expectedStart = epoch.endMyr;
				++count;
			}
			// Eons and eras tile the whole history from the planet's formation to
			// now.
			if (rank == EpochRank::Eon)
				CHECK(count == 4);
			CHECK(expectedStart == doctest::Approx(0.0).epsilon(1e-9));
		}
	}

	const PlanetEpochTable earth(4540.0);
	REQUIRE(earth.EpochAt(0.0, EpochRank::Period) != nullptr);
	CHECK(earth.EpochAt(0.0, EpochRank::Period)->name == "Quaternary");
	CHECK(earth.EpochAt(100.0, EpochRank::Period)->name == "Cretaceous");
	CHECK(earth.EpochAt(66.0, EpochRank::Era)->name == "Cenozoic"); // a boundary belongs to the younger unit
	CHECK(earth.EpochAt(300.0, EpochRank::Era)->name == "Paleozoic");
	CHECK(earth.EpochAt(3000.0, EpochRank::Eon)->name == "Archean");
	CHECK(earth.EpochAt(3000.0, EpochRank::Period) == nullptr); // no periods before the Siderian
	CHECK(earth.EpochAt(5000.0, EpochRank::Eon) == nullptr);
	CHECK(earth.EpochAt(-1.0, EpochRank::Eon) == nullptr);

	// The same sequence fits a planet half as old.
	const PlanetEpochTable young(2270.0);
	CHECK(young.EpochAt(50.0, EpochRank::Period)->name == "Cretaceous");
	CHECK(young.EpochAt(1500.0, EpochRank::Eon)->name == "Archean");
}

TEST_CASE("BiosphereStatesGateGeologicalContent")
{
	const PlanetEpochTable earth(4540.0);

	const BiosphereState archean = earth.BiosphereAt(3500.0);
	CHECK_FALSE(archean.freeOxygen);
	CHECK_FALSE(archean.landPlants);

	const BiosphereState bifEra = earth.BiosphereAt(2000.0);
	CHECK(bifEra.freeOxygen);
	CHECK(bifEra.bandedIronFormations);
	CHECK_FALSE(bifEra.landPlants);
	CHECK_FALSE(earth.BiosphereAt(2600.0).freeOxygen);
	CHECK(earth.BiosphereAt(2600.0).bandedIronFormations);
	CHECK_FALSE(earth.BiosphereAt(1500.0).bandedIronFormations);

	const BiosphereState carboniferous = earth.BiosphereAt(320.0);
	CHECK(carboniferous.landPlants);
	CHECK(carboniferous.organicRichMarineSediments);
	CHECK_FALSE(carboniferous.calcareousPlankton);

	const BiosphereState cretaceous = earth.BiosphereAt(100.0);
	CHECK(cretaceous.calcareousPlankton);
	CHECK_FALSE(cretaceous.bandedIronFormations);

	// Each milestone, once reached, stays reached.
	bool oxygen = false, plants = false, plankton = false, organic = false;
	for (double age = 4540.0; age >= 0.0; age -= 5.0)
	{
		const BiosphereState state = earth.BiosphereAt(age);
		CHECK((!oxygen || state.freeOxygen));
		CHECK((!plants || state.landPlants));
		CHECK((!plankton || state.calcareousPlankton));
		CHECK((!organic || state.organicRichMarineSediments));
		oxygen = oxygen || state.freeOxygen;
		plants = plants || state.landPlants;
		plankton = plankton || state.calcareousPlankton;
		organic = organic || state.organicRichMarineSediments;
	}

	// Milestones scale with the planet's age; a lifeless scenario has none of
	// them.
	const PlanetEpochTable old(9080.0);
	CHECK(old.BiosphereAt(4900.0).freeOxygen == false);
	CHECK(old.BiosphereAt(4799.0).freeOxygen == true);
	CHECK(old.BiosphereAt(4000.0).freeOxygen == true);
	const PlanetEpochTable lifeless(4540.0, false);
	const BiosphereState none = lifeless.BiosphereAt(0.0);
	CHECK_FALSE(none.freeOxygen);
	CHECK_FALSE(none.bandedIronFormations);
	CHECK_FALSE(none.landPlants);
	CHECK_FALSE(none.organicRichMarineSediments);
	CHECK_FALSE(none.calcareousPlankton);
	CHECK(lifeless.EpochAt(100.0, EpochRank::Period) != nullptr);
}

TEST_CASE("PlanetMapExposesItsEpochTable")
{
	const PlanetMap &map = SharedMap();
	CHECK(map.GetEpochs().PlanetAgeMyr() == doctest::Approx(map.GetClimateInputs().planetAgeYears / 1.0e6));
	CHECK(map.GetEpochs().HasBiosphere());
	CHECK(kPlanetGeologyDataVersion >= 1);
}

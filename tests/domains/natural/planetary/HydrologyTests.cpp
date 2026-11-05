#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/internal/PlanetMapHydrologyHelpers.h"
#include "support/Require.h"

// --------------------------------------------------------------------------
// Robustness tests: edge-case planet configurations.
// --------------------------------------------------------------------------
TEST_CASE("SyntheticFullyLandDrainageFallback")
{
	constexpr int latitudeBands = 5;
	constexpr int longitudeSegments = 8;
	const std::size_t cellCount = static_cast<std::size_t>(latitudeBands) * longitudeSegments;
	std::vector<double> elevations(cellCount);
	std::vector<bool> oceanCells(cellCount, false);
	std::vector<double> localRunoff(cellCount);
	std::vector<double> localRunoffVolume(cellCount);

	for (std::size_t index = 0; index < cellCount; ++index)
	{
		elevations[index] = 0.1 + static_cast<double>((index * 7) % 17) / 20.0;
		localRunoff[index] = 1.0 + static_cast<double>(index % 5);
		localRunoffVolume[index] = localRunoff[index] * 0.01;
	}

	// Guarantee a fully land-covered map with no ocean cells at all.
	for (std::size_t index = 0; index < cellCount; ++index)
	{
		oceanCells[index] = false;
	}

	const auto network = Infinitesia::GenerationDetail::BuildDrainageNetwork(
	    latitudeBands, longitudeSegments, elevations, oceanCells, localRunoff, localRunoffVolume);

	Require(std::none_of(oceanCells.begin(), oceanCells.end(), [](bool isOcean) { return isOcean; }),
	        "Synthetic fallback map must contain no ocean cells.");
	Require(network.usedLandFallback, "Fully land-covered map did not use the Priority-Flood fallback.");

	const std::size_t minimumIndex = static_cast<std::size_t>(
	    std::distance(elevations.begin(), std::min_element(elevations.begin(), elevations.end())));
	for (std::size_t cell = 0; cell < cellCount; ++cell)
	{
		if (cell == minimumIndex)
		{
			Require(network.flowDirections[cell] == -1,
			        "Global minimum of a fully land-covered map must be the drainage "
			        "sink.");
		}
		else
		{
			Require(network.flowDirections[cell] >= 0,
			        "Land cell in a fully land-covered map has no downstream route.");
		}

		std::size_t downstream = cell;
		std::size_t steps = 0;
		while (network.flowDirections[downstream] >= 0 && steps < cellCount)
		{
			downstream = static_cast<std::size_t>(network.flowDirections[downstream]);
			++steps;
		}
		Require(downstream == minimumIndex && steps < cellCount,
		        "Land drainage path does not terminate at the global minimum.");
	}
}

TEST_CASE("DrainageAccumulationIsMonotonicAlongFlowGraph")
{
	constexpr int latitudeBands = 5;
	constexpr int longitudeSegments = 8;
	const std::size_t cellCount = static_cast<std::size_t>(latitudeBands) * longitudeSegments;
	std::vector<double> elevations(cellCount);
	std::vector<bool> oceanCells(cellCount, false);
	std::vector<double> localRunoff(cellCount);
	std::vector<double> localRunoffVolume(cellCount);

	for (std::size_t index = 0; index < cellCount; ++index)
	{
		elevations[index] = static_cast<double>((index * 11) % 23) / 10.0;
		localRunoff[index] = 1.0 + static_cast<double>(index % 3);
		localRunoffVolume[index] = localRunoff[index] * 0.025;
	}
	oceanCells[3] = true;
	oceanCells[latitudeBands * longitudeSegments - 5] = true;

	const auto network = Infinitesia::GenerationDetail::BuildDrainageNetwork(
	    latitudeBands, longitudeSegments, elevations, oceanCells, localRunoff, localRunoffVolume);

	Require(!network.usedLandFallback, "Drainage graph test with ocean outlets unexpectedly used the land "
	                                   "fallback.");
	for (std::size_t cell = 0; cell < cellCount; ++cell)
	{
		if (oceanCells[cell])
			continue;
		const int downstreamIndex = network.flowDirections[cell];
		if (downstreamIndex < 0)
			continue;

		const std::size_t downstream = static_cast<std::size_t>(downstreamIndex);
		Require(network.filledElevations[downstream] < network.filledElevations[cell] - 1e-18,
		        "Drainage edge does not descend in the filled elevation graph.");
		Require(network.normalizedAccumulation[downstream] + 1e-12 >= network.normalizedAccumulation[cell],
		        "Normalized drainage accumulation decreased downstream.");
		Require(network.volumeAccumulation[downstream] + 1e-12 >= network.volumeAccumulation[cell],
		        "Runoff volume accumulation decreased downstream.");
	}
}

// Water must be conserved across the whole drainage network: every unit of
// land runoff ends up either in the ocean or in a land sink, never lost or
// duplicated.  Without this the balance is only verified inside lakes.
TEST_CASE("DrainageNetworkConservesWaterAtPlanetScale")
{
	constexpr int latitudeBands = 90;
	constexpr int longitudeSegments = 180;
	const std::size_t cellCount = static_cast<std::size_t>(latitudeBands) * longitudeSegments;

	for (const double seaLevel : {-2.0, 0.0, 0.35, 5.0})
	{
		std::vector<double> elevations(cellCount);
		std::vector<bool> oceanCells(cellCount);
		std::vector<double> localRunoff(cellCount, 0.0);
		std::vector<double> localRunoffVolume(cellCount, 0.0);
		double expectedLandRunoff = 0.0;

		for (int la = 0; la < latitudeBands; ++la)
		{
			// Latitude-dependent weight, like the generator's cell areas.
			const double phi = -std::numbers::pi / 2.0 + (la + 0.5) * std::numbers::pi / latitudeBands;
			const double areaWeight = std::cos(phi);
			for (int lo = 0; lo < longitudeSegments; ++lo)
			{
				const std::size_t index = static_cast<std::size_t>(la) * longitudeSegments + lo;
				// Rough terrain with closed depressions and ridges.
				const double h = std::sin(la * 0.37) * std::cos(lo * 0.11) + 0.6 * std::sin(la * 1.3 + lo * 0.7) +
				                 0.3 * std::sin(static_cast<double>(index % 97));
				elevations[index] = h;
				oceanCells[index] = h < seaLevel - 1.0;
				if (oceanCells[index])
					continue;
				localRunoff[index] = (1.0 + (index % 7)) * areaWeight;
				localRunoffVolume[index] = (0.5 + (index % 11)) * areaWeight;
				expectedLandRunoff += localRunoffVolume[index];
			}
		}

		const auto network = Infinitesia::GenerationDetail::BuildDrainageNetwork(
		    latitudeBands, longitudeSegments, elevations, oceanCells, localRunoff, localRunoffVolume);

		Require(std::isfinite(network.oceanInflowVolume) && network.oceanInflowVolume >= 0.0 &&
		            std::isfinite(network.landSinkVolume) && network.landSinkVolume >= 0.0,
		        "Drainage ocean/sink totals must be finite and non-negative.");
		Require(NearlyEqual(network.oceanInflowVolume + network.landSinkVolume, expectedLandRunoff, 1e-9),
		        "Drainage network does not conserve water: ocean inflow plus "
		        "land sinks differs from total land runoff.");

		if (network.usedLandFallback)
		{
			Require(network.oceanInflowVolume == 0.0, "Ocean-free map reported ocean inflow.");
			Require(NearlyEqual(network.landSinkVolume, expectedLandRunoff, 1e-9),
			        "Ocean-free map must drain everything into its single land sink.");
		}
		else
		{
			// With an ocean present, Priority-Flood leaves no land sinks.
			Require(network.landSinkVolume == 0.0, "Land cell without downstream route on a map with ocean.");
		}

		// Cross-check the totals against per-cell accumulation: the ocean
		// total is exactly what the ocean cells themselves received.
		double oceanCellSum = 0.0;
		for (std::size_t cell = 0; cell < cellCount; ++cell)
		{
			if (oceanCells[cell])
				oceanCellSum += network.volumeAccumulation[cell];
		}
		Require(NearlyEqual(oceanCellSum, network.oceanInflowVolume, 1e-12) ||
		            (oceanCellSum == 0.0 && network.oceanInflowVolume == 0.0),
		        "Ocean inflow total disagrees with ocean-cell accumulation.");
	}
}

// Whole-planet freshwater balance after lakes: runoff produced on land must
// reach the ocean or be accounted for as a lake loss / dry-basin loss.
TEST_CASE("PlanetWaterBalanceIncludingLakes")
{
	Infinitesia::PlanetParameters params;
	params.latitudeBands = 45;
	params.longitudeSegments = 90;
	bool sawLake = false;
	for (std::uint32_t seed = 0xbeef0000u; seed < 0xbeef0008u; ++seed)
	{
		params.seed = seed;
		const Infinitesia::PlanetMap map(params);
		const auto &b = map.GetWaterBalance();

		double lakeLossFromCells = 0.0;
		for (int la = 0; la < map.GetLatitudeBandCount(); ++la)
		{
			for (int lo = 0; lo < map.GetLongitudeSegmentCount(); ++lo)
			{
				const auto &r = map.GetRegion(la, lo);
				if (!r.isLake)
					continue;
				sawLake = true;
				lakeLossFromCells += r.lakeAnnualEvaporationKm3 + r.lakeAnnualInfiltrationKm3;
			}
		}

		Require(b.landRunoffKm3 >= 0.0 && b.oceanDischargeKm3 >= 0.0 && b.landSinkKm3 >= 0.0 &&
		            b.lakeLossKm3 >= -1e-9 && b.drySinkLossKm3 >= 0.0,
		        "Planet water-balance terms must be non-negative.");
		const double supply = b.landRunoffKm3 + b.lakePrecipitationGainKm3;
		const double sinks = b.lakeLossKm3 + b.drySinkLossKm3;
		Require(std::abs(b.oceanDischargeKm3 + b.landSinkKm3 - (supply - sinks)) <= 1e-9 * std::max(1.0, supply),
		        "Planet water is not conserved: ocean discharge + land sinks "
		        "differs from runoff plus lake gain minus losses.");
		Require(b.oceanDischargeKm3 <= supply + 1e-9 * std::max(1.0, supply),
		        "Ocean receives more water than the land produced.");
		// The ledger's lake loss must agree with the per-lake evaporation and
		// infiltration published on the cells.
		Require(std::abs(b.lakeLossKm3 - lakeLossFromCells) <= 1e-6 * std::max(1.0, b.lakeLossKm3),
		        "Planet lake loss disagrees with per-lake evaporation and "
		        "infiltration.");
	}
	Require(sawLake, "Water-balance test never encountered a lake.");
}

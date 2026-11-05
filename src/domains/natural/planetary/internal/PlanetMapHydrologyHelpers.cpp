#include "domains/natural/planetary/internal/PlanetMapHydrologyHelpers.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <utility>

namespace Infinitesia::GenerationDetail
{
DrainageNetwork BuildDrainageNetwork(int latitudeBands, int longitudeSegments, const std::vector<double> &elevations,
                                     const std::vector<bool> &oceanCells,
                                     const std::vector<double> &localNormalizedRunoff,
                                     const std::vector<double> &localRunoffVolume)
{
	if (latitudeBands <= 0 || longitudeSegments <= 0)
		throw std::invalid_argument("Drainage grid dimensions must be positive.");

	const std::size_t cellCount = static_cast<std::size_t>(latitudeBands) * longitudeSegments;
	if (elevations.size() != cellCount || oceanCells.size() != cellCount || localNormalizedRunoff.size() != cellCount ||
	    localRunoffVolume.size() != cellCount)
	{
		throw std::invalid_argument("Drainage input sizes must match grid dimensions.");
	}

	constexpr int dx[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
	constexpr int dy[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
	DrainageNetwork network;
	network.filledElevations = elevations;

	std::vector<bool> processed(cellCount, false);
	using HeapEntry = std::pair<double, int>;
	std::priority_queue<HeapEntry, std::vector<HeapEntry>, std::greater<HeapEntry>> heap;

	for (std::size_t index = 0; index < cellCount; ++index)
	{
		if (!oceanCells[index])
			continue;
		processed[index] = true;
		heap.emplace(network.filledElevations[index], static_cast<int>(index));
	}

	if (heap.empty())
	{
		network.usedLandFallback = true;
		const auto minimum = std::min_element(network.filledElevations.begin(), network.filledElevations.end());
		const int minIndex = static_cast<int>(std::distance(network.filledElevations.begin(), minimum));
		processed[static_cast<std::size_t>(minIndex)] = true;
		heap.emplace(*minimum, minIndex);
	}

	constexpr double kEpsilon = 1e-7;
	while (!heap.empty())
	{
		const auto [elevation, cellIndex] = heap.top();
		heap.pop();

		const int latitude = cellIndex / longitudeSegments;
		const int longitude = cellIndex % longitudeSegments;
		for (int direction = 0; direction < 8; ++direction)
		{
			const int neighborLatitude = latitude + dx[direction];
			const int neighborLongitude = ((longitude + dy[direction]) + longitudeSegments) % longitudeSegments;
			if (neighborLatitude < 0 || neighborLatitude >= latitudeBands)
				continue;

			const std::size_t neighborIndex =
			    static_cast<std::size_t>(neighborLatitude) * longitudeSegments + neighborLongitude;
			if (processed[neighborIndex])
				continue;

			const double filledElevation = std::max(network.filledElevations[neighborIndex], elevation + kEpsilon);
			network.filledElevations[neighborIndex] = filledElevation;
			processed[neighborIndex] = true;
			heap.emplace(filledElevation, static_cast<int>(neighborIndex));
		}
	}

	// Flow goes along the steepest slope: elevation drop divided by the
	// great-circle distance between cell centers (unit sphere; the radius
	// cancels when comparing slopes). Cell centers sit at mid-band latitude.
	const double deltaLat = std::numbers::pi / latitudeBands;
	const double deltaLon = (2.0 * std::numbers::pi) / longitudeSegments;
	std::vector<double> bandLatitude(static_cast<std::size_t>(latitudeBands));
	for (int band = 0; band < latitudeBands; ++band)
	{
		bandLatitude[static_cast<std::size_t>(band)] = -std::numbers::pi / 2.0 + (band + 0.5) * deltaLat;
	}
	const auto centerDistance = [&](int latA, int latB, int lonOffset) {
		const double phiA = bandLatitude[static_cast<std::size_t>(latA)];
		const double phiB = bandLatitude[static_cast<std::size_t>(latB)];
		const double sinHalfLat = std::sin((phiB - phiA) * 0.5);
		const double sinHalfLon = std::sin(lonOffset * deltaLon * 0.5);
		const double h = sinHalfLat * sinHalfLat + std::cos(phiA) * std::cos(phiB) * sinHalfLon * sinHalfLon;
		return 2.0 * std::asin(std::sqrt(std::min(1.0, h)));
	};

	network.flowDirections.assign(cellCount, -1);
	for (int latitude = 0; latitude < latitudeBands; ++latitude)
	{
		for (int longitude = 0; longitude < longitudeSegments; ++longitude)
		{
			const std::size_t index = static_cast<std::size_t>(latitude) * longitudeSegments + longitude;
			if (oceanCells[index])
				continue;

			const double currentElevation = network.filledElevations[index];
			double steepestSlope = 0.0;
			int bestNeighbor = -1;
			for (int direction = 0; direction < 8; ++direction)
			{
				const int neighborLatitude = latitude + dx[direction];
				const int neighborLongitude = ((longitude + dy[direction]) + longitudeSegments) % longitudeSegments;
				if (neighborLatitude < 0 || neighborLatitude >= latitudeBands)
					continue;

				const std::size_t neighborIndex =
				    static_cast<std::size_t>(neighborLatitude) * longitudeSegments + neighborLongitude;
				const double drop = currentElevation - network.filledElevations[neighborIndex];
				if (drop <= 0.0)
					continue;

				const double distance = centerDistance(latitude, neighborLatitude, dy[direction]);
				if (distance <= 0.0)
					continue;

				const double slope = drop / distance;
				if (slope > steepestSlope)
				{
					steepestSlope = slope;
					bestNeighbor = static_cast<int>(neighborIndex);
				}
			}
			network.flowDirections[index] = bestNeighbor;
		}
	}

	std::vector<int> order(cellCount);
	std::iota(order.begin(), order.end(), 0);
	std::sort(order.begin(), order.end(), [&](int first, int second) {
		return network.filledElevations[static_cast<std::size_t>(first)] >
		       network.filledElevations[static_cast<std::size_t>(second)];
	});

	network.processingOrder = order;

	network.normalizedAccumulation.assign(cellCount, 0.0);
	network.volumeAccumulation.assign(cellCount, 0.0);
	for (std::size_t cell = 0; cell < cellCount; ++cell)
	{
		if (oceanCells[cell])
			continue;
		network.normalizedAccumulation[cell] = localNormalizedRunoff[cell];
		network.volumeAccumulation[cell] = localRunoffVolume[cell];
	}

	for (const int index : order)
	{
		const std::size_t cell = static_cast<std::size_t>(index);
		if (oceanCells[cell])
			continue;

		const int target = network.flowDirections[cell];
		if (target < 0)
			continue;

		const std::size_t downstream = static_cast<std::size_t>(target);
		network.normalizedAccumulation[downstream] += network.normalizedAccumulation[cell];
		network.volumeAccumulation[downstream] += network.volumeAccumulation[cell];
	}

	for (std::size_t cell = 0; cell < cellCount; ++cell)
	{
		if (oceanCells[cell])
			network.oceanInflowVolume += network.volumeAccumulation[cell];
		else if (network.flowDirections[cell] < 0)
			network.landSinkVolume += network.volumeAccumulation[cell];
	}

	return network;
}
} // namespace Infinitesia::GenerationDetail

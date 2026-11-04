#include "domains/natural/planetary/internal/GridDistance.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <queue>
#include <utility>

namespace Infinitesia::GenerationDetail {
std::vector<double> DistanceToSourcesKm(const std::vector<bool>& isSource,
                                        int latitudeBands,
                                        int longitudeSegments,
                                        double radiusKm) {
  const std::size_t cellCount = static_cast<std::size_t>(latitudeBands) *
                                static_cast<std::size_t>(longitudeSegments);
  std::vector<double> distance(cellCount,
                               std::numeric_limits<double>::infinity());

  using QueueEntry = std::pair<double, int>;
  std::priority_queue<QueueEntry, std::vector<QueueEntry>,
                      std::greater<QueueEntry>>
      frontier;
  for (std::size_t index = 0; index < cellCount && index < isSource.size();
       ++index) {
    if (isSource[index]) {
      distance[index] = 0.0;
      frontier.emplace(0.0, static_cast<int>(index));
    }
  }

  const double latitudeStepRadians = std::numbers::pi / latitudeBands;
  const double longitudeStepRadians =
      2.0 * std::numbers::pi / longitudeSegments;
  const auto latitudeOf = [latitudeStepRadians](int row) {
    return (static_cast<double>(row) + 0.5) * latitudeStepRadians -
           std::numbers::pi * 0.5;
  };

  constexpr int kDeltaLatitude[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
  constexpr int kDeltaLongitude[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

  // A step's length depends only on the row and the direction, so the
  // trigonometry is done once per row instead of once per relaxation.
  std::vector<double> edgeKm(static_cast<std::size_t>(latitudeBands) * 8, 0.0);
  for (int row = 0; row < latitudeBands; ++row) {
    const double latitude = latitudeOf(row);
    for (int direction = 0; direction < 8; ++direction) {
      const int nextRow = row + kDeltaLatitude[direction];
      if (nextRow < 0 || nextRow >= latitudeBands) {
        continue;
      }
      const double nextLatitude = latitudeOf(nextRow);
      const double halfLatitude = std::sin(0.5 * (nextLatitude - latitude));
      const double halfLongitude =
          std::sin(0.5 * kDeltaLongitude[direction] * longitudeStepRadians);
      const double haversine = halfLatitude * halfLatitude +
                               std::cos(latitude) * std::cos(nextLatitude) *
                                   halfLongitude * halfLongitude;
      edgeKm[static_cast<std::size_t>(row) * 8 + direction] =
          2.0 * radiusKm *
          std::asin(std::sqrt(std::clamp(haversine, 0.0, 1.0)));
    }
  }

  while (!frontier.empty()) {
    const auto [distanceToCell, index] = frontier.top();
    frontier.pop();
    if (distanceToCell != distance[static_cast<std::size_t>(index)]) {
      continue;
    }

    const int row = index / longitudeSegments;
    const int column = index % longitudeSegments;

    for (int direction = 0; direction < 8; ++direction) {
      const int nextRow = row + kDeltaLatitude[direction];
      if (nextRow < 0 || nextRow >= latitudeBands) {
        continue;
      }
      const int nextColumn =
          (column + kDeltaLongitude[direction] + longitudeSegments) %
          longitudeSegments;
      const std::size_t nextIndex =
          static_cast<std::size_t>(nextRow) * longitudeSegments + nextColumn;

      const double candidate =
          distanceToCell +
          edgeKm[static_cast<std::size_t>(row) * 8 + direction];
      if (candidate < distance[nextIndex]) {
        distance[nextIndex] = candidate;
        frontier.emplace(candidate, static_cast<int>(nextIndex));
      }
    }
  }
  return distance;
}
}  // namespace Infinitesia::GenerationDetail

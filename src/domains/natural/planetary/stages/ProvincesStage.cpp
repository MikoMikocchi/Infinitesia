#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <queue>
#include <vector>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/internal/GenerationMath.h"
#include "domains/natural/planetary/stages/PlanetStages.h"

namespace Infinitesia {
using namespace GenerationDetail;

namespace {
constexpr double kEarthAgeMyr = 4540.0;

// Components smaller than this are absorbed by their longest-bordering
// neighbour.
constexpr int kMinProvinceCells = 6;
constexpr int kMajorityPasses = 2;

constexpr std::uint32_t kAncientBeltSalt = 0xa11c1e47u;
constexpr std::uint32_t kMetamorphicSalt = 0x3e7a40f1u;
constexpr std::uint32_t kEventAgeSalt = 0xe7e47a6eu;

double Near(double distanceKm, double widthKm) {
  return 1.0 - SmootherStep(distanceKm / widthKm);
}

// 1 inside [full0, full1], rising over [zero0, full0] and falling over [full1,
// zero1].
double Window(double x, double zero0, double full0, double full1,
              double zero1) {
  return SmootherStep((x - zero0) / (full0 - zero0)) *
         (1.0 - SmootherStep((x - full1) / (zero1 - full1)));
}

bool IsSubduction(PlateBoundaryKind kind) {
  return kind == PlateBoundaryKind::OceanicSubduction ||
         kind == PlateBoundaryKind::ContinentalSubduction;
}

// Later entries lose ties against earlier ones (the priority order of the
// types).
double Priority(ProvinceType type) {
  return 1.0 - 0.02 * static_cast<int>(type);
}

// How strongly the cell belongs to each province type, 0..1. Each term is a
// product of smooth windows over the planet fields, so a type fades out over
// a belt instead of switching at a threshold.
std::array<double, kProvinceTypeCount> Score(const PlanetRegion& c,
                                             double planetAgeMyr,
                                             double distanceToOceanKm,
                                             std::uint32_t seed) {
  std::array<double, kProvinceTypeCount> score{};
  const auto set = [&score](ProvinceType type, double value) {
    score[static_cast<std::size_t>(type)] =
        std::clamp(value, 0.0, 1.0) * Priority(type);
  };

  const bool continental = c.continentalCrustFraction > 0.5;
  const PlateBoundaryKind kind = c.boundarySubtype;
  const double distance = c.distanceToBoundaryKm;
  const double sediment = c.sedimentThicknessKm;

  if (!continental) {
    const double youngCrust = 1.0 - SmootherStep((c.crustAge - 0.10) / 0.10);
    const double ridge =
        kind == PlateBoundaryKind::MidOceanRidge ? Near(distance, 200.0) : 0.0;
    set(ProvinceType::OceanicRidge, std::max(youngCrust, ridge));
    set(ProvinceType::AbyssalPlain, 0.30);
    if (IsSubduction(kind) && c.isSubductingPlate) {
      set(ProvinceType::Trench, Near(distance, 220.0));
    }
    if (kind == PlateBoundaryKind::OceanicSubduction && !c.isSubductingPlate) {
      set(ProvinceType::IslandArc, Near(distance, 350.0));
    }
  } else {
    const double ageFraction =
        std::clamp(c.crustAgeMyr / planetAgeMyr, 0.0, 1.0);
    const double awayFromBoundary = 1.0 - Near(distance, 400.0);

    if (kind == PlateBoundaryKind::ContinentalSubduction) {
      set(ProvinceType::ContinentalArc, Near(distance, 450.0));
    }
    if (kind == PlateBoundaryKind::ContinentalCollision) {
      set(ProvinceType::CollisionOrogen, Near(distance, 380.0));
    }
    if (kind == PlateBoundaryKind::ContinentalRift) {
      set(ProvinceType::RiftSystem, Near(distance, 250.0));
    }

    // Relict belts: ridged noise marks sutures and fold belts of past
    // collisions. A prior, not a reconstruction; G9 replaces it with history.
    const double ridged =
        1.0 - std::abs(2.0 * SphericalFbm(c.latitudeDegrees, c.longitudeDegrees,
                                          seed ^ kAncientBeltSalt, 2, 3) -
                       1.0);
    const double belt = SmootherStep((ridged - 0.90) / 0.05);
    set(ProvinceType::AncientOrogen,
        belt * SmootherStep((ageFraction - 0.15) / 0.15) *
            (1.0 - Near(distance, 500.0)));

    const double blob =
        SmootherStep((SphericalFbm(c.latitudeDegrees, c.longitudeDegrees,
                                   seed ^ kMetamorphicSalt, 3, 4) -
                      0.60) /
                     0.06);
    set(ProvinceType::MetamorphicComplex,
        blob * SmootherStep((ageFraction - 0.25) / 0.15) *
            (1.0 - Near(distance, 500.0)));

    set(ProvinceType::Craton, SmootherStep((ageFraction - 0.60) / 0.12) *
                                  (1.0 - SmootherStep((sediment - 0.6) / 0.9)) *
                                  awayFromBoundary);
    // Interior continental crust with a modest cover is platform by default.
    set(ProvinceType::Platform,
        std::max(0.30, SmootherStep((ageFraction - 0.35) / 0.20) *
                           Window(sediment, 0.4, 1.0, 3.0, 4.5) *
                           awayFromBoundary));
    // Thick cover away from the coast; the coastal wedge is the margin's.
    set(ProvinceType::SedimentaryBasin,
        SmootherStep((sediment - 3.0) / 1.5) * (1.0 - Near(distance, 250.0)) *
            SmootherStep(distanceToOceanKm / 250.0));
    set(ProvinceType::PassiveMargin,
        (1.0 - SmootherStep(distanceToOceanKm / 350.0)) *
            (1.0 - Near(distance, 300.0)));
  }

  // Plume provinces cross the oceanic/continental divide.
  set(ProvinceType::LargeIgneousProvince,
      SmootherStep((c.hotspotInfluence - 0.25) / 0.35));
  return score;
}

ProvinceType BestType(const std::array<double, kProvinceTypeCount>& score) {
  std::size_t best = 0;
  for (std::size_t i = 1; i < score.size(); ++i) {
    if (score[i] > score[best]) best = i;
  }
  return static_cast<ProvinceType>(best);
}

struct EventModel {
  double minMyr;  // on the Earth timeline
  double maxMyr;
  double erosionKmPerMyr;  // unroofing rate while the province exists
  double maxDepthKm;       // erosion stops at the basement's own thickness
  bool fromCrustAge;       // the main event is the crust's own formation
};

EventModel ModelFor(ProvinceType type) {
  switch (type) {
    case ProvinceType::OceanicRidge:
      return {0.0, 0.0, 0.0, 0.0, true};
    case ProvinceType::AbyssalPlain:
      return {0.0, 0.0, 0.0, 0.0, true};
    case ProvinceType::Trench:
      return {20.0, 150.0, 0.0, 0.0, false};
    case ProvinceType::IslandArc:
      return {20.0, 150.0, 0.02, 6.0, false};
    case ProvinceType::ContinentalArc:
      return {30.0, 220.0, 0.03, 10.0, false};
    case ProvinceType::CollisionOrogen:
      return {20.0, 250.0, 0.04, 25.0, false};
    case ProvinceType::AncientOrogen:
      return {350.0, 2000.0, 0.015, 30.0, false};
    case ProvinceType::RiftSystem:
      return {10.0, 180.0, 0.005, 4.0, false};
    case ProvinceType::LargeIgneousProvince:
      return {30.0, 700.0, 0.01, 4.0, false};
    case ProvinceType::Craton:
      return {0.0, 0.0, 0.01, 25.0, true};
    case ProvinceType::Platform:
      return {100.0, 900.0, 0.002, 3.0, false};
    case ProvinceType::SedimentaryBasin:
      return {30.0, 500.0, 0.002, 2.0, false};
    case ProvinceType::PassiveMargin:
      return {80.0, 300.0, 0.002, 2.0, false};
    case ProvinceType::MetamorphicComplex:
      return {500.0, 2500.0, 0.012, 28.0, false};
  }
  return {};
}
}  // namespace

void RunProvinces(PlanetBuild& build) {
  const Field<double>& distanceToOcean =
      build.data.layers.Require<double>(kDistanceToOceanLayer);
  const int latB = build.LatBands();
  const int lonS = build.LonSegs();
  const std::size_t cellCount = static_cast<std::size_t>(latB) * lonS;
  const std::uint32_t seed = build.data.planet.GetSeed();
  const double radiusKm = build.data.planet.GetRadius() / 1000.0;
  const double planetAgeMyr =
      std::max(build.data.climate.planetAgeYears / 1.0e6, 1.0);

  // ---- 1. best-scoring type per cell
  std::vector<ProvinceType> type(cellCount);
  for (int la = 0; la < latB; ++la) {
    for (int lo = 0; lo < lonS; ++lo) {
      const std::size_t index = static_cast<std::size_t>(la) * lonS + lo;
      type[index] = BestType(Score(build.Cell(la, lo), planetAgeMyr,
                                   distanceToOcean.At(la, lo), seed));
    }
  }

  // ---- 2. majority filter: removes single-cell speckle, keeps real boundaries
  for (int pass = 0; pass < kMajorityPasses; ++pass) {
    std::vector<ProvinceType> filtered = type;
    for (int la = 0; la < latB; ++la) {
      for (int lo = 0; lo < lonS; ++lo) {
        const std::size_t index = static_cast<std::size_t>(la) * lonS + lo;
        std::array<int, kProvinceTypeCount> votes{};
        for (int dla = -1; dla <= 1; ++dla) {
          for (int dlo = -1; dlo <= 1; ++dlo) {
            const int nla = std::clamp(la + dla, 0, latB - 1);
            const int nlo = (lo + dlo + lonS) % lonS;
            const bool centre = dla == 0 && dlo == 0;
            votes[static_cast<std::size_t>(
                type[static_cast<std::size_t>(nla) * lonS + nlo])] +=
                centre ? 2 : 1;
          }
        }
        std::size_t best = static_cast<std::size_t>(type[index]);
        for (std::size_t t = 0; t < votes.size(); ++t) {
          if (votes[t] > votes[best]) best = t;
        }
        filtered[index] = static_cast<ProvinceType>(best);
      }
    }
    type = std::move(filtered);
  }

  // ---- 3. connected components, small ones merged into their neighbours
  std::vector<std::uint32_t> label(cellCount, 0);
  int componentCount = 0;
  std::vector<int> componentSize;

  const auto labelComponents = [&]() {
    std::fill(label.begin(), label.end(), 0u);
    componentSize.assign(1, 0);
    componentCount = 0;
    std::vector<std::size_t> stack;
    for (std::size_t start = 0; start < cellCount; ++start) {
      if (label[start] != 0) continue;
      const auto id = static_cast<std::uint32_t>(++componentCount);
      componentSize.push_back(0);
      stack.push_back(start);
      label[start] = id;
      while (!stack.empty()) {
        const std::size_t index = stack.back();
        stack.pop_back();
        ++componentSize[id];
        const int la = static_cast<int>(index / lonS);
        const int lo = static_cast<int>(index % lonS);
        for (int dla = -1; dla <= 1; ++dla) {
          for (int dlo = -1; dlo <= 1; ++dlo) {
            const int nla = la + dla;
            if ((dla == 0 && dlo == 0) || nla < 0 || nla >= latB) continue;
            const std::size_t next =
                static_cast<std::size_t>(nla) * lonS + (lo + dlo + lonS) % lonS;
            if (label[next] == 0 && type[next] == type[index]) {
              label[next] = id;
              stack.push_back(next);
            }
          }
        }
      }
    }
  };

  for (int round = 0; round < 6; ++round) {
    labelComponents();
    std::vector<ProvinceType> typeOf(
        static_cast<std::size_t>(componentCount) + 1,
        ProvinceType::AbyssalPlain);
    std::vector<std::vector<std::size_t>> smallMembers(
        static_cast<std::size_t>(componentCount) + 1);
    for (std::size_t index = 0; index < cellCount; ++index) {
      typeOf[label[index]] = type[index];
      if (componentSize[label[index]] < kMinProvinceCells)
        smallMembers[label[index]].push_back(index);
    }

    // Each small component takes the type of the neighbour it borders most.
    std::vector<std::pair<std::size_t, ProvinceType>> changes;
    for (int id = 1; id <= componentCount; ++id) {
      const std::vector<std::size_t>& members =
          smallMembers[static_cast<std::size_t>(id)];
      if (members.empty()) continue;
      std::vector<std::pair<std::uint32_t, int>> contacts;
      for (const std::size_t index : members) {
        const int la = static_cast<int>(index / lonS);
        const int lo = static_cast<int>(index % lonS);
        for (int dla = -1; dla <= 1; ++dla) {
          for (int dlo = -1; dlo <= 1; ++dlo) {
            const int nla = la + dla;
            if ((dla == 0 && dlo == 0) || nla < 0 || nla >= latB) continue;
            const std::uint32_t other =
                label[static_cast<std::size_t>(nla) * lonS +
                      (lo + dlo + lonS) % lonS];
            if (other == static_cast<std::uint32_t>(id)) continue;
            auto found = std::find_if(
                contacts.begin(), contacts.end(),
                [other](const auto& entry) { return entry.first == other; });
            if (found == contacts.end())
              contacts.emplace_back(other, 1);
            else
              ++found->second;
          }
        }
      }
      if (contacts.empty()) continue;
      std::sort(contacts.begin(), contacts.end(),
                [](const auto& first, const auto& second) {
                  return first.second != second.second
                             ? first.second > second.second
                             : first.first < second.first;
                });
      const ProvinceType takenType = typeOf[contacts.front().first];
      for (const std::size_t index : members)
        changes.emplace_back(index, takenType);
    }
    if (changes.empty()) break;
    for (const auto& [index, takenType] : changes) type[index] = takenType;
  }
  labelComponents();

  // ---- 4. province table
  build.data.provinces.assign(static_cast<std::size_t>(componentCount),
                              Province{});
  struct Accumulator {
    double x = 0.0, y = 0.0, z = 0.0;
    double landArea = 0.0;
    double crustAgeArea = 0.0;
    double sedimentArea = 0.0;
    double motionCos =
        0.0;  // plate motion axis, doubled-angle sums weighted by speed
    double motionSin = 0.0;
  };
  std::vector<Accumulator> sums(static_cast<std::size_t>(componentCount));

  const double latStepRadians = std::numbers::pi / latB;
  const double lonStepRadians = 2.0 * std::numbers::pi / lonS;
  for (int la = 0; la < latB; ++la) {
    for (int lo = 0; lo < lonS; ++lo) {
      const std::size_t index = static_cast<std::size_t>(la) * lonS + lo;
      auto& c = build.Cell(la, lo);
      c.provinceId = label[index];
      Province& province = build.data.provinces[label[index] - 1];
      Accumulator& sum = sums[label[index] - 1];

      const double latitude = c.latitudeDegrees * std::numbers::pi / 180.0;
      const double area = radiusKm * radiusKm * latStepRadians *
                          lonStepRadians * std::cos(latitude);
      const Vec3d point = LatLonToCart(c.latitudeDegrees, c.longitudeDegrees);
      province.id = label[index];
      province.type = type[index];
      province.cellCount += 1;
      province.areaKm2 += area;
      sum.x += point.x * area;
      sum.y += point.y * area;
      sum.z += point.z * area;
      sum.landArea += c.isOcean ? 0.0 : area;
      sum.crustAgeArea += c.crustAgeMyr * area;
      sum.sedimentArea += c.sedimentThicknessKm * area;
      const double motionEast = c.boundaryRelativeVelocityEastCmPerYear;
      const double motionNorth = c.boundaryRelativeVelocityNorthCmPerYear;
      const double azimuth = std::atan2(motionEast, motionNorth);
      const double weight = std::hypot(motionEast, motionNorth) * area;
      sum.motionCos += weight * std::cos(2.0 * azimuth);
      sum.motionSin += weight * std::sin(2.0 * azimuth);
    }
  }

  for (Province& province : build.data.provinces) {
    const Accumulator& sum = sums[province.id - 1];
    const double area = std::max(province.areaKm2, 1.0e-9);
    const double length =
        std::sqrt(sum.x * sum.x + sum.y * sum.y + sum.z * sum.z);
    if (length > 1.0e-12) {
      province.centroidLatitudeDegrees =
          std::asin(std::clamp(sum.y / length, -1.0, 1.0)) * 180.0 /
          std::numbers::pi;
      double longitude = std::atan2(sum.z, sum.x) * 180.0 / std::numbers::pi;
      if (longitude < 0.0) longitude += 360.0;
      province.centroidLongitudeDegrees = longitude;
    }
    if (std::hypot(sum.motionCos, sum.motionSin) > 1.0e-9) {
      double axis = 0.5 * std::atan2(sum.motionSin, sum.motionCos) * 180.0 /
                    std::numbers::pi;
      if (axis < 0.0) axis += 180.0;
      province.compressionAzimuthDegrees = axis;
    }
    province.landFraction = sum.landArea / area;
    province.meanCrustAgeMyr = sum.crustAgeArea / area;
    province.meanSedimentThicknessKm = sum.sedimentArea / area;

    // Main-event age and erosion level.
    const EventModel model = ModelFor(province.type);
    double eventAge = province.meanCrustAgeMyr;
    if (!model.fromCrustAge) {
      const double pick = RandUnit(seed ^ kEventAgeSalt, province.id);
      eventAge = (model.minMyr + (model.maxMyr - model.minMyr) * pick) *
                 (planetAgeMyr / kEarthAgeMyr);
    }
    province.eventAgeMyr = std::clamp(eventAge, 0.0, planetAgeMyr);
    province.erosionDepthKm = std::min(
        model.maxDepthKm, model.erosionKmPerMyr * province.eventAgeMyr);
    province.erosionLevel = ClassifyErosionDepth(province.erosionDepthKm);
  }
}
}  // namespace Infinitesia

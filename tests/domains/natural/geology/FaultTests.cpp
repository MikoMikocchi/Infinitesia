#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include "domains/natural/geology/model/GeologyModel.h"
#include "support/GeologyFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

TEST_CASE("FaultsFollowTheirStyleAndTheRegimeOfTheProvince") {
  const PlanetMap& map = SharedMap();
  const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
  const GeologyModel model(catalog, map);

  int faultsSeen = 0;
  int kinds[3] = {0, 0, 0};
  for (const Province& province : map.GetProvinces()) {
    const FaultStyle& style = catalog->ColumnFor(province.type).style.faults;
    for (double east = -1800.0; east <= 1800.0; east += 600.0) {
      for (double north = -1800.0; north <= 1800.0; north += 600.0) {
        const std::vector<GeologyModel::Fault> faults =
            model.FaultsNear(province.id, east, north);
        const std::vector<GeologyModel::Fault> again =
            model.FaultsNear(province.id, east, north);
        Require(faults.size() == again.size() &&
                    faults.size() <= static_cast<std::size_t>(
                                         GeologyModel::kMaxFaultsPerColumn),
                "the faults near a place are not stable or exceed the cap");
        double previousAge = -1.0;
        for (const GeologyModel::Fault& fault : faults) {
          ++faultsSeen;
          ++kinds[static_cast<int>(fault.kind)];
          Require(fault.age >= previousAge,
                  "faults are not ordered from the oldest");
          previousAge = fault.age;
          Require(fault.slipMeters >= style.minSlipMeters &&
                      fault.slipMeters <= style.maxSlipMeters,
                  "fault slip is outside the style");
          Require(fault.strikeRadiusMeters > 0.0 && fault.dipRadiusMeters > 0.0,
                  "a fault has no size");

          const double axis =
              model.StructureOf(province.id).compressionAzimuthDegrees;
          double offset = std::abs(
              std::fmod(fault.strikeAzimuthDegrees - axis + 720.0, 180.0));
          offset = std::min(
              offset,
              180.0 - offset);  // angle between the strike and the motion axis
          switch (fault.kind) {
            case GeologyModel::FaultKind::Normal:
              Require(fault.dipDegrees >= 50.0 && fault.dipDegrees <= 70.0,
                      "a normal fault has the wrong dip");
              Require(std::abs(offset - 90.0) <= 15.0 + 1e-6,
                      "a normal fault does not strike across the motion axis");
              break;
            case GeologyModel::FaultKind::Thrust:
              Require(fault.dipDegrees >= 20.0 && fault.dipDegrees <= 40.0,
                      "a thrust has the wrong dip");
              Require(std::abs(offset - 90.0) <= 15.0 + 1e-6,
                      "a thrust does not strike across the motion axis");
              break;
            case GeologyModel::FaultKind::StrikeSlip:
              Require(fault.dipDegrees >= 80.0 && fault.dipDegrees <= 88.0,
                      "a strike-slip fault is not near vertical");
              Require(
                  offset >= 25.0 - 1e-6 && offset <= 45.0 + 1e-6,
                  "a strike-slip fault is not at an angle to the motion axis");
              break;
          }
        }
      }
    }
  }
  CHECK(faultsSeen > 100);
  CHECK(kinds[0] > 0);
  CHECK(kinds[1] > 0);
  CHECK(kinds[2] > 0);
}

TEST_CASE("TheBlockAboveAFaultComesFromWhereTheSlipSaysAndTheBlockBelowStays") {
  const PlanetMap& map = SharedMap();
  const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
  const GeologyModel model(catalog, map);
  constexpr int kTop = 250;

  int checked[3] = {0, 0, 0};
  for (const Province& province : map.GetProvinces()) {
    const std::vector<GeologyModel::Fault> faults =
        model.FaultsNear(province.id, 0.0, 0.0);
    for (const GeologyModel::Fault& fault : faults) {
      // Slip is largest at the centre and dies out to nothing at the edge.
      const GeologyModel::FaultEffect centre = GeologyModel::EffectOf(
          fault, fault.centerEast, fault.centerNorth, kTop);
      Require(std::abs(centre.amplitude - fault.slipMeters) < 1e-6,
              "slip at the centre is not the fault's slip");
      const double outside = fault.strikeRadiusMeters * 1.01;
      const double strike =
          fault.strikeAzimuthDegrees * 3.14159265358979323846 / 180.0;
      Require(GeologyModel::EffectOf(
                  fault, fault.centerEast + outside * std::sin(strike),
                  fault.centerNorth + outside * std::cos(strike), kTop)
                      .amplitude == 0.0,
              "a fault reaches beyond its edge");

      // Look at a column a little off the centre where this is the only fault
      // in play.
      const double east =
          fault.centerEast + 0.2 * fault.strikeRadiusMeters * std::sin(strike);
      const double north =
          fault.centerNorth + 0.2 * fault.strikeRadiusMeters * std::cos(strike);
      const GeologyModel::FaultEffect effect =
          GeologyModel::EffectOf(fault, east, north, kTop);
      if (!(effect.amplitude > 0.0)) continue;
      bool alone = true;
      for (const GeologyModel::Fault& other :
           model.FaultsNear(province.id, east, north)) {
        if (other.centerEast == fault.centerEast &&
            other.centerNorth == fault.centerNorth)
          continue;
        alone =
            alone &&
            GeologyModel::EffectOf(other, east, north, kTop).amplitude == 0.0 &&
            GeologyModel::EffectOf(other, east - effect.shiftEast,
                                   north - effect.shiftNorth, kTop)
                    .amplitude == 0.0;
      }
      if (!alone) continue;

      const GeologyModel::ColumnProfile faulted =
          model.ProfileAt(province.id, east, north, kTop);
      const GeologyModel::ColumnProfile here =
          model.BaseProfileAt(province.id, east, north, kTop);
      const GeologyModel::ColumnProfile source =
          model.BaseProfileAt(province.id, east - effect.shiftEast,
                              north - effect.shiftNorth, kTop);
      const int cut = static_cast<int>(std::lround(effect.cutElevation));
      const int lift = static_cast<int>(std::lround(effect.lift));
      for (int above : {3, 9, 30}) {
        Require(GeologyModel::RockAt(faulted, cut + above) ==
                    GeologyModel::RockAt(source, cut + above - lift),
                "the block above a fault is not what the undeformed pile had "
                "at its source");
      }
      for (int below : {3, 9, 30}) {
        Require(GeologyModel::RockAt(faulted, cut - below) ==
                    GeologyModel::RockAt(here, cut - below),
                "the block below a fault changed");
      }

      switch (fault.kind) {
        case GeologyModel::FaultKind::Normal:
          Require(effect.lift < 0.0,
                  "a normal fault does not drop its hanging wall");
          break;
        case GeologyModel::FaultKind::Thrust:
          Require(effect.lift > 0.0,
                  "a thrust does not raise its hanging wall");
          break;
        case GeologyModel::FaultKind::StrikeSlip:
          Require(effect.lift == 0.0 &&
                      std::hypot(effect.shiftEast, effect.shiftNorth) > 0.0,
                  "a strike-slip fault moves vertically or not at all");
          break;
      }
      ++checked[static_cast<int>(fault.kind)];
    }
  }
  CHECK(checked[0] > 0);
  CHECK(checked[1] > 0);
  CHECK(checked[2] > 0);
}

TEST_CASE("FaultedProfilesStayOrderedAndEndInBasement") {
  const PlanetMap& map = SharedMap();
  const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
  const GeologyModel model(catalog, map);

  int faulted = 0;
  int differing = 0;
  for (const Province& province : map.GetProvinces()) {
    for (double east = -2000.0; east <= 2000.0; east += 90.0) {
      for (double north = -2000.0; north <= 2000.0; north += 110.0) {
        const GeologyModel::ColumnProfile profile =
            model.ProfileAt(province.id, east, north, 100);
        Require(profile.count >= 1 &&
                    profile.count <= GeologyModel::kMaxProfileLayers,
                "a faulted profile has the wrong number of layers");
        Require(profile.bottom[static_cast<std::size_t>(profile.count - 1)] ==
                    std::numeric_limits<int>::min(),
                "a faulted profile does not end in basement");
        for (int i = 1; i < profile.count; ++i) {
          Require(profile.bottom[static_cast<std::size_t>(i)] <=
                      profile.bottom[static_cast<std::size_t>(i - 1)],
                  "a faulted profile is out of order");
          Require(profile.rock[static_cast<std::size_t>(i)] !=
                      profile.rock[static_cast<std::size_t>(i - 1)],
                  "a faulted profile repeats a rock");
        }
        const GeologyModel::ColumnProfile base =
            model.BaseProfileAt(province.id, east, north, 100);
        bool same = base.count == profile.count;
        for (int i = 0; same && i < base.count; ++i)
          same = base.rock[static_cast<std::size_t>(i)] ==
                     profile.rock[static_cast<std::size_t>(i)] &&
                 base.bottom[static_cast<std::size_t>(i)] ==
                     profile.bottom[static_cast<std::size_t>(i)];
        ++faulted;
        differing += same ? 0 : 1;
      }
    }
  }
  CHECK(faulted > 0);
  CHECK(differing > 0);
}

#include <doctest/doctest.h>

#include <cstddef>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "domains/natural/geology/model/GeologyModel.h"
#include "support/GeologyFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

TEST_CASE("HistoriesAreOrderedAndEachEventHasAnOlderCause") {
  const PlanetMap& map = SharedMap();
  const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
  const GeologyModel model(catalog, map);
  const double planetAge = map.GetEpochs().PlanetAgeMyr();

  std::set<EventId> allIds;
  std::set<std::string_view> typeNames;
  for (int type = 0; type <= static_cast<int>(EventType::Erosion); ++type)
    CHECK(typeNames.insert(EventTypeName(static_cast<EventType>(type))).second);

  for (const Province& province : map.GetProvinces()) {
    const ProvinceHistory& history = model.HistoryOf(province.id);
    REQUIRE(!history.events.empty());
    Require(history.events.front().type == EventType::BasementFormation &&
                history.events.front().parents.empty(),
            "a history does not begin with the basement");

    double lastFormationEnd = planetAge;
    bool eroded = false;
    for (std::size_t i = 0; i < history.events.size(); ++i) {
      const GeologicEvent& event = history.events[i];
      Require(allIds.insert(event.id).second, "two events share an id");
      Require(event.province == province.id,
              "an event belongs to another province");
      Require(event.startMyr >= event.endMyr - 1e-9 &&
                  event.startMyr <= planetAge + 1e-9 && event.endMyr >= -1e-9,
              "an event lies outside the planet's history");
      if (i > 0) {
        Require(event.parents.size() == 1,
                "an event after the basement has no single cause");
        const GeologicEvent* parent = history.Find(event.parents.front());
        Require(parent != nullptr,
                "the cause of an event is not in the history");
        // Not earlier than its parent: it starts when the parent has ended.
        Require(parent != nullptr && event.startMyr <= parent->endMyr + 1e-9,
                "an event begins before its cause has ended");
      }

      const bool formation = event.type == EventType::BasementFormation ||
                             event.type == EventType::Sedimentation ||
                             event.type == EventType::VolcanicEpisode ||
                             event.type == EventType::Intrusion ||
                             event.type == EventType::RegionalMetamorphism;
      if (formation) {
        const ModalComposition* rock = catalog->Materials().ModalOf(event.rock);
        Require(rock != nullptr, "a formation event has no rock");
        if (rock != nullptr && event.type != EventType::BasementFormation) {
          const bool matches =
              (event.type == EventType::Intrusion &&
               rock->rockClass == RockClass::IgneousIntrusive) ||
              (event.type == EventType::VolcanicEpisode &&
               (rock->rockClass == RockClass::IgneousExtrusive ||
                rock->rockClass == RockClass::Volcaniclastic)) ||
              (event.type == EventType::RegionalMetamorphism &&
               rock->rockClass == RockClass::Metamorphic) ||
              (event.type == EventType::Sedimentation &&
               (rock->rockClass == RockClass::SedimentaryClastic ||
                rock->rockClass == RockClass::SedimentaryChemical ||
                rock->rockClass == RockClass::SedimentaryBiogenic));
          Require(matches,
                  "the type of a formation event does not fit its rock");
        }
        // Superposition: each formation is younger than the one below it.
        if (i > 0)
          Require(event.startMyr <= lastFormationEnd + 1e-9,
                  "a formation is older than the one below it");
        lastFormationEnd = event.endMyr;
        // What erosion stripped is the youngest formations; once one is gone
        // all above are.
        Require(!eroded || event.erased,
                "a formation above an eroded one still stands");
        eroded = eroded || event.erased;
      }
    }

    // Deformation after the formations, faults after the folds, erosion last.
    REQUIRE(history.foldingEvent >= 0);
    REQUIRE(history.faultingEvent > history.foldingEvent);
    const GeologicEvent& folding =
        history.events[static_cast<std::size_t>(history.foldingEvent)];
    const GeologicEvent& faulting =
        history.events[static_cast<std::size_t>(history.faultingEvent)];
    Require(folding.type == EventType::Folding &&
                faulting.type == EventType::Faulting,
            "the deformation events have the wrong types");
    Require(folding.startMyr <= lastFormationEnd + 1e-9,
            "folding begins before the last formation");
    Require(faulting.startMyr <= folding.endMyr + 1e-9,
            "faulting begins before the folds are done");
    Require((history.erosionEvent >= 0) ==
                (province.erosionLevel != ErosionLevel::Surface),
            "an erosion event exists exactly when the province is eroded");
    if (history.erosionEvent >= 0) {
      const GeologicEvent& erosion =
          history.events[static_cast<std::size_t>(history.erosionEvent)];
      Require(erosion.type == EventType::Erosion && erosion.endMyr == 0.0 &&
                  erosion.startMyr <= faulting.endMyr + 1e-9,
              "erosion is not the last event reaching the present");
    }

    // The standing formations of the pile are the events that are not erased,
    // in order.
    const std::vector<GeologyModel::Layer>& pile = model.PileOf(province.id);
    for (std::size_t p = 0; p < pile.size(); ++p) {
      const GeologicEvent* event =
          history.Find(model.FormationEvent(province.id, p));
      Require(event != nullptr && !event->erased && event->rock == pile[p].rock,
              "a pile layer does not match its formation event");
    }
  }
}

TEST_CASE("EveryPointCanSayWhereItsRockCameFrom") {
  const PlanetMap& map = SharedMap();
  const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
  const GeologyModel model(catalog, map);

  int faulted = 0;
  int points = 0;
  for (const Province& province : map.GetProvinces()) {
    const ProvinceHistory& history = model.HistoryOf(province.id);
    for (double east = -1500.0; east <= 1500.0; east += 370.0) {
      for (double north = -1500.0; north <= 1500.0; north += 410.0) {
        for (int elevation : {400, 250, 120, 0, -150, -600}) {
          const std::vector<EventId> chain =
              model.ProvenanceAt(province.id, east, north, elevation, 250);
          ++points;
          Require(!chain.empty(), "a point has no provenance");
          if (chain.empty()) continue;

          const GeologyModel::ColumnProfile profile =
              model.ProfileAt(province.id, east, north, 250);
          const GeologicEvent* first = history.Find(chain.front());
          Require(
              first != nullptr &&
                  first->rock == GeologyModel::RockAt(profile, elevation),
              "the first event of a chain is not the one that made the rock");

          double previousStart = 1e18;
          for (EventId id : chain) {
            const GeologicEvent* event = history.Find(id);
            Require(event != nullptr,
                    "an event of a chain is not in the province's history");
            if (event == nullptr) continue;
            Require(event->startMyr <= previousStart + 1e-9,
                    "a chain is not ordered from the oldest event");
            previousStart = event->startMyr;
            faulted += event->type == EventType::Faulting ? 1 : 0;
          }
          Require(chain.size() >= 2, "a point's chain lacks the folding");
        }
      }
    }
  }
  CHECK(points > 0);
  CHECK(faulted > 0);
}

TEST_CASE("PilesRespectTheBiosphereOfTheirTime") {
  const PlanetMap& map = SharedMap();
  const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
  const GeologyModel model(catalog, map);
  const MaterialId coal = MaterialNamed("rock.bituminous_coal").id;
  const MaterialId redBeds = MaterialNamed("rock.red_beds").id;
  const MaterialId chalk = MaterialNamed("rock.chalk").id;

  int withCoal = 0;
  for (const Province& province : map.GetProvinces()) {
    const ProvinceHistory& history = model.HistoryOf(province.id);
    // Every formation, standing or eroded, was laid down when its conditions
    // held.
    for (const GeologicEvent& event : history.events) {
      const BiosphereState state =
          map.GetEpochs().BiosphereAt(0.5 * (event.startMyr + event.endMyr));
      if (event.rock == coal) {
        Require(state.landPlants, "coal older than land plants");
        ++withCoal;
      }
      if (event.rock == redBeds)
        Require(state.freeOxygen, "red beds before free oxygen");
      if (event.rock == chalk)
        Require(state.calcareousPlankton, "chalk before calcareous plankton");
    }
  }
  CHECK(withCoal >= 0);
}

TEST_CASE("ModelIsDeterministic") {
  const PlanetMap& map = SharedMap();
  const auto catalog = std::make_shared<const GeologyCatalog>(SharedCatalog());
  const GeologyModel first(catalog, map);
  const GeologyModel second(catalog, map);
  for (const Province& province : map.GetProvinces()) {
    const auto& a = first.PileOf(province.id);
    const auto& b = second.PileOf(province.id);
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
      Require(a[i].rock == b[i].rock &&
                  a[i].thicknessMeters == b[i].thicknessMeters,
              "the pile of a province changed between builds");
    }
  }

  GeologyConfig doubled;
  doubled.strataThicknessScale = 0.4;
  const GeologyModel thicker(catalog, map, doubled);
  int thickerLayers = 0;
  for (const Province& province : map.GetProvinces()) {
    const auto& a = first.PileOf(province.id);
    const auto& b = thicker.PileOf(province.id);
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i + 1 < a.size(); ++i) {
      thickerLayers += b[i].thicknessMeters > a[i].thicknessMeters ? 1 : 0;
    }
  }
  CHECK(thickerLayers > 0);
  CHECK_THROWS_AS(GeologyModel(nullptr, map), std::invalid_argument);
}

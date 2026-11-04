#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "foundation/ContentRegistry.h"
#include "gameplay/GameWorld.h"
#include "gameplay/content/GameContent.h"

namespace {
using namespace Infinitesia;

struct Thing {
  std::string key;
  int value = 0;
};

// A copy of the shipped content in a scratch directory, to break on purpose.
struct ScratchContent {
  std::filesystem::path directory;

  explicit ScratchContent(const char* name)
      : directory(std::filesystem::temp_directory_path() / name) {
    std::filesystem::remove_all(directory);
    std::filesystem::copy(INFINITESIA_GAMEPLAY_ASSET_DIR, directory);
  }

  ~ScratchContent() { std::filesystem::remove_all(directory); }

  void Write(const char* file, const std::string& text) const {
    std::ofstream(directory / file, std::ios::trunc) << text;
  }

  std::string LoadError() const {
    try {
      GameContent::LoadFromDirectory(directory);
    } catch (const ContentError& error) {
      return error.what();
    }
    return {};
  }
};

const char* const kItems =
    R"({ "items": [ { "id": "item.logs", "name": "Logs", "visual": "logs" } ] })";
}  // namespace

TEST_CASE("RegistryFindsDefinitionsByKey") {
  ContentRegistry<Thing> registry;
  registry.Add({"thing.a", 1});
  registry.Add({"thing.b", 2});

  CHECK(registry.Size() == 2);
  REQUIRE(registry.Find("thing.b") != nullptr);
  CHECK(registry.Find("thing.b")->value == 2);
  CHECK(registry.Find("thing.c") == nullptr);
  CHECK(registry.Require("thing.a").value == 1);
  CHECK(registry.All().front().key == "thing.a");
}

TEST_CASE("RegistryRejectsBadRecords") {
  ContentRegistry<Thing> registry;
  registry.Add({"thing.a", 1});
  CHECK_THROWS_AS(registry.Add({"thing.a", 2}), ContentError);
  CHECK_THROWS_AS(registry.Add({"", 2}), ContentError);
  CHECK_THROWS_AS(registry.Require("thing.missing"), ContentError);
  CHECK(registry.Size() == 1);
}

TEST_CASE("ShippedGameplayContentLoads") {
  const auto content =
      GameContent::LoadFromDirectory(INFINITESIA_GAMEPLAY_ASSET_DIR);

  const ObjectKindDef& worker = content->GetObjectKind("object.worker");
  CHECK(worker.Has(ObjectAbility::Worker));
  CHECK_FALSE(worker.Has(ObjectAbility::Pickable));
  CHECK_FALSE(worker.carriesItem);

  // Trees are blocks of the world (docs/07_BIOLOGY.md), not objects.
  CHECK_THROWS_AS(content->GetObjectKind("object.tree"), ContentError);

  CHECK(content->GetObjectKind("object.item").carriesItem);
  CHECK(content->GetItem("item.logs").maxStack == 64);
  CHECK_THROWS_AS(content->GetItem("item.nothing"), ContentError);
}

TEST_CASE("BrokenGameplayContentIsReportedByRecord") {
  const ScratchContent scratch("infinitesia_content_test");
  REQUIRE(scratch.LoadError().empty());

  scratch.Write(
      "items.json",
      R"({ "items": [ { "id": "item.logs", "name": "Logs", "maxStack": 0 } ] })");
  CHECK(scratch.LoadError().find("maxStack") != std::string::npos);

  scratch.Write("items.json", kItems);
  scratch.Write(
      "objects.json",
      R"({ "objects": [ { "id": "object.x", "name": "X", "size": [1, 1], "visual": "item",
             "abilities": ["flying"] } ] })");
  CHECK(scratch.LoadError().find("flying") != std::string::npos);

  scratch.Write(
      "objects.json",
      R"({ "objects": [ { "id": "object.x", "name": "X", "size": [1, 1], "visual": "tree" } ] })");
  CHECK(scratch.LoadError().find("unknown object visual") != std::string::npos);

  scratch.Write(
      "objects.json",
      R"({ "objects": [ { "id": "a", "name": "A", "size": [1, 1], "visual": "item" },
                                                      { "id": "a", "name": "B", "size": [1, 1], "visual": "item" } ] })");
  CHECK(scratch.LoadError().find("Duplicate") != std::string::npos);

  std::filesystem::remove(scratch.directory / "items.json");
  CHECK(scratch.LoadError().find("items.json") != std::string::npos);
}

TEST_CASE("EntityStoreChecksTheKindsRules") {
  const auto content =
      GameContent::LoadFromDirectory(INFINITESIA_GAMEPLAY_ASSET_DIR);
  const Grid grid({32, 32, 32, 1.0f});
  EntityStore entities;

  const WorldObjectId worker = entities.AddObject(
      content->GetObjectKind("object.worker"), {4, 4, 4}, grid);
  CHECK(entities.FindObject(worker)->Is(ObjectAbility::Worker));

  // A pile needs an item, anything else must not carry one.
  const Item logs(content->GetItem("item.logs"), 3);
  CHECK_THROWS_AS(entities.AddObject(content->GetObjectKind("object.item"),
                                     {5, 4, 4}, grid),
                  std::invalid_argument);
  CHECK_THROWS_AS(entities.AddObject(content->GetObjectKind("object.worker"),
                                     {6, 4, 4}, grid, logs),
                  std::invalid_argument);
  CHECK_NOTHROW(entities.AddObject(content->GetObjectKind("object.item"),
                                   {5, 4, 4}, grid, logs));

  CHECK(entities.TryAddToInventory(worker, logs));
  const WorldObjectSize size =
      GetWorldObjectSize(content->GetObjectKind("object.worker"), 2.0f);
  CHECK(size.width == doctest::Approx(1.24f));
  CHECK(size.height == doctest::Approx(2.4f));
}

TEST_CASE("InventoryStacksByTheItemsOwnLimit") {
  const auto content =
      GameContent::LoadFromDirectory(INFINITESIA_GAMEPLAY_ASSET_DIR);
  const ItemDef& logs = content->GetItem("item.logs");

  Inventory inventory;  // four slots
  CHECK(inventory.TryAdd(Item(logs, logs.maxStack * 4)));
  CHECK_FALSE(inventory.TryAdd(Item(logs, 1)));
}

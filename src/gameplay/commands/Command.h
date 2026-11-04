#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "gameplay/GameWorld.h"
#include "world/Grid.h"

namespace Infinitesia {
struct CommandOption {
  std::string id;
  std::string label;
  bool enabled;
  std::string disabledReason;
};

struct CommandResult {
  bool accepted;
  std::string message;
};

struct CommandRequest {
  std::string commandId;
  std::optional<WorldObjectId> actor;
  WorldObjectId target;
};

class Command {
 public:
  virtual ~Command() = default;

  virtual std::string_view GetId() const = 0;
  virtual std::string_view GetLabel() const = 0;
  virtual bool IsApplicable(const WorldObject& target) const = 0;
  virtual std::string GetDisabledReason(std::optional<WorldObjectId> actor,
                                        const WorldObject& target,
                                        const EntityStore& world) const = 0;
  virtual CommandResult Execute(std::optional<WorldObjectId> actor,
                                WorldObjectId target,
                                GameWorld& world) const = 0;
};
}  // namespace Infinitesia

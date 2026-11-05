#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "gameplay/commands/Command.h"

namespace Infinitesia
{
class CommandRegistry
{
public:
	void Register(std::unique_ptr<Command> command);

	std::vector<CommandOption> GetOptions(WorldObjectId target, std::optional<WorldObjectId> actor,
	                                      const EntityStore &world) const;

	CommandResult Execute(std::string_view commandId, std::optional<WorldObjectId> actor, WorldObjectId target,
	                      GameWorld &world) const;

private:
	const Command *FindCommand(std::string_view commandId) const;

	std::vector<std::unique_ptr<Command>> m_commands;
};
} // namespace Infinitesia

#pragma once

#include "commands/Command.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace Infinitesia
{
    class CommandRegistry
    {
    public:
        void Register(std::unique_ptr<Command> command);

        std::vector<CommandOption> GetOptions(
            WorldObjectId target,
            std::optional<WorldObjectId> actor,
            const World& world) const;

        CommandResult Execute(
            std::string_view commandId,
            std::optional<WorldObjectId> actor,
            WorldObjectId target,
            World& world) const;

    private:
        const Command* FindCommand(std::string_view commandId) const;

        std::vector<std::unique_ptr<Command>> m_commands;
    };
}

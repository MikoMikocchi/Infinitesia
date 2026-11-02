#include "world/CommandRegistry.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace Infinitesia
{
    void CommandRegistry::Register(std::unique_ptr<Command> command)
    {
        if (command == nullptr || command->GetId().empty() || command->GetLabel().empty())
        {
            throw std::invalid_argument("Command must have an ID and label.");
        }
        if (FindCommand(command->GetId()) != nullptr)
        {
            throw std::invalid_argument("Command IDs must be unique.");
        }
        m_commands.push_back(std::move(command));
    }

    std::vector<CommandOption> CommandRegistry::GetOptions(
        WorldObjectId targetId,
        std::optional<WorldObjectId> actor,
        const World& world) const
    {
        const WorldObject* target = world.FindObject(targetId);
        if (target == nullptr)
        {
            return {};
        }

        std::vector<CommandOption> options;
        for (const std::unique_ptr<Command>& command : m_commands)
        {
            if (!command->IsApplicable(*target))
            {
                continue;
            }

            std::string disabledReason =
                command->GetDisabledReason(actor, *target, world);
            options.push_back({
                std::string(command->GetId()),
                std::string(command->GetLabel()),
                disabledReason.empty(),
                std::move(disabledReason)
            });
        }
        return options;
    }

    CommandResult CommandRegistry::Execute(
        std::string_view commandId,
        std::optional<WorldObjectId> actor,
        WorldObjectId targetId,
        World& world) const
    {
        const Command* command = FindCommand(commandId);
        if (command == nullptr)
        {
            return { false, "Command is no longer available" };
        }

        const WorldObject* target = world.FindObject(targetId);
        if (target == nullptr || !command->IsApplicable(*target))
        {
            return { false, "Command target is no longer available" };
        }

        const std::string disabledReason =
            command->GetDisabledReason(actor, *target, world);
        if (!disabledReason.empty())
        {
            return { false, disabledReason };
        }
        return command->Execute(actor, targetId, world);
    }

    const Command* CommandRegistry::FindCommand(std::string_view commandId) const
    {
        for (const std::unique_ptr<Command>& command : m_commands)
        {
            if (command->GetId() == commandId)
            {
                return command.get();
            }
        }
        return nullptr;
    }
}

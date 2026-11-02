#include "world/WorkerCommandRegistry.h"

#include "world/WorkSystem.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace Infinitesia
{
    void WorkerCommandRegistry::Register(std::unique_ptr<WorkerCommand> command)
    {
        if (command == nullptr || command->GetId().empty() || command->GetLabel().empty())
        {
            throw std::invalid_argument("Worker command must have an ID and label.");
        }
        if (FindCommand(command->GetId()) != nullptr)
        {
            throw std::invalid_argument("Worker command IDs must be unique.");
        }
        m_commands.push_back(std::move(command));
    }

    std::vector<WorkerCommandOption> WorkerCommandRegistry::GetOptions(
        WorldObjectId targetId,
        std::optional<WorldObjectId> actor,
        const World& world,
        const Grid& grid,
        const WorkSystem& workSystem) const
    {
        const WorldObject* target = world.FindObject(targetId);
        if (target == nullptr)
        {
            return {};
        }

        std::vector<WorkerCommandOption> options;
        for (const std::unique_ptr<WorkerCommand>& command : m_commands)
        {
            if (!command->IsApplicable(*target))
            {
                continue;
            }

            std::string disabledReason =
                command->GetDisabledReason(actor, *target, world, grid, workSystem);
            options.push_back({
                std::string(command->GetId()),
                std::string(command->GetLabel()),
                disabledReason.empty(),
                std::move(disabledReason)
            });
        }
        return options;
    }

    WorkerCommandResult WorkerCommandRegistry::Execute(
        std::string_view commandId,
        WorldObjectId actor,
        WorldObjectId targetId,
        World& world,
        const Grid& grid,
        WorkSystem& workSystem) const
    {
        const WorkerCommand* command = FindCommand(commandId);
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
            command->GetDisabledReason(actor, *target, world, grid, workSystem);
        if (!disabledReason.empty())
        {
            return { false, disabledReason };
        }
        return command->Execute(actor, targetId, world, grid, workSystem);
    }

    const WorkerCommand* WorkerCommandRegistry::FindCommand(std::string_view commandId) const
    {
        for (const std::unique_ptr<WorkerCommand>& command : m_commands)
        {
            if (command->GetId() == commandId)
            {
                return command.get();
            }
        }
        return nullptr;
    }
}

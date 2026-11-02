#include "commands/PickUpLogsCommand.h"

namespace Infinitesia
{
    std::string_view PickUpLogsCommand::GetId() const
    {
        return "pick_up_logs";
    }

    std::string_view PickUpLogsCommand::GetLabel() const
    {
        return "Pick up";
    }

    bool PickUpLogsCommand::IsApplicable(const WorldObject& target) const
    {
        return target.type == WorldObjectType::Logs;
    }

    std::string PickUpLogsCommand::GetDisabledReason(
        std::optional<WorldObjectId> actor,
        const WorldObject& target,
        const World& world) const
    {
        if (!IsApplicable(target))
        {
            return "This command requires logs";
        }
        if (!actor.has_value())
        {
            return "Select a worker first";
        }

        const WorldObject* worker = world.FindObject(*actor);
        if (worker == nullptr || worker->type != WorldObjectType::Worker)
        {
            return "Selected worker is no longer available";
        }
        return {};
    }

    CommandResult PickUpLogsCommand::Execute(
        std::optional<WorldObjectId> actor,
        WorldObjectId target,
        World& world) const
    {
        if (!actor.has_value())
        {
            return { false, "Select a worker first" };
        }

        const WorldObject* worker = world.FindObject(*actor);
        if (worker == nullptr || worker->type != WorldObjectType::Worker)
        {
            return { false, "Selected worker is no longer available" };
        }

        const WorldObject* logs = world.FindObject(target);
        if (logs == nullptr || !IsApplicable(*logs))
        {
            return { false, "Logs are no longer available" };
        }

        if (!world.RemoveObject(target))
        {
            return { false, "Logs are no longer available" };
        }
        return { true, "Logs picked up" };
    }
}

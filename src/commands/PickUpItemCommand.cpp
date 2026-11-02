#include "commands/PickUpItemCommand.h"

namespace Infinitesia
{
    std::string_view PickUpItemCommand::GetId() const
    {
        return "pick_up_item";
    }

    std::string_view PickUpItemCommand::GetLabel() const
    {
        return "Pick up";
    }

    bool PickUpItemCommand::IsApplicable(const WorldObject& target) const
    {
        return target.type == WorldObjectType::Item;
    }

    std::string PickUpItemCommand::GetDisabledReason(
        std::optional<WorldObjectId> actor,
        const WorldObject& target,
        const World& world) const
    {
        if (!IsApplicable(target))
        {
            return "This command requires an item";
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

    CommandResult PickUpItemCommand::Execute(
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

        const WorldObject* item = world.FindObject(target);
        if (item == nullptr || !IsApplicable(*item))
        {
            return { false, "Item is no longer available" };
        }

        if (!world.RemoveObject(target))
        {
            return { false, "Item is no longer available" };
        }
        return { true, "Item picked up" };
    }
}

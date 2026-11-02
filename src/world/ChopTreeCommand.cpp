#include "world/ChopTreeCommand.h"

#include "world/WorkSystem.h"

namespace Infinitesia
{
    std::string_view ChopTreeCommand::GetId() const
    {
        return "chop_tree";
    }

    std::string_view ChopTreeCommand::GetLabel() const
    {
        return "Chop";
    }

    bool ChopTreeCommand::IsApplicable(const WorldObject& target) const
    {
        return target.type == WorldObjectType::Tree;
    }

    std::string ChopTreeCommand::GetDisabledReason(
        std::optional<WorldObjectId> actor,
        const WorldObject&,
        const World& world,
        const Grid&,
        const WorkSystem& workSystem) const
    {
        if (!actor.has_value())
        {
            return "Select a worker first";
        }

        const WorldObject* worker = world.FindObject(*actor);
        if (worker == nullptr || worker->type != WorldObjectType::Worker)
        {
            return "Selected worker is no longer available";
        }
        if (workSystem.GetActivity(*actor) != WorkerActivity::Idle)
        {
            return "Worker already has an order";
        }
        return {};
    }

    WorkerCommandResult ChopTreeCommand::Execute(
        WorldObjectId actor,
        WorldObjectId target,
        World& world,
        const Grid& grid,
        WorkSystem& workSystem) const
    {
        switch (workSystem.AssignChopOrder(actor, target, world, grid))
        {
        case WorkOrderResult::Accepted:
            return { true, "Worker is moving to the tree" };
        case WorkOrderResult::InvalidWorker:
            return { false, "Selected worker is no longer available" };
        case WorkOrderResult::InvalidTarget:
            return { false, "Tree is no longer available" };
        case WorkOrderResult::WorkerBusy:
            return { false, "Worker already has an order" };
        case WorkOrderResult::TargetBusy:
            return { false, "Tree is already being chopped" };
        case WorkOrderResult::NoReachableWorkCell:
            return { false, "No free adjacent cell for the worker" };
        }
        return { false, "Could not assign work order" };
    }
}

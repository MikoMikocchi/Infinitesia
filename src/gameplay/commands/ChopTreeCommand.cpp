#include "gameplay/commands/ChopTreeCommand.h"

#include "gameplay/work/WorkSystem.h"

namespace Infinitesia
{
    ChopTreeCommand::ChopTreeCommand(WorkSystem& workSystem, const Grid& grid)
        : m_workSystem(workSystem),
          m_grid(grid)
    {
    }

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
        const WorldObject& target,
        const EntityStore& world) const
    {
        if (!IsApplicable(target))
        {
            return "This command requires a tree";
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
        if (m_workSystem.GetActivity(*actor) != WorkerActivity::Idle)
        {
            return "Worker already has an order";
        }
        return {};
    }

    CommandResult ChopTreeCommand::Execute(
        std::optional<WorldObjectId> actor,
        WorldObjectId target,
        GameWorld& world) const
    {
        if (!actor.has_value())
        {
            return { false, "Select a worker first" };
        }

        switch (m_workSystem.AssignChopOrder(*actor, target, world, m_grid))
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

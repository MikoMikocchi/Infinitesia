#include "input/WorldInteractionController.h"

namespace Infinitesia
{
    namespace
    {
        const char* GetWorkOrderMessage(WorkOrderResult result)
        {
            switch (result)
            {
            case WorkOrderResult::Accepted:
                return "Worker is moving to the tree";
            case WorkOrderResult::InvalidWorker:
                return "Selected worker is no longer available";
            case WorkOrderResult::InvalidTarget:
                return "Tree is no longer available";
            case WorkOrderResult::WorkerBusy:
                return "Worker already has an order";
            case WorkOrderResult::TargetBusy:
                return "Tree is already being chopped";
            case WorkOrderResult::NoReachableWorkCell:
                return "No free adjacent cell for the worker";
            }

            return "Could not assign work order";
        }

        std::optional<WorldObjectId> GetObjectOfType(
            const World& world,
            std::optional<WorldObjectId> objectId,
            WorldObjectType type)
        {
            if (!objectId.has_value())
            {
                return std::nullopt;
            }

            const WorldObject* object = world.FindObject(*objectId);
            if (object == nullptr || object->type != type)
            {
                return std::nullopt;
            }
            return object->id;
        }
    }

    void WorldInteractionController::Update(
        WorldInteractionInput input,
        const Grid& grid,
        World& world,
        const Camera3D& camera,
        WorkSystem& workSystem,
        int screenWidth,
        int screenHeight)
    {
        m_hoveredCell = m_gridPointer.PickCell(grid, camera, input.mousePosition);
        if (m_contextMenu.BlocksWorldInput(input.mousePosition))
        {
            m_hoveredObject.reset();
        }
        else
        {
            m_hoveredObject = m_worldObjectPicker.PickObject(
                grid, world, camera, input.mousePosition);
        }

        const std::optional<WorldObjectId> hoveredTree =
            GetObjectOfType(world, m_hoveredObject, WorldObjectType::Tree);
        const std::optional<WorldObjectId> selectedWorker =
            GetObjectOfType(world, m_selectedObject, WorldObjectType::Worker);

        const ContextMenuUpdate menuUpdate = m_contextMenu.Update(
            input.mousePosition,
            input.rightClickPressed,
            input.leftClickPressed,
            input.escapePressed,
            hoveredTree,
            selectedWorker,
            screenWidth,
            screenHeight);

        if (menuUpdate.actionRequest.has_value())
        {
            const WorldActionRequest& request = *menuUpdate.actionRequest;
            if (request.action == WorldAction::Chop)
            {
                m_actionNotice = GetWorkOrderMessage(
                    workSystem.AssignChopOrder(request.actor, request.target, world, grid));
            }
        }
        else if (input.leftClickPressed && !menuUpdate.consumedMouseClick)
        {
            m_selectedObject = m_hoveredObject;
            m_actionNotice.clear();
        }
    }

    std::optional<GridCell> WorldInteractionController::GetHoveredCell() const
    {
        return m_hoveredCell;
    }

    std::optional<WorldObjectId> WorldInteractionController::GetHoveredObject() const
    {
        return m_hoveredObject;
    }

    std::optional<WorldObjectId> WorldInteractionController::GetSelectedObject() const
    {
        return m_selectedObject;
    }

    const std::string& WorldInteractionController::GetActionNotice() const
    {
        return m_actionNotice;
    }

    void WorldInteractionController::DrawOverlay() const
    {
        m_contextMenu.Draw();
    }
}

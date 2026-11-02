#include "input/WorldInteractionController.h"

#include "raylib.h"

namespace Infinitesia
{
    void WorldInteractionController::LoadAssets()
    {
        m_characterProfilePanel.LoadAssets();
    }

    void WorldInteractionController::UnloadAssets()
    {
        m_characterProfilePanel.UnloadAssets();
    }

    void WorldInteractionController::Update(
        WorldInteractionInput input,
        const Grid& grid,
        World& world,
        WorkSystem& workSystem,
        const Camera3D& camera,
        const CommandRegistry& commands,
        int screenWidth,
        int screenHeight,
        int selectedLevel)
    {
        if (m_characterProfilePanel.IsOpen())
        {
            const WorldObject* character = world.FindObject(m_characterProfilePanel.GetCharacter());
            if (character == nullptr || character->type != WorldObjectType::Worker)
            {
                m_characterProfilePanel.Close();
            }
            else
            {
                m_characterProfilePanel.Update(
                    input.mousePosition,
                    input.leftClickPressed,
                    input.escapePressed,
                    screenWidth,
                    screenHeight,
                    character->inventory.GetSlots().size());
                m_hoveredObject.reset();
                m_hoveredCell.reset();
                return;
            }
        }

        m_hoveredCell = m_gridPointer.PickCell(
            grid,
            camera,
            input.mousePosition,
            selectedLevel);
        if (m_contextMenu.BlocksWorldInput(input.mousePosition))
        {
            m_hoveredObject.reset();
            m_hoveredCell.reset();
        }
        else
        {
            m_hoveredObject = m_worldObjectPicker.PickObject(
                grid, world, camera, input.mousePosition, selectedLevel);
        }

        std::optional<WorldObjectId> actor = m_selectedObject;
        if (actor.has_value() && world.FindObject(*actor) == nullptr)
        {
            actor.reset();
        }
        if (actor.has_value())
        {
            const WorldObject* selectedObject = world.FindObject(*actor);
            if (selectedObject == nullptr || selectedObject->cell.y != selectedLevel)
            {
                actor.reset();
                m_selectedObject.reset();
            }
        }

        std::vector<CommandOption> targetCommands;
        if (m_hoveredObject.has_value())
        {
            targetCommands = commands.GetOptions(
                *m_hoveredObject,
                actor,
                world);
        }

        std::vector<CommandOption> cellCommands;
        if (m_hoveredCell.has_value())
        {
            const Block* block = world.GetBlock(*m_hoveredCell);
            const std::string workerDisabledReason = [&]()
            {
                if (!actor.has_value())
                {
                    return std::string("Select a worker first");
                }
                const WorldObject* worker = world.FindObject(*actor);
                if (worker == nullptr || worker->type != WorldObjectType::Worker)
                {
                    return std::string("Selected worker is no longer available");
                }
                if (workSystem.GetActivity(*actor) != WorkerActivity::Idle)
                {
                    return std::string("Worker already has an order");
                }
                return std::string{};
            }();
            const bool solidCell = block != nullptr && block->type != BlockType::Air;
            if (solidCell)
            {
                const bool occupied = world.FindObjectAt(*m_hoveredCell).has_value();
                const std::string digDisabledReason = occupied
                    ? "Cell is occupied"
                    : workerDisabledReason;
                cellCommands.push_back({
                    "dig_block",
                    "Dig",
                    digDisabledReason.empty(),
                    digDisabledReason
                });
            }
            const std::optional<WorldObjectId> occupant = world.FindObjectAt(*m_hoveredCell);
            const bool occupiedByAnotherWorker =
                occupant.has_value() && (!actor.has_value() || *occupant != *actor);
            const std::string moveDisabledReason = !solidCell
                ? "Select a solid cell"
                : occupiedByAnotherWorker
                    ? "Cell is occupied"
                    : workerDisabledReason;
            cellCommands.push_back({
                "move_to_cell",
                "Move here",
                moveDisabledReason.empty(),
                moveDisabledReason
            });
        }

        std::optional<WorldObjectId> profileTarget;
        if (m_hoveredObject.has_value())
        {
            const WorldObject* hoveredObject = world.FindObject(*m_hoveredObject);
            if (hoveredObject != nullptr && hoveredObject->type == WorldObjectType::Worker)
            {
                profileTarget = hoveredObject->id;
            }
        }

        const ContextMenuUpdate menuUpdate = m_contextMenu.Update(
            input.mousePosition,
            input.rightClickPressed,
            input.leftClickPressed,
            input.escapePressed,
            m_hoveredObject,
            m_hoveredCell,
            actor,
            targetCommands,
            cellCommands,
            profileTarget,
            screenWidth,
            screenHeight);

        if (menuUpdate.profileRequest.has_value())
        {
            m_characterProfilePanel.Open(*menuUpdate.profileRequest);
            m_lastClickedObject.reset();
        }
        else if (menuUpdate.commandRequest.has_value())
        {
            const CommandRequest& request = *menuUpdate.commandRequest;
            const CommandResult result = commands.Execute(
                request.commandId,
                request.actor,
                request.target,
                world);
            m_actionNotice = result.message;
        }
        else if (menuUpdate.cellCommandRequest.has_value())
        {
            const ContextMenuUpdate::CellCommandRequest& request =
                *menuUpdate.cellCommandRequest;
            WorkOrderResult result = WorkOrderResult::InvalidTarget;
            if (request.commandId == "dig_block")
            {
                result = request.actor.has_value()
                    ? workSystem.AssignDigOrder(*request.actor, request.target, world, grid)
                    : WorkOrderResult::InvalidWorker;
            }
            else if (request.commandId == "move_to_cell")
            {
                result = request.actor.has_value()
                    ? workSystem.AssignMoveOrder(*request.actor, request.target, world, grid)
                    : WorkOrderResult::InvalidWorker;
            }

            switch (result)
            {
            case WorkOrderResult::Accepted:
                m_actionNotice = request.commandId == "dig_block"
                    ? "Worker is going to dig"
                    : "Worker is moving to the selected cell";
                break;
            case WorkOrderResult::InvalidWorker:
                m_actionNotice = "Selected worker is no longer available";
                break;
            case WorkOrderResult::InvalidTarget:
                m_actionNotice = "Select a reachable solid cell on the worker's level";
                break;
            case WorkOrderResult::WorkerBusy:
                m_actionNotice = "Worker already has an order";
                break;
            case WorkOrderResult::TargetBusy:
                m_actionNotice = "Cell already has an order";
                break;
            case WorkOrderResult::NoReachableWorkCell:
                m_actionNotice = "Cell is not reachable";
                break;
            }
        }
        else if (input.leftClickPressed && !menuUpdate.consumedMouseClick)
        {
            m_selectedObject = m_hoveredObject;
            m_actionNotice.clear();

            const WorldObject* clickedObject = m_hoveredObject.has_value()
                ? world.FindObject(*m_hoveredObject)
                : nullptr;
            if (clickedObject != nullptr && clickedObject->type == WorldObjectType::Worker)
            {
                const double clickTime = GetTime();
                constexpr double kDoubleClickInterval = 0.35;
                if (m_lastClickedObject == clickedObject->id &&
                    clickTime - m_lastObjectClickTime <= kDoubleClickInterval)
                {
                    m_characterProfilePanel.Open(clickedObject->id);
                    m_contextMenu.Close();
                    m_lastClickedObject.reset();
                }
                else
                {
                    m_lastClickedObject = clickedObject->id;
                    m_lastObjectClickTime = clickTime;
                }
            }
            else
            {
                m_lastClickedObject.reset();
            }
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

    void WorldInteractionController::DrawOverlay(const World& world) const
    {
        m_contextMenu.Draw();
        if (m_characterProfilePanel.IsOpen())
        {
            const WorldObject* character = world.FindObject(m_characterProfilePanel.GetCharacter());
            if (character != nullptr && character->type == WorldObjectType::Worker)
            {
                m_characterProfilePanel.Draw(*character, GetScreenWidth(), GetScreenHeight());
            }
        }
    }
}

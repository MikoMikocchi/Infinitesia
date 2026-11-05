#include "presentation/input/WorldInteractionController.h"

#include "raylib.h"

namespace Infinitesia
{
void WorldInteractionController::Update(WorldInteractionInput input, const Grid &grid, GameWorld &world,
                                        WorkSystem &workSystem, const Camera3D &camera, const CommandRegistry &commands,
                                        int screenWidth, int screenHeight, int selectedLevel, ViewMode viewMode)
{
	if (m_characterProfilePanel.IsOpen())
	{
		const WorldObject *character = world.entities.FindObject(m_characterProfilePanel.GetCharacter());
		if (character == nullptr || !character->Is(ObjectAbility::Worker))
		{
			m_characterProfilePanel.Close();
		}
		else
		{
			m_characterProfilePanel.Update(input.mousePosition, input.leftClickPressed, input.escapePressed,
			                               screenWidth, screenHeight, character->inventory.GetSlots().size());
			m_hoveredObject.reset();
			m_hoveredCell.reset();
			return;
		}
	}

	m_hoveredCell = m_gridPointer.PickCell(grid, world.terrain, camera, input.mousePosition, selectedLevel, viewMode);
	if (m_contextMenu.BlocksWorldInput(input.mousePosition))
	{
		m_hoveredObject.reset();
		m_hoveredCell.reset();
	}
	else
	{
		m_hoveredObject =
		    m_worldObjectPicker.PickObject(grid, world.entities, camera, input.mousePosition, selectedLevel, viewMode);
	}

	std::optional<WorldObjectId> actor = m_selectedObject;
	if (actor.has_value())
	{
		const WorldObject *selectedObject = world.entities.FindObject(*actor);
		if (selectedObject == nullptr)
		{
			actor.reset();
			m_selectedObject.reset();
		}
		else if (!selectedObject->Is(ObjectAbility::Worker))
		{
			actor.reset();
		}
		else if (!IsLevelShown(viewMode, selectedObject->cell.y, selectedLevel))
		{
			actor.reset();
			m_selectedObject.reset();
		}
	}

	std::vector<CommandOption> targetCommands;
	if (m_hoveredObject.has_value())
	{
		targetCommands = commands.GetOptions(*m_hoveredObject, actor, world.entities);
	}

	std::vector<CommandOption> cellCommands;
	if (m_hoveredCell.has_value())
	{
		const Block *block = world.terrain.GetBlock(*m_hoveredCell);
		const std::string workerDisabledReason = [&]() {
			if (!actor.has_value())
			{
				return std::string("Select a worker first");
			}
			const WorldObject *worker = world.entities.FindObject(*actor);
			if (worker == nullptr || !worker->Is(ObjectAbility::Worker))
			{
				return std::string("Selected worker is no longer available");
			}
			if (workSystem.GetActivity(*actor) != WorkerActivity::Idle)
			{
				return std::string("Worker already has an order");
			}
			return std::string{};
		}();
		const bool solidCell = block != nullptr && world.terrain.Registry().Occupied(block->state);
		const bool treeWood = block != nullptr && world.terrain.Registry().Has(block->state.matter, Tag::TreeWood);
		const bool foliage = block != nullptr && world.terrain.Registry().Has(block->state.matter, Tag::TreeLeaves);
		if (treeWood)
		{
			// A tree is felled whole, from any cell of its wood.
			cellCommands.push_back({"chop_tree", "Chop tree", workerDisabledReason.empty(), workerDisabledReason});
		}
		else if (solidCell && !foliage)
		{
			const bool occupied = world.entities.FindObjectAt(*m_hoveredCell).has_value();
			const std::string digDisabledReason = occupied ? "Cell is occupied" : workerDisabledReason;
			cellCommands.push_back({"dig_block", "Dig", digDisabledReason.empty(), digDisabledReason});
		}
		const std::optional<WorldObjectId> occupant = world.entities.FindObjectAt(*m_hoveredCell);
		const bool occupiedByAnotherWorker = occupant.has_value() && (!actor.has_value() || *occupant != *actor);
		const std::string moveDisabledReason = !solidCell                ? "Select a solid cell"
		                                       : occupiedByAnotherWorker ? "Cell is occupied"
		                                                                 : workerDisabledReason;
		cellCommands.push_back({"move_to_cell", "Move here", moveDisabledReason.empty(), moveDisabledReason});
	}

	std::optional<WorldObjectId> profileTarget;
	if (m_hoveredObject.has_value())
	{
		const WorldObject *hoveredObject = world.entities.FindObject(*m_hoveredObject);
		if (hoveredObject != nullptr && hoveredObject->Is(ObjectAbility::Worker))
		{
			profileTarget = hoveredObject->id;
		}
	}

	const ContextMenuUpdate menuUpdate = m_contextMenu.Update(
	    input.mousePosition, input.rightClickPressed, input.leftClickPressed, input.escapePressed, m_hoveredObject,
	    m_hoveredCell, actor, targetCommands, cellCommands, profileTarget, screenWidth, screenHeight);

	if (menuUpdate.profileRequest.has_value())
	{
		m_characterProfilePanel.Open(*menuUpdate.profileRequest);
		m_lastClickedObject.reset();
	}
	else if (menuUpdate.commandRequest.has_value())
	{
		const CommandRequest &request = *menuUpdate.commandRequest;
		const CommandResult result = commands.Execute(request.commandId, request.actor, request.target, world);
		m_actionNotice = result.message;
	}
	else if (menuUpdate.cellCommandRequest.has_value())
	{
		const ContextMenuUpdate::CellCommandRequest &request = *menuUpdate.cellCommandRequest;
		WorkOrderResult result = WorkOrderResult::InvalidTarget;
		if (request.commandId == "dig_block")
		{
			result = request.actor.has_value() ? workSystem.AssignDigOrder(*request.actor, request.target, world, grid)
			                                   : WorkOrderResult::InvalidWorker;
		}
		else if (request.commandId == "chop_tree")
		{
			result = request.actor.has_value() ? workSystem.AssignChopOrder(*request.actor, request.target, world, grid)
			                                   : WorkOrderResult::InvalidWorker;
		}
		else if (request.commandId == "move_to_cell")
		{
			result = request.actor.has_value() ? workSystem.AssignMoveOrder(*request.actor, request.target, world, grid)
			                                   : WorkOrderResult::InvalidWorker;
		}

		switch (result)
		{
		case WorkOrderResult::Accepted:
			m_actionNotice = request.commandId == "dig_block"   ? "Worker is going to dig"
			                 : request.commandId == "chop_tree" ? "Worker is going to fell the tree"
			                                                    : "Worker is moving to the selected cell";
			break;
		case WorkOrderResult::InvalidWorker:
			m_actionNotice = "Selected worker is no longer available";
			break;
		case WorkOrderResult::InvalidTarget:
			m_actionNotice = request.commandId == "chop_tree" ? "That is no longer a tree"
			                                                  : "Select a reachable solid cell on the worker's level";
			break;
		case WorkOrderResult::WorkerBusy:
			m_actionNotice = "Worker already has an order";
			break;
		case WorkOrderResult::TargetBusy:
			m_actionNotice =
			    request.commandId == "chop_tree" ? "The tree is already being felled" : "Cell already has an order";
			break;
		case WorkOrderResult::NoReachableWorkCell:
			m_actionNotice =
			    request.commandId == "chop_tree" ? "No free ground by the trunk to work from" : "Cell is not reachable";
			break;
		}
	}
	else if (input.leftClickPressed && !menuUpdate.consumedMouseClick)
	{
		m_selectedObject = m_hoveredObject;
		m_actionNotice.clear();

		const WorldObject *clickedObject =
		    m_hoveredObject.has_value() ? world.entities.FindObject(*m_hoveredObject) : nullptr;
		if (clickedObject != nullptr && clickedObject->Is(ObjectAbility::Worker))
		{
			const double clickTime = GetTime();
			constexpr double kDoubleClickInterval = 0.35;
			if (m_lastClickedObject == clickedObject->id && clickTime - m_lastObjectClickTime <= kDoubleClickInterval)
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

const std::string &WorldInteractionController::GetActionNotice() const
{
	return m_actionNotice;
}

void WorldInteractionController::DrawOverlay(const EntityStore &world, const TextureAssetManager &assets) const
{
	m_contextMenu.Draw();
	if (m_characterProfilePanel.IsOpen())
	{
		const WorldObject *character = world.FindObject(m_characterProfilePanel.GetCharacter());
		if (character != nullptr && character->Is(ObjectAbility::Worker))
		{
			m_characterProfilePanel.Draw(*character, assets, GetScreenWidth(), GetScreenHeight());
		}
	}
}
} // namespace Infinitesia

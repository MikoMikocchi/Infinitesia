#include "gameplay/commands/PickUpItemCommand.h"

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

bool PickUpItemCommand::IsApplicable(const WorldObject &target) const
{
	return target.Is(ObjectAbility::Pickable);
}

std::string PickUpItemCommand::GetDisabledReason(std::optional<WorldObjectId> actor, const WorldObject &target,
                                                 const EntityStore &world) const
{
	if (!IsApplicable(target))
	{
		return "This command requires an item";
	}
	if (!actor.has_value())
	{
		return "Select a worker first";
	}

	const WorldObject *worker = world.FindObject(*actor);
	if (worker == nullptr || !worker->Is(ObjectAbility::Worker))
	{
		return "Selected worker is no longer available";
	}
	if (!target.item.has_value() || !worker->inventory.CanStore(*target.item))
	{
		return "Inventory has no room";
	}
	return {};
}

CommandResult PickUpItemCommand::Execute(std::optional<WorldObjectId> actor, WorldObjectId target,
                                         GameWorld &world) const
{
	if (!actor.has_value())
	{
		return {false, "Select a worker first"};
	}

	const WorldObject *worker = world.entities.FindObject(*actor);
	if (worker == nullptr || !worker->Is(ObjectAbility::Worker))
	{
		return {false, "Selected worker is no longer available"};
	}

	const WorldObject *item = world.entities.FindObject(target);
	if (item == nullptr || !IsApplicable(*item))
	{
		return {false, "Item is no longer available"};
	}

	const Item pickedUpItem = *item->item;
	if (!world.entities.TryAddToInventory(*actor, pickedUpItem))
	{
		return {false, "Inventory has no room"};
	}
	if (!world.entities.RemoveObject(target))
	{
		return {false, "Item is no longer available"};
	}
	return {true, "Item picked up"};
}
} // namespace Infinitesia

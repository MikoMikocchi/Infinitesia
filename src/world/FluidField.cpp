#include "world/FluidField.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "world/Block.h"
#include "world/TerrainSource.h"
#include "world/VoxelWorld.h"

namespace Infinitesia
{
namespace
{
constexpr std::array<GridCell, 6> kNeighborOffsets{GridCell{-1, 0, 0}, GridCell{1, 0, 0},  GridCell{0, -1, 0},
                                                   GridCell{0, 1, 0},  GridCell{0, 0, -1}, GridCell{0, 0, 1}};
constexpr std::array<GridCell, 4> kSideOffsets{GridCell{-1, 0, 0}, GridCell{1, 0, 0}, GridCell{0, 0, -1},
                                               GridCell{0, 0, 1}};
// Pressure tries down, then sideways, then up, like Dwarf Fortress.
constexpr std::array<GridCell, 6> kPressureOffsets{GridCell{0, -1, 0}, GridCell{-1, 0, 0}, GridCell{1, 0, 0},
                                                   GridCell{0, 0, -1}, GridCell{0, 0, 1},  GridCell{0, 1, 0}};
// One of the seven depth levels of a Dwarf Fortress tile.
constexpr std::uint16_t kPressureLevelMb = kFullFluidCellMb / 7;
// A pressure search is a local teleport, not a scan of the whole sea.
constexpr std::size_t kPressureSearchLimit = 2048;

std::uint16_t AddAmount(std::uint16_t left, std::uint16_t right)
{
	return static_cast<std::uint16_t>(
	    std::min<int>(static_cast<int>(left) + static_cast<int>(right), kFullFluidCellMb));
}
} // namespace

FluidField::FluidField(int gridWidth, int gridHeight, int gridDepth)
    : m_gridWidth(gridWidth), m_gridHeight(gridHeight), m_gridDepth(gridDepth)
{
	if (gridWidth <= 0 || gridHeight <= 0 || gridDepth <= 0)
	{
		throw std::invalid_argument("Fluid field needs a valid grid.");
	}
}

bool FluidField::Contains(GridCell cell) const noexcept
{
	return cell.x >= 0 && cell.x < m_gridWidth && cell.y >= 0 && cell.y < m_gridHeight && cell.z >= 0 &&
	       cell.z < m_gridDepth;
}

FluidChunk &FluidField::GetOrCreateChunk(ChunkCoordinate coordinate)
{
	const auto found = m_chunks.find(coordinate);
	if (found != m_chunks.end())
	{
		return found->second;
	}

	const int startX = coordinate.x * kChunkSize;
	const int startY = coordinate.y * kChunkSize;
	const int startZ = coordinate.z * kChunkSize;
	FluidChunk chunk;
	chunk.coordinate = coordinate;
	chunk.width = std::min(kChunkSize, m_gridWidth - startX);
	chunk.height = std::min(kChunkSize, m_gridHeight - startY);
	chunk.depth = std::min(kChunkSize, m_gridDepth - startZ);
	chunk.cells.assign(static_cast<std::size_t>(chunk.width) * static_cast<std::size_t>(chunk.height) *
	                       static_cast<std::size_t>(chunk.depth),
	                   FluidCell{});
	return m_chunks.emplace(coordinate, std::move(chunk)).first->second;
}

std::size_t FluidField::IndexIn(const FluidChunk &chunk, GridCell cell) const
{
	const int localX = cell.x - chunk.coordinate.x * kChunkSize;
	const int localY = cell.y - chunk.coordinate.y * kChunkSize;
	const int localZ = cell.z - chunk.coordinate.z * kChunkSize;
	return (static_cast<std::size_t>(localZ) * static_cast<std::size_t>(chunk.height) +
	        static_cast<std::size_t>(localY)) *
	           static_cast<std::size_t>(chunk.width) +
	       static_cast<std::size_t>(localX);
}

const FluidCell *FluidField::AtRaw(GridCell cell) const
{
	if (!Contains(cell))
	{
		return nullptr;
	}
	const auto chunk = m_chunks.find({cell.x / kChunkSize, cell.y / kChunkSize, cell.z / kChunkSize});
	if (chunk == m_chunks.end())
	{
		return nullptr;
	}
	return &chunk->second.cells[IndexIn(chunk->second, cell)];
}

FluidCell *FluidField::AtRaw(GridCell cell)
{
	return const_cast<FluidCell *>(static_cast<const FluidField &>(*this).AtRaw(cell));
}

const FluidCell *FluidField::At(GridCell cell) const
{
	return AtRaw(cell);
}

FluidCell *FluidField::At(GridCell cell)
{
	return AtRaw(cell);
}

bool FluidField::WriteCell(GridCell cell, FluidCell value)
{
	if (!Contains(cell))
	{
		return false;
	}
	FluidCell *current = AtRaw(cell);
	if (current != nullptr && *current == value)
	{
		return false;
	}
	if (current == nullptr)
	{
		// A dry cell in a dry chunk needs no storage.
		if (value.amountMb == 0)
		{
			return false;
		}
		FluidChunk &chunk = GetOrCreateChunk({cell.x / kChunkSize, cell.y / kChunkSize, cell.z / kChunkSize});
		current = &chunk.cells[IndexIn(chunk, cell)];
	}
	*current = value;
	++m_revision;
	return true;
}

void FluidField::Activate(GridCell cell)
{
	if (Contains(cell))
	{
		m_active.insert(cell);
	}
}

void FluidField::ActivateAround(GridCell cell)
{
	Activate(cell);
	for (const GridCell offset : kNeighborOffsets)
	{
		Activate(GridCell{cell.x + offset.x, cell.y + offset.y, cell.z + offset.z});
	}
}

void FluidField::Set(GridCell cell, FluidCell value)
{
	if (!Contains(cell))
	{
		throw std::out_of_range("Fluid cell is outside the grid.");
	}
	WriteCell(cell, value);
	ActivateAround(cell);
	// A placed body of water (a lake in a test, a filled reservoir later) can
	// push through a connected tunnel just like the terrain's sea.
	PressurizeAround(cell);
	MarkAroundModified(cell);
}

void FluidField::MarkModified(GridCell cell)
{
	const auto chunk = m_chunks.find({cell.x / kChunkSize, cell.y / kChunkSize, cell.z / kChunkSize});
	if (chunk != m_chunks.end())
	{
		chunk->second.modified = true;
	}
}

void FluidField::MarkAroundModified(GridCell cell)
{
	MarkModified(cell);
	for (const GridCell offset : kNeighborOffsets)
	{
		MarkModified(GridCell{cell.x + offset.x, cell.y + offset.y, cell.z + offset.z});
	}
}

void FluidField::RemoveChunk(ChunkCoordinate coordinate)
{
	if (m_chunks.erase(coordinate) == 0)
	{
		return;
	}
	const auto inChunk = [coordinate](GridCell cell) {
		return cell.x / kChunkSize == coordinate.x && cell.y / kChunkSize == coordinate.y &&
		       cell.z / kChunkSize == coordinate.z;
	};
	for (auto cell = m_active.begin(); cell != m_active.end();)
	{
		cell = inChunk(*cell) ? m_active.erase(cell) : std::next(cell);
	}
	for (auto cell = m_pressure.begin(); cell != m_pressure.end();)
	{
		cell = inChunk(*cell) ? m_pressure.erase(cell) : std::next(cell);
	}
	++m_revision;
}

void FluidField::InitializeChunk(const WorldChunk &chunk, const TerrainSource &terrain)
{
	if (m_chunks.find(chunk.coordinate) != m_chunks.end())
	{
		return;
	}

	const int startX = chunk.coordinate.x * kChunkSize;
	const int startY = chunk.coordinate.y * kChunkSize;
	const int startZ = chunk.coordinate.z * kChunkSize;
	FluidChunk fluidChunk;
	fluidChunk.coordinate = chunk.coordinate;
	fluidChunk.width = chunk.width;
	fluidChunk.height = chunk.height;
	fluidChunk.depth = chunk.depth;
	fluidChunk.cells.assign(static_cast<std::size_t>(chunk.width) * static_cast<std::size_t>(chunk.height) *
	                            static_cast<std::size_t>(chunk.depth),
	                        FluidCell{});
	bool anyFluid = false;

	for (int localZ = 0; localZ < chunk.depth; ++localZ)
	{
		for (int localX = 0; localX < chunk.width; ++localX)
		{
			const int x = startX + localX;
			const int z = startZ + localZ;
			const FluidColumn column = terrain.FluidAt(x, z);
			if (!column.HasFluid() || column.groundBlock < 0 || column.topBlock <= column.groundBlock)
			{
				continue;
			}

			const int firstBlock = column.groundBlock + 1;
			const int topBlock = std::min(column.topBlock, startY + chunk.height - 1);
			for (int y = std::max(firstBlock, startY); y <= topBlock; ++y)
			{
				const double bottomMeters = column.groundMeters + static_cast<double>(y - firstBlock);
				const double depthInCell = std::clamp(column.surfaceMeters - bottomMeters, 0.0, 1.0);
				const bool isTop = y == column.topBlock;
				const std::uint16_t amount =
				    isTop ? static_cast<std::uint16_t>(std::lround(depthInCell * kFullFluidCellMb)) : kFullFluidCellMb;
				if (amount == 0)
				{
					continue;
				}

				const int localY = y - startY;
				const std::size_t index = (static_cast<std::size_t>(localZ) * static_cast<std::size_t>(chunk.height) +
				                           static_cast<std::size_t>(localY)) *
				                              static_cast<std::size_t>(chunk.width) +
				                          static_cast<std::size_t>(localX);
				fluidChunk.cells[index] = {column.matter, amount, column.origin, column.flowEast, column.flowNorth};
				// Deep full cells are a static body. Only the surface cell has to
				// settle; digging activates the surrounding water through
				// OnBlockChanged, so a whole ocean does not have to be stepped every
				// tick.
				if (isTop)
				{
					m_active.insert({x, y, z});
				}
				anyFluid = true;
			}
		}
	}

	if (!anyFluid)
	{
		return;
	}
	m_chunks.emplace(chunk.coordinate, std::move(fluidChunk));
	++m_revision;
}

bool FluidField::CanAccept(GridCell cell, MatterId matter, const VoxelWorld &world) const
{
	if (!Contains(cell))
	{
		return false;
	}
	// A cell in a chunk that is not loaded is never filled: that would leak
	// fluid into places the simulation cannot see.
	const Block *block = world.GetBlock(cell);
	if (block == nullptr || world.Registry().Occupied(block->state))
	{
		return false;
	}
	const FluidCell *fluid = At(cell);
	if (fluid == nullptr)
	{
		return true;
	}
	return fluid->amountMb < kFullFluidCellMb && (fluid->amountMb == 0 || fluid->matter == matter);
}

std::uint16_t FluidField::Transfer(GridCell from, GridCell to, std::uint16_t maximumMb, const VoxelWorld &world)
{
	const FluidCell *source = At(from);
	if (source == nullptr || source->amountMb == 0 || !CanAccept(to, source->matter, world))
	{
		return 0;
	}

	const FluidCell *target = At(to);
	const std::uint16_t targetAmount = target != nullptr ? target->amountMb : 0;
	const std::uint16_t room = static_cast<std::uint16_t>(kFullFluidCellMb - targetAmount);
	const std::uint16_t moved = std::min({maximumMb, source->amountMb, room});
	if (moved == 0)
	{
		return 0;
	}

	const MatterId matter = source->matter;
	FluidCell targetValue;
	targetValue.amountMb = AddAmount(targetAmount, moved);
	if (target != nullptr && targetAmount > 0)
	{
		// The receiver already held this fluid: keep what it was.
		targetValue.matter = target->matter;
		targetValue.origin = target->origin;
		targetValue.flowEast = target->flowEast;
		targetValue.flowNorth = target->flowNorth;
	}
	else if (target != nullptr && target->origin == FluidOrigin::Sea)
	{
		// A drained sea source stays a sea source: the sea is endless.
		targetValue.matter = target->matter;
		targetValue.origin = FluidOrigin::Sea;
		targetValue.flowEast = target->flowEast;
		targetValue.flowNorth = target->flowNorth;
	}
	else
	{
		// Fluid that flowed here is no longer at its original source.
		targetValue.matter = matter;
		targetValue.origin = FluidOrigin::None;
		// A river's direction travels with its water while the channel is
		// filling; it is metadata, not a force.
		targetValue.flowEast = source->flowEast;
		targetValue.flowNorth = source->flowNorth;
	}
	WriteCell(to, targetValue);

	FluidCell sourceValue = *source;
	sourceValue.amountMb = static_cast<std::uint16_t>(sourceValue.amountMb - moved);
	if (sourceValue.amountMb == 0 && sourceValue.origin != FluidOrigin::Sea)
	{
		sourceValue.matter = kAirMatter;
		sourceValue.origin = FluidOrigin::None;
	}
	WriteCell(from, sourceValue);

	ActivateAround(from);
	ActivateAround(to);
	return moved;
}

std::uint16_t FluidField::Deposit(FluidCell fluid, GridCell to, const VoxelWorld &world)
{
	if (fluid.amountMb == 0 || !CanAccept(to, fluid.matter, world))
	{
		return fluid.amountMb;
	}
	const FluidCell *target = At(to);
	const std::uint16_t targetAmount = target != nullptr ? target->amountMb : 0;
	const std::uint16_t room = static_cast<std::uint16_t>(kFullFluidCellMb - targetAmount);
	const std::uint16_t moved = std::min(fluid.amountMb, room);
	if (moved == 0)
	{
		return fluid.amountMb;
	}

	FluidCell targetValue;
	targetValue.amountMb = AddAmount(targetAmount, moved);
	if (target != nullptr && targetAmount > 0)
	{
		targetValue.matter = target->matter;
		targetValue.origin = target->origin;
		targetValue.flowEast = target->flowEast;
		targetValue.flowNorth = target->flowNorth;
	}
	else if (target != nullptr && target->origin == FluidOrigin::Sea)
	{
		targetValue.matter = target->matter;
		targetValue.origin = FluidOrigin::Sea;
		targetValue.flowEast = target->flowEast;
		targetValue.flowNorth = target->flowNorth;
	}
	else
	{
		targetValue.matter = fluid.matter;
		targetValue.origin = FluidOrigin::None;
		targetValue.flowEast = fluid.flowEast;
		targetValue.flowNorth = fluid.flowNorth;
	}
	WriteCell(to, targetValue);
	ActivateAround(to);
	return static_cast<std::uint16_t>(fluid.amountMb - moved);
}

void FluidField::PressurizeAround(GridCell cell)
{
	if (!Contains(cell))
	{
		return;
	}
	m_pressure.insert(cell);
	for (const GridCell offset : kNeighborOffsets)
	{
		const GridCell neighbor{cell.x + offset.x, cell.y + offset.y, cell.z + offset.z};
		if (Contains(neighbor))
		{
			m_pressure.insert(neighbor);
		}
	}
}

void FluidField::PropagatePressure(GridCell cell, const VoxelWorld &world)
{
	const FluidCell *source = At(cell);
	if (source == nullptr || source->amountMb == 0 || source->matter == kAirMatter)
	{
		return;
	}

	// The head is the first full 7/7 tile of the path: the source itself for an
	// endless sea, otherwise the full tile under the falling water. Pressure may
	// reach that level but never above it.
	GridCell start = cell;
	int head = cell.y;
	if (source->origin != FluidOrigin::Sea)
	{
		start = GridCell{cell.x, cell.y - 1, cell.z};
		const FluidCell *below = At(start);
		if (below == nullptr || below->amountMb < kFullFluidCellMb || below->matter != source->matter)
		{
			return;
		}
		head = start.y;
	}

	std::set<GridCell> visited;
	std::vector<GridCell> frontier;
	visited.insert(start);
	frontier.push_back(start);
	GridCell destination{};
	bool found = false;
	for (std::size_t index = 0; index < frontier.size() && !found && visited.size() <= kPressureSearchLimit; ++index)
	{
		const GridCell current = frontier[index];
		for (const GridCell offset : kPressureOffsets)
		{
			const GridCell next{current.x + offset.x, current.y + offset.y, current.z + offset.z};
			// Pressure moves through full water only, orthogonally, and never up
			// above the first full tile.
			if (!Contains(next) || next.y > head || visited.count(next) != 0)
			{
				continue;
			}
			const Block *block = world.GetBlock(next);
			if (block == nullptr || world.Registry().Occupied(block->state))
			{
				continue;
			}
			const FluidCell *fluid = At(next);
			if (fluid != nullptr && fluid->amountMb >= kFullFluidCellMb && fluid->matter == source->matter)
			{
				visited.insert(next);
				frontier.push_back(next);
				continue;
			}
			if (!CanAccept(next, source->matter, world))
			{
				continue;
			}
			destination = next;
			found = true;
			break;
		}
	}
	if (!found)
	{
		return;
	}

	// The full body acts as a pipe: the water is taken from the source cell and
	// appears at the far end. One level per tick, as in Dwarf Fortress.
	if (Transfer(cell, destination, kPressureLevelMb, world) > 0)
	{
		// A source keeps pushing until the reachable space is full.
		m_pressure.insert(cell);
	}
}

void FluidField::OnBlockChanged(GridCell cell, const VoxelWorld &world)
{
	if (!Contains(cell))
	{
		return;
	}

	const Block *block = world.GetBlock(cell);
	const bool isSolid = block != nullptr && world.Registry().Occupied(block->state);
	if (isSolid)
	{
		const FluidCell *current = At(cell);
		if (current != nullptr && current->amountMb > 0)
		{
			FluidCell displaced = *current;
			WriteCell(cell, FluidCell{});
			// First the cell above (the water level rises), then the sides; what
			// finds no room is lost (docs/tasks/MVP_FLUID_FIELD.md, step 3).
			const std::array<GridCell, 5> targets{
			    GridCell{cell.x, cell.y + 1, cell.z}, GridCell{cell.x - 1, cell.y, cell.z},
			    GridCell{cell.x + 1, cell.y, cell.z}, GridCell{cell.x, cell.y, cell.z - 1},
			    GridCell{cell.x, cell.y, cell.z + 1}};
			for (const GridCell target : targets)
			{
				displaced.amountMb = Deposit(displaced, target, world);
				if (displaced.amountMb == 0)
				{
					break;
				}
			}
		}
	}

	// A dug cell must be noticed by the fluid around it, and a placed block must
	// let the fluid settle against its new shape.
	ActivateAround(cell);
	// Digging may open a tunnel under a full body of water: pressure can now
	// reach it through the full cells.
	PressurizeAround(cell);
	MarkAroundModified(cell);
}

void FluidField::Step(const VoxelWorld &world)
{
	if (m_active.empty() && m_pressure.empty())
	{
		return;
	}

	// Work on a snapshot and start the next tick with only what changed, so a
	// calm sea does not stay active forever.
	const std::vector<GridCell> active(m_active.begin(), m_active.end());
	m_active.clear();
	for (const GridCell cell : active)
	{
		FluidCell *current = At(cell);
		if (current == nullptr)
		{
			continue;
		}

		if (current->origin == FluidOrigin::Sea && current->amountMb < kFullFluidCellMb)
		{
			// The sea has no bottom: a cell drained into a hole fills again.
			if (current->matter != kAirMatter)
			{
				FluidCell restored = *current;
				restored.amountMb = kFullFluidCellMb;
				WriteCell(cell, restored);
				ActivateAround(cell);
				// A refilled sea source can push water through a tunnel it is
				// connected to, like a river in Dwarf Fortress.
				m_pressure.insert(cell);
			}
		}

		current = At(cell);
		if (current == nullptr || current->amountMb == 0)
		{
			continue;
		}

		// Down first: a fluid falls into every open cell below it.
		const GridCell below{cell.x, cell.y - 1, cell.z};
		if (CanAccept(below, current->matter, world))
		{
			Transfer(cell, below, current->amountMb, world);
		}

		current = At(cell);
		if (current == nullptr || current->amountMb == 0)
		{
			continue;
		}

		// Then sideways: level out with the neighbours, half the difference at a
		// time. Never up, never into another fluid.
		for (const GridCell offset : kSideOffsets)
		{
			const GridCell neighbor{cell.x + offset.x, cell.y, cell.z + offset.z};
			if (!CanAccept(neighbor, current->matter, world))
			{
				continue;
			}
			const FluidCell *target = At(neighbor);
			const std::uint16_t targetAmount = target != nullptr ? target->amountMb : 0;
			if (current->amountMb <= targetAmount + 1)
			{
				continue;
			}
			const std::uint16_t share =
			    static_cast<std::uint16_t>((current->amountMb - targetAmount) / kSideFlowDivisor);
			if (share == 0)
			{
				continue;
			}
			Transfer(cell, neighbor, share, world);
			current = At(cell);
			if (current == nullptr || current->amountMb == 0)
			{
				break;
			}
		}
	}

	// Pressure is a second pass: it uses the full cells left by gravity and
	// diffusion as a pipe, and is the part that can move water to a distant tile
	// or back up a U-bend.
	if (m_pressure.empty())
	{
		return;
	}
	const std::vector<GridCell> pressure(m_pressure.begin(), m_pressure.end());
	m_pressure.clear();
	for (const GridCell cell : pressure)
	{
		PropagatePressure(cell, world);
	}
}
} // namespace Infinitesia

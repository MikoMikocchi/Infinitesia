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
	const ChunkCoordinate coordinate{cell.x / kChunkSize, cell.y / kChunkSize, cell.z / kChunkSize};
	FluidChunk *chunk = nullptr;
	const auto found = m_chunks.find(coordinate);
	if (found != m_chunks.end())
	{
		chunk = &found->second;
	}
	FluidCell *current = chunk != nullptr ? &chunk->cells[IndexIn(*chunk, cell)] : nullptr;
	if (current != nullptr && *current == value)
	{
		return false;
	}
	if (current == nullptr)
	{
		// A dry cell in a dry chunk needs no storage, unless it is part of a
		// river channel: that flag must survive until the water comes back.
		if (value.amountMb == 0 && !value.river)
		{
			return false;
		}
		chunk = &GetOrCreateChunk(coordinate);
		current = &chunk->cells[IndexIn(*chunk, cell)];
	}
	*current = value;
	++m_revision;
	++chunk->revision;
	chunk->hasSurface = true;
	// The top row of a chunk is the cell above the chunk below: the surface quad
	// of that chunk depends on it too, and water may have left its top row.
	if (cell.y % kChunkSize == 0)
	{
		TouchChunk(ChunkCoordinate{coordinate.x, coordinate.y - 1, coordinate.z}, true);
	}
	return true;
}

void FluidField::TouchChunk(ChunkCoordinate coordinate, bool mayExposeSurface)
{
	const auto found = m_chunks.find(coordinate);
	if (found == m_chunks.end())
	{
		return;
	}
	++found->second.revision;
	if (mayExposeSurface)
	{
		found->second.hasSurface = true;
	}
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
	// Without this chunk the one below loses the water that covered its top row,
	// so its surface may appear again.
	TouchChunk(ChunkCoordinate{coordinate.x, coordinate.y - 1, coordinate.z}, true);
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
				// A river is fed at its upstream end, not at every surface
				// cell: the water then has to flow downstream, so a dam stops
				// the section below it instead of every cell refilling itself.
				// The upstream neighbour is taken from the flow direction; if
				// it is outside the grid or not a river, this cell is the
				// source. A flat river with no flow keeps its old behaviour.
				std::uint16_t source = 0;
				if (isTop && column.origin == FluidOrigin::River)
				{
					const int stepX = column.flowEast > 0.0 ? -1 : (column.flowEast < 0.0 ? 1 : 0);
					const int stepZ = column.flowNorth > 0.0 ? -1 : (column.flowNorth < 0.0 ? 1 : 0);
					bool upstream = stepX == 0 && stepZ == 0;
					if (!upstream)
					{
						const int upstreamX = x + stepX;
						const int upstreamZ = z + stepZ;
						if (upstreamX < 0 || upstreamX >= m_gridWidth || upstreamZ < 0 ||
						    upstreamZ >= m_gridDepth)
						{
							upstream = true;
						}
						else
						{
							upstream = terrain.FluidAt(upstreamX, upstreamZ).origin != FluidOrigin::River;
						}
					}
					source = upstream ? amount : static_cast<std::uint16_t>(0);
				}
				const bool riverCell = column.origin == FluidOrigin::River;
				fluidChunk.cells[index] = {column.matter, amount, column.origin, column.flowEast, column.flowNorth,
				                           source, riverCell};
				// Deep full cells are a static body. Only the surface cell has to
				// settle; digging activates the surrounding water through
				// OnBlockChanged, so a whole ocean does not have to be stepped every
				// tick.
				if (isTop)
				{
					m_active.insert({x, y, z});
					// This chunk holds the top of the column, so it is the one that
					// draws the water surface. Deeper chunks draw nothing.
					fluidChunk.hasSurface = true;
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
	// The chunk below used to see open air (or no fluid) above itself; with this
	// chunk loaded its top rows may now be covered by water.
	TouchChunk(ChunkCoordinate{chunk.coordinate.x, chunk.coordinate.y - 1, chunk.coordinate.z});
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

bool FluidField::CanRiverFlowInto(GridCell from, GridCell to, const FluidCell &river,
                                  const VoxelWorld &world) const
{
	const FluidCell *target = At(to);
	if (target != nullptr && target->river)
	{
		return true;
	}
	const double downstream = river.flowEast * static_cast<double>(to.x - from.x) +
	                          river.flowNorth * static_cast<double>(to.z - from.z);
	// A river may flow into a standing body of water downstream: a lake or the
	// sea is the natural end of a river, not a bank.
	if (target != nullptr && target->amountMb > 0 &&
	    (target->origin == FluidOrigin::Lake || target->origin == FluidOrigin::Sea))
	{
		return downstream > 0.0;
	}
	// A river can always fall into an open cell below it.
	if (to.y < from.y)
	{
		return true;
	}
	// A cell whose natural ground is solid must have been dug out by the
	// player: that is a breach, and the river is allowed to flood it.
	const BlockState natural = world.GroundStateAt(to);
	if (world.Registry().Occupied(natural))
	{
		return true;
	}
	// Only the surface of the river can spill downstream. A deep cell leaking
	// sideways through the bank is not a waterfall, it is a hole in the bank.
	const FluidCell *above = At(GridCell{from.x, from.y + 1, from.z});
	if (above != nullptr && above->river)
	{
		return false;
	}
	// Natural air is allowed only downstream and only when there is a dry drop
	// below it: that is a waterfall. A natural-air cell with water below is the
	// air above a pool, and a flat one is a bank at the water level; letting
	// the river crawl onto either is the thin film the crest should prevent.
	if (downstream <= 0.0)
	{
		return false;
	}
	const Block *below = world.GetBlock({to.x, to.y - 1, to.z});
	if (below == nullptr || world.Registry().Occupied(below->state))
	{
		return false;
	}
	const FluidCell *belowFluid = At(GridCell{to.x, to.y - 1, to.z});
	return belowFluid == nullptr || belowFluid->amountMb == 0;
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
	// Only an existing river-channel cell stays a river cell. Water that
	// breaches the bank becomes ordinary water and can spread.
	targetValue.river = target != nullptr && target->river;
	if (target != nullptr && targetAmount > 0)
	{
		// The receiver already held this fluid: keep what it was.
		targetValue.matter = target->matter;
		targetValue.origin = target->origin;
		targetValue.flowEast = target->flowEast;
		targetValue.flowNorth = target->flowNorth;
		targetValue.sourceAmountMb = target->sourceAmountMb;
	}
	else if (target != nullptr && target->origin == FluidOrigin::Sea)
	{
		// A drained sea source stays a sea source: the sea is endless.
		targetValue.matter = target->matter;
		targetValue.origin = FluidOrigin::Sea;
		targetValue.flowEast = target->flowEast;
		targetValue.flowNorth = target->flowNorth;
		targetValue.sourceAmountMb = target->sourceAmountMb;
	}
	else if (target != nullptr && target->sourceAmountMb > 0)
	{
		// A drained river source is still a source: the water that reaches it
		// from a neighbour must not erase its upstream feed.
		targetValue.matter = target->matter;
		targetValue.origin = target->origin;
		targetValue.flowEast = target->flowEast;
		targetValue.flowNorth = target->flowNorth;
		targetValue.sourceAmountMb = target->sourceAmountMb;
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
	if (sourceValue.amountMb == 0 && sourceValue.origin != FluidOrigin::Sea && sourceValue.sourceAmountMb == 0)
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
	targetValue.river = target != nullptr && target->river;
	if (target != nullptr && targetAmount > 0)
	{
		targetValue.matter = target->matter;
		targetValue.origin = target->origin;
		targetValue.flowEast = target->flowEast;
		targetValue.flowNorth = target->flowNorth;
		targetValue.sourceAmountMb = target->sourceAmountMb;
	}
	else if (target != nullptr && target->origin == FluidOrigin::Sea)
	{
		targetValue.matter = target->matter;
		targetValue.origin = FluidOrigin::Sea;
		targetValue.flowEast = target->flowEast;
		targetValue.flowNorth = target->flowNorth;
		targetValue.sourceAmountMb = target->sourceAmountMb;
	}
	else if (target != nullptr && target->sourceAmountMb > 0)
	{
		// Keep the upstream feed of a drained river cell that is being filled
		// from the side.
		targetValue.matter = target->matter;
		targetValue.origin = target->origin;
		targetValue.flowEast = target->flowEast;
		targetValue.flowNorth = target->flowNorth;
		targetValue.sourceAmountMb = target->sourceAmountMb;
	}
	else
	{
		// Displaced water is no longer in its channel: it does not keep the
		// river's upstream feed.
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
	// A river is fed by its upstream source and by gravity, not by pressure.
	// Letting a distant sea or lake use the full river as a pipe would teleport
	// water straight into a bank or a dug room beside it.
	if (source->river)
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
			// A river channel is not a pressure pipe: neither crossing it nor
			// depositing into it is allowed for a non-river source.
			if (fluid != nullptr && fluid->river)
			{
				continue;
			}
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
			// The cell keeps its channel flag while the block stands there, so
			// removing the block later opens a river cell again instead of a
			// permanent hole. The water itself is pushed out normally.
			FluidCell emptied;
			emptied.river = displaced.river;
			WriteCell(cell, emptied);
			// First the cell above (the water level rises), then the sides; what
			// finds no room is lost (docs/tasks/MVP_FLUID_FIELD.md, step 3).
			const std::array<GridCell, 5> targets{
			    GridCell{cell.x, cell.y + 1, cell.z}, GridCell{cell.x - 1, cell.y, cell.z},
			    GridCell{cell.x + 1, cell.y, cell.z}, GridCell{cell.x, cell.y, cell.z - 1},
			    GridCell{cell.x, cell.y, cell.z + 1}};
			for (const GridCell target : targets)
			{
				if (displaced.river && !CanRiverFlowInto(cell, target, displaced, world))
				{
					continue;
				}
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

bool FluidField::IsOpenBoundary(GridCell cell, const FluidCell &fluid) const
{
	const bool noFlow = fluid.flowEast == 0.0 && fluid.flowNorth == 0.0;

	// The outer ring of the site window: only water that actually flows out
	// leaves. A river running parallel to the edge is not cut by the invisible
	// boundary; ordinary water without a flow is drained as before.
	if (cell.x == 0 || cell.x == m_gridWidth - 1 || cell.z == 0 || cell.z == m_gridDepth - 1)
	{
		const double normalX = cell.x == 0 ? -1.0 : (cell.x == m_gridWidth - 1 ? 1.0 : 0.0);
		const double normalZ = cell.z == 0 ? -1.0 : (cell.z == m_gridDepth - 1 ? 1.0 : 0.0);
		if (noFlow || fluid.flowEast * normalX + fluid.flowNorth * normalZ > 0.0)
		{
			return true;
		}
	}

	// The sea is an endless sink as well. Only a river that flows into it is
	// drained; one that runs along the coast keeps its water. A sea above or
	// below the river cell is always a sink.
	const auto isSea = [this](GridCell neighbour) {
		const FluidCell *sea = At(neighbour);
		return sea != nullptr && sea->origin == FluidOrigin::Sea && sea->amountMb > 0;
	};
	for (const GridCell offset : kNeighborOffsets)
	{
		const GridCell neighbour{cell.x + offset.x, cell.y + offset.y, cell.z + offset.z};
		if (!isSea(neighbour))
		{
			continue;
		}
		if (offset.y != 0 || noFlow || fluid.flowEast * offset.x + fluid.flowNorth * offset.z > 0.0)
		{
			return true;
		}
	}
	return false;
}

void FluidField::DrainOpenBoundaries(const std::vector<GridCell> &active)
{
	// Work on the cells that were awake at the start of this tick. A cell that
	// receives water later is activated by Transfer and is drained next tick;
	// that is enough to keep an open edge from filling up.
	for (const GridCell cell : active)
	{
		const FluidCell *current = At(cell);
		if (current == nullptr || current->amountMb == 0)
		{
			continue;
		}
		// A sea is endless and a lake is a real finite body: neither is a drain.
		if (current->origin == FluidOrigin::Sea || current->origin == FluidOrigin::Lake)
		{
			continue;
		}
		// At the world edge any water leaves the model. At the coast only river
		// water is drained: ordinary water next to a sea source (a U-bend, a
		// dug hole) must stay put.
		const bool atWorldEdge =
		    cell.x == 0 || cell.x == m_gridWidth - 1 || cell.z == 0 || cell.z == m_gridDepth - 1;
		const bool riverWater = current->river || current->flowEast != 0.0 || current->flowNorth != 0.0;
		if (!riverWater && !atWorldEdge)
		{
			continue;
		}
		if (!IsOpenBoundary(cell, *current))
		{
			continue;
		}
		// Remove the water but keep the cell's channel identity and its
		// upstream feed: the flow refills it in the same tick, so the river
		// reaches the edge instead of ending one cell short.
		FluidCell drained = *current;
		drained.amountMb = 0;
		if (!drained.river)
		{
			drained.origin = FluidOrigin::None;
		}
		WriteCell(cell, drained);
		ActivateAround(cell);
	}
}

void FluidField::Step(const VoxelWorld &world)
{
	if (m_active.empty() && m_pressure.empty())
	{
		return;
	}

	// Open sinks first: remove the water that reached the edge on the previous
	// tick, then let the active loop below refill the boundary cell from
	// upstream in the same tick. The river visually reaches the edge; only the
	// water that flowed out is gone.
	{
		const std::vector<GridCell> sinks(m_active.begin(), m_active.end());
		DrainOpenBoundaries(sinks);
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

		// Only the upstream cells of a river carry a source. Refilling to the
		// level the channel had when it was generated keeps the source from
		// running dry; a downstream cell has no source and is fed by the flow
		// from above, so a dam really separates the two sections.
		if (current->origin == FluidOrigin::River && current->sourceAmountMb > current->amountMb)
		{
			FluidCell refilled = *current;
			refilled.amountMb = current->sourceAmountMb;
			WriteCell(cell, refilled);
			ActivateAround(cell);
		}

		current = At(cell);
		if (current == nullptr || current->amountMb == 0)
		{
			continue;
		}

		// Down first: a fluid falls into every open cell below it. A river is
		// ordinary water in a carved channel now: a bed that was dug out is a
		// breach, and the water is allowed to follow it.
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

		// A river also carries its water downstream along its own flow. Without
		// this advection the feed at the upstream end only spreads by diffusion
		// and a long river slowly starves from its mouth; with it, a dam really
		// separates the two sections.
		if (current->river && (current->flowEast != 0.0 || current->flowNorth != 0.0))
		{
			const int stepX = current->flowEast > 0.0 ? 1 : (current->flowEast < 0.0 ? -1 : 0);
			const int stepZ = current->flowNorth > 0.0 ? 1 : (current->flowNorth < 0.0 ? -1 : 0);
			const GridCell downstream{cell.x + stepX, cell.y, cell.z + stepZ};
			const FluidCell *target = At(downstream);
			if (target != nullptr && target->river && CanAccept(downstream, current->matter, world))
			{
				const std::uint16_t share = static_cast<std::uint16_t>(current->amountMb / 4);
				if (share > 0)
				{
					Transfer(cell, downstream, share, world);
				}
			}
		}

		current = At(cell);
		if (current == nullptr || current->amountMb == 0)
		{
			continue;
		}

		// Then sideways: level out with the neighbours, half the difference at a
		// time. Never up, never into another fluid. The carved channel and its
		// banks are what keep a natural river in place; a dug bank is a breach
		// and is allowed to flood, as in Dwarf Fortress.
		for (const GridCell offset : kSideOffsets)
		{
			const GridCell neighbor{cell.x + offset.x, cell.y, cell.z + offset.z};
			if (!CanAccept(neighbor, current->matter, world))
			{
				continue;
			}
			if (current->river && !CanRiverFlowInto(cell, neighbor, *current, world))
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
	std::vector<GridCell> pressure;
	if (!m_pressure.empty())
	{
		pressure.assign(m_pressure.begin(), m_pressure.end());
		m_pressure.clear();
		for (const GridCell cell : pressure)
		{
			PropagatePressure(cell, world);
		}
	}

	// The sea is endless. Refill it after all movement, not only before it:
	// otherwise a sea cell that fed a river mouth or a dug tunnel ends the tick
	// one level short, which shows as a permanent dip at the coast. The refill
	// also keeps the pressure source alive for the next tick.
	const auto refillSea = [this](const std::vector<GridCell> &cells) {
		for (const GridCell cell : cells)
		{
			FluidCell *sea = At(cell);
			if (sea == nullptr || sea->origin != FluidOrigin::Sea || sea->matter == kAirMatter ||
			    sea->amountMb >= kFullFluidCellMb)
			{
				continue;
			}
			FluidCell restored = *sea;
			restored.amountMb = kFullFluidCellMb;
			WriteCell(cell, restored);
			ActivateAround(cell);
			m_pressure.insert(cell);
		}
	};
	refillSea(active);
	refillSea(pressure);
}
} // namespace Infinitesia

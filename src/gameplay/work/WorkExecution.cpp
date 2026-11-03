#include "gameplay/work/WorkSystem.h"
#include "gameplay/work/WorkMovement.h"
#include "gameplay/work/WorkUtils.h"

#include <cmath>
#include <stdexcept>

namespace Infinitesia
{
    void WorkSystem::Tick(GameWorld& world, const Grid& grid, float fixedDeltaTime)
    {
        if (!std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f)
        {
            throw std::invalid_argument("Work system time step must be finite and greater than zero.");
        }

        for (std::size_t index = 0; index < m_orders.size();)
        {
            const bool orderIsValid = WorkInternal::AdvanceOrder(*this, m_orders[index], world, grid, fixedDeltaTime);
            const WorldObject* worker = world.entities.FindObject(m_orders[index].worker);
            const WorldObject* targetObject = m_orders[index].targetObject.has_value()
                ? world.entities.FindObject(*m_orders[index].targetObject)
                : nullptr;
            const Block* targetBlock = m_orders[index].type == WorkType::DigBlock
                ? world.terrain.GetBlock(m_orders[index].targetCell)
                : nullptr;
            if (!orderIsValid || worker == nullptr || worker->type != WorldObjectType::Worker ||
                (m_orders[index].type == WorkType::ChopTree &&
                 (targetObject == nullptr || targetObject->type != WorldObjectType::Tree)) ||
                (m_orders[index].type == WorkType::DigBlock &&
                 (targetBlock == nullptr || targetBlock->type == BlockType::Air)))
            {
                m_orders.erase(m_orders.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }

            if (m_orders[index].type == WorkType::MoveToCell &&
                m_orders[index].phase == WorkPhase::Working)
            {
                m_orders.erase(m_orders.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }

            if (m_orders[index].phase == WorkPhase::Working &&
                m_orders[index].workProgress >= GetWorkDuration(m_orders[index]))
            {
                if (m_orders[index].type == WorkType::ChopTree)
                {
                    world.entities.TransformObject(
                        *m_orders[index].targetObject,
                        WorldObjectType::Item,
                        Item{ ItemType::Logs, m_config.logsPerTree });
                }
                else
                {
                    world.terrain.SetBlockType(m_orders[index].targetCell, BlockType::Air);
                }
                m_orders.erase(m_orders.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }

            ++index;
        }
    }

    WorkerActivity WorkSystem::GetActivity(WorldObjectId worker) const
    {
        for (const WorkOrder& order : m_orders)
        {
            if (order.worker == worker)
            {
                return order.phase == WorkPhase::Moving
                    ? WorkerActivity::Moving
                    : WorkerActivity::Working;
            }
        }
        return WorkerActivity::Idle;
    }

    float WorkSystem::GetWorkProgress(WorldObjectId worker) const
    {
        for (const WorkOrder& order : m_orders)
        {
            if (order.worker == worker && order.phase == WorkPhase::Working)
            {
                return order.workProgress / GetWorkDuration(order);
            }
        }
        return 0.0f;
    }

    std::vector<GridCell> WorkSystem::GetRemainingPath(WorldObjectId worker) const
    {
        for (const WorkOrder& order : m_orders)
        {
            if (order.worker == worker && order.phase == WorkPhase::Moving &&
                order.nextPathCell < order.path.size())
            {
                return {
                    order.path.begin() + static_cast<std::ptrdiff_t>(order.nextPathCell),
                    order.path.end()
                };
            }
        }
        return {};
    }
}

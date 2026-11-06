#include "domains/natural/planetary/world/TheaterCache.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "domains/natural/planetary/world/WorldGenerator.h"

namespace Infinitesia
{
TheaterCache::TheaterCache(const PlanetMap &planet, PlanetId planetId, std::size_t capacity)
    : m_planet(planet), m_planetId(planetId), m_capacity(std::max<std::size_t>(1, capacity))
{
}

TheaterCache::~TheaterCache()
{
	// std::async's future blocks in its destructor as well, but doing it here
	// makes the wait explicit and keeps the map alive until the workers finish.
	std::vector<std::future<std::shared_ptr<const WorldMap>>> jobs;
	{
		const std::lock_guard lock(m_mutex);
		for (auto &[key, pending] : m_pending)
		{
			if (pending.job.valid())
			{
				jobs.push_back(std::move(pending.job));
			}
		}
		m_pending.clear();
	}
	for (auto &job : jobs)
	{
		job.wait();
	}
}

bool TheaterCache::IsThisPlanet(const TheaterKey &key) const noexcept
{
	return key.planet == m_planetId;
}

TheaterKey TheaterCache::KeyAt(const GeographicCoordinates &position) const noexcept
{
	const int rows = std::max(1, m_planet.GetLatitudeBandCount());
	const int columns = std::max(1, m_planet.GetLongitudeSegmentCount());
	TheaterKey key;
	key.planet = m_planetId;
	key.cell.latitude =
	    std::clamp(static_cast<int>((position.latitudeDegrees + 90.0) / 180.0 * rows), 0, rows - 1);
	key.cell.longitude =
	    std::clamp(static_cast<int>(position.longitudeDegrees / 360.0 * columns), 0, columns - 1);
	return key;
}

TheaterKey TheaterCache::KeyAt(CartesianPoint3D direction) const noexcept
{
	return KeyAt(PlanetaryCoordinates::ToGeographic(direction));
}

GeographicCoordinates TheaterCache::Anchor(const TheaterKey &key) const noexcept
{
	return m_planet.GetPlanet().GetRegionCenter(key.cell.latitude, key.cell.longitude);
}

void TheaterCache::StartGeneration(const TheaterKey &key)
{
	const PlanetMap *planet = &m_planet;
	const GeographicCoordinates anchor = Anchor(key);
	Pending pending;
	pending.job = std::async(std::launch::async, [planet, anchor] {
		return std::make_shared<const WorldMap>(WorldGenerator(*planet).Generate(anchor));
	});
	m_pending.emplace(key, std::move(pending));
}

void TheaterCache::Touch(std::list<TheaterKey>::iterator order)
{
	m_recent.splice(m_recent.begin(), m_recent, order);
}

void TheaterCache::EvictOverflow()
{
	while (m_entries.size() > m_capacity && !m_recent.empty())
	{
		const TheaterKey victim = m_recent.back();
		m_recent.pop_back();
		m_entries.erase(victim);
	}
}

void TheaterCache::InsertReady(const TheaterKey &key, std::shared_ptr<const WorldMap> world)
{
	const auto found = m_entries.find(key);
	if (found != m_entries.end())
	{
		found->second.world = std::move(world);
		Touch(found->second.order);
		return;
	}

	m_recent.push_front(key);
	m_entries.emplace(key, Entry{std::move(world), m_recent.begin()});
	EvictOverflow();
}

void TheaterCache::CollectFinished(const TheaterKey *keep)
{
	// A player can start several previews faster than they finish. Without this
	// sweep the abandoned futures would hold their finished maps forever; move
	// them into the LRU (which discards the surplus) as soon as they are ready.
	for (auto pending = m_pending.begin(); pending != m_pending.end();)
	{
		if ((keep != nullptr && *keep == pending->first) || !pending->second.job.valid() ||
		    pending->second.job.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		{
			++pending;
			continue;
		}

		const TheaterKey finished = pending->first;
		std::shared_ptr<const WorldMap> world;
		try
		{
			world = pending->second.job.get();
		}
		catch (...)
		{
			pending = m_pending.erase(pending);
			continue;
		}
		pending = m_pending.erase(pending);
		InsertReady(finished, std::move(world));
	}
}

std::shared_ptr<const WorldMap> TheaterCache::GetOrCreate(const TheaterKey &key)
{
	if (!IsThisPlanet(key))
	{
		throw std::invalid_argument("Theater key names a planet this cache does not own.");
	}

	const std::lock_guard lock(m_mutex);

	if (const auto found = m_entries.find(key); found != m_entries.end())
	{
		Touch(found->second.order);
		return found->second.world;
	}

	CollectFinished(&key);

	if (const auto pending = m_pending.find(key); pending != m_pending.end())
	{
		if (!pending->second.job.valid() ||
		    pending->second.job.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		{
			return nullptr;
		}

		// Collect the finished worker. A generation error propagates to the
		// caller once, like any other future, but the failed job is removed so a
		// later request can try again.
		std::shared_ptr<const WorldMap> world;
		try
		{
			world = pending->second.job.get();
		}
		catch (...)
		{
			m_pending.erase(pending);
			throw;
		}
		m_pending.erase(pending);
		InsertReady(key, std::move(world));
		return m_entries.at(key).world;
	}

	StartGeneration(key);
	return nullptr;
}

std::shared_ptr<const WorldMap> TheaterCache::WaitFor(const TheaterKey &key)
{
	for (;;)
	{
		if (std::shared_ptr<const WorldMap> world = GetOrCreate(key))
		{
			return world;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

std::shared_ptr<const WorldMap> TheaterCache::Find(const TheaterKey &key)
{
	const std::lock_guard lock(m_mutex);
	const auto found = m_entries.find(key);
	if (found == m_entries.end())
	{
		return nullptr;
	}
	Touch(found->second.order);
	return found->second.world;
}

std::shared_ptr<const WorldMap> TheaterCache::Peek(const TheaterKey &key) const
{
	const std::lock_guard lock(m_mutex);
	const auto found = m_entries.find(key);
	return found != m_entries.end() ? found->second.world : nullptr;
}

bool TheaterCache::IsLoading(const TheaterKey &key) const
{
	const std::lock_guard lock(m_mutex);
	return m_pending.find(key) != m_pending.end();
}

bool TheaterCache::IsReady(const TheaterKey &key) const
{
	const std::lock_guard lock(m_mutex);
	return m_entries.find(key) != m_entries.end();
}

std::size_t TheaterCache::Size() const noexcept
{
	const std::lock_guard lock(m_mutex);
	return m_entries.size();
}
} // namespace Infinitesia

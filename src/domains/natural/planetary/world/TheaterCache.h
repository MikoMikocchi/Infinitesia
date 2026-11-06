#pragma once

#include <cstddef>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "core/PlanetaryCoordinates.h"
#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/world/TheaterKey.h"
#include "domains/natural/planetary/world/WorldMap.h"

namespace Infinitesia
{
// Shared, bounded cache of theater maps. A miss starts the expensive
// WorldGenerator on a worker thread and returns nullptr; calling GetOrCreate
// again with the same key returns the map as soon as it is ready. The most
// recently used theaters stay in memory, so revisiting an area is a cache hit
// instead of a second generation.
//
// The cache is bound to one PlanetMap (the home planet today). The key still
// carries the PlanetId: the map never lets a cell of another planet resolve to
// this planet's theater.
class TheaterCache
{
public:
	static constexpr std::size_t kDefaultCapacity = 8;

	TheaterCache(const PlanetMap &planet, PlanetId planetId = kHomePlanet,
	             std::size_t capacity = kDefaultCapacity);
	~TheaterCache();

	TheaterCache(const TheaterCache &) = delete;
	TheaterCache &operator=(const TheaterCache &) = delete;

	// The ready theater, or nullptr while it is being generated. A hit also
	// marks the entry as most recently used.
	std::shared_ptr<const WorldMap> GetOrCreate(const TheaterKey &key);

	// Blocking convenience for tools and tests. The UI must not call it.
	std::shared_ptr<const WorldMap> WaitFor(const TheaterKey &key);

	// Look up a ready theater without generating it; touching it if found
	// (`Find`) or not (`Peek`).
	std::shared_ptr<const WorldMap> Find(const TheaterKey &key);
	std::shared_ptr<const WorldMap> Peek(const TheaterKey &key) const;

	bool IsLoading(const TheaterKey &key) const;
	bool IsReady(const TheaterKey &key) const;

	std::size_t Size() const noexcept;
	std::size_t Capacity() const noexcept
	{
		return m_capacity;
	}

	PlanetId HomePlanet() const noexcept
	{
		return m_planetId;
	}
	const PlanetMap &Planet() const noexcept
	{
		return m_planet;
	}

	// The key whose anchor is the centre of the planet cell containing the
	// point; the inverse is Anchor().
	TheaterKey KeyAt(const GeographicCoordinates &position) const noexcept;
	TheaterKey KeyAt(CartesianPoint3D direction) const noexcept;
	GeographicCoordinates Anchor(const TheaterKey &key) const noexcept;

private:
	struct Entry
	{
		std::shared_ptr<const WorldMap> world;
		std::list<TheaterKey>::iterator order;
	};

	struct Pending
	{
		std::future<std::shared_ptr<const WorldMap>> job;
	};

	void StartGeneration(const TheaterKey &key);
	void CollectFinished(const TheaterKey *keep);
	void InsertReady(const TheaterKey &key, std::shared_ptr<const WorldMap> world);
	void Touch(std::list<TheaterKey>::iterator order);
	void EvictOverflow();
	bool IsThisPlanet(const TheaterKey &key) const noexcept;

	const PlanetMap &m_planet;
	PlanetId m_planetId;
	std::size_t m_capacity;

	mutable std::mutex m_mutex;
	std::unordered_map<TheaterKey, Entry> m_entries;
	std::unordered_map<TheaterKey, Pending> m_pending;
	std::list<TheaterKey> m_recent; // front = most recently used
};
} // namespace Infinitesia

#pragma once

#include <cstddef>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Infinitesia
{
// A content file is missing, malformed or inconsistent; the message names the
// file and the record.
class ContentError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

// The definitions of one kind of content (items, object kinds, ...) read from
// data.
//
// A definition is found by its stable string key ("item.logs"). Definitions are
// filled in while loading and only read afterwards: once loading is over,
// references and pointers to them stay valid for as long as the registry lives
// (it must not be added to or copied then). Saves must store keys, never
// positions.
//
// Def needs a `std::string key` member.
template <typename Def> class ContentRegistry
{
public:
	// Adds a definition; throws ContentError for an empty or repeated key.
	const Def &Add(Def def)
	{
		if (def.key.empty())
		{
			throw ContentError("A content record has no id");
		}
		if (m_indexByKey.contains(def.key))
		{
			throw ContentError("Duplicate content id '" + def.key + "'");
		}
		m_indexByKey.emplace(def.key, m_defs.size());
		m_defs.push_back(std::move(def));
		return m_defs.back();
	}

	// nullptr when there is no such key.
	const Def *Find(std::string_view key) const noexcept
	{
		const auto found = m_indexByKey.find(key);
		return found == m_indexByKey.end() ? nullptr : &m_defs[found->second];
	}

	// For loaders resolving references between records once every record is
	// added.
	Def *FindMutable(std::string_view key) noexcept
	{
		const auto found = m_indexByKey.find(key);
		return found == m_indexByKey.end() ? nullptr : &m_defs[found->second];
	}

	// Throws ContentError naming the key when it is missing.
	const Def &Require(std::string_view key) const
	{
		if (const Def *def = Find(key))
		{
			return *def;
		}
		throw ContentError("Unknown content id '" + std::string(key) + "'");
	}

	std::span<const Def> All() const noexcept
	{
		return m_defs;
	}
	std::size_t Size() const noexcept
	{
		return m_defs.size();
	}

private:
	std::vector<Def> m_defs;
	std::map<std::string, std::size_t, std::less<>> m_indexByKey;
};
} // namespace Infinitesia

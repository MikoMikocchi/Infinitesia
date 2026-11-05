#pragma once

#include <utility>

#include "raylib.h"

namespace Infinitesia
{
// Move-only owner of a GPU texture. Must be reset (or destroyed) while the
// GL context is still alive.
class UniqueTexture
{
public:
	UniqueTexture() noexcept = default;
	explicit UniqueTexture(Texture2D texture) noexcept : m_texture(texture)
	{
	}
	~UniqueTexture()
	{
		Reset();
	}

	UniqueTexture(const UniqueTexture &) = delete;
	UniqueTexture &operator=(const UniqueTexture &) = delete;

	UniqueTexture(UniqueTexture &&other) noexcept : m_texture(std::exchange(other.m_texture, Texture2D{}))
	{
	}

	UniqueTexture &operator=(UniqueTexture &&other) noexcept
	{
		if (this != &other)
		{
			Reset();
			m_texture = std::exchange(other.m_texture, Texture2D{});
		}
		return *this;
	}

	void Reset() noexcept
	{
		if (m_texture.id != 0)
		{
			UnloadTexture(m_texture);
			m_texture = {};
		}
	}

	bool IsValid() const noexcept
	{
		return m_texture.id != 0;
	}
	const Texture2D &Get() const noexcept
	{
		return m_texture;
	}

private:
	Texture2D m_texture{};
};
} // namespace Infinitesia

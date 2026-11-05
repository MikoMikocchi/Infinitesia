#pragma once

#include <stdexcept>

namespace Infinitesia
{
// A catalog file is missing, malformed or inconsistent; the message names the
// file and the record.
class CatalogError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};
} // namespace Infinitesia

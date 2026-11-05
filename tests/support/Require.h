#pragma once

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

// Non-fatal check: a failure is reported with the call-site line and the
// remaining checks (and test cases) still run. Cheap when the condition holds,
// which matters for per-cell loops over a whole planet map.
#define Require(condition, message)                                                                                    \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(condition))                                                                                              \
		{                                                                                                              \
			FAIL_CHECK(message);                                                                                       \
		}                                                                                                              \
	} while (0)

// Fatal variant for checks that guard a following dereference.
#define RequireFatal(condition, message)                                                                               \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(condition))                                                                                              \
		{                                                                                                              \
			FAIL(message);                                                                                             \
		}                                                                                                              \
	} while (0)

inline bool NearlyEqual(double first, double second, double relativeTolerance = 1e-10)
{
	return std::abs(first - second) <= std::max(std::abs(first), std::abs(second)) * relativeTolerance;
}

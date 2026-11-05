#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Infinitesia::Content
{
// How serious a content problem is.
enum class Severity
{
	Warning,
	Error
};

// One problem found while reading content: where it is and what is wrong
// (docs/DataDriven.md, section 4.6). A line of 0 means the place is a whole
// file or set rather than one line.
struct Diagnostic
{
	Severity severity = Severity::Error;
	std::string file;
	int line = 0;
	std::string message;
};

// Collects the problems of one load, so a whole file set can report every
// problem instead of stopping at the first (docs/DataDriven.md, section 4.6).
class Diagnostics
{
public:
	void Add(Diagnostic diagnostic);
	void Error(std::string file, int line, std::string message);
	void Warning(std::string file, int line, std::string message);

	bool Empty() const noexcept
	{
		return m_diagnostics.empty();
	}
	std::size_t Size() const noexcept
	{
		return m_diagnostics.size();
	}
	const std::vector<Diagnostic> &All() const noexcept
	{
		return m_diagnostics;
	}

	// The problems as "severity: file:line: message" lines, for a report.
	std::vector<std::string> Formatted() const;

private:
	std::vector<Diagnostic> m_diagnostics;
};
} // namespace Infinitesia::Content

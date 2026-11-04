#include "content/Diagnostics.h"

#include <utility>

namespace Infinitesia::Content {
namespace {
std::string Format(const Diagnostic& diagnostic) {
  std::string line =
      diagnostic.severity == Severity::Error ? "error: " : "warning: ";
  if (!diagnostic.file.empty()) {
    line += diagnostic.file;
    if (diagnostic.line > 0) line += ":" + std::to_string(diagnostic.line);
    line += ": ";
  }
  return line + diagnostic.message;
}
}  // namespace

void Diagnostics::Add(Diagnostic diagnostic) {
  m_diagnostics.push_back(std::move(diagnostic));
}

void Diagnostics::Error(std::string file, int line, std::string message) {
  Add({Severity::Error, std::move(file), line, std::move(message)});
}

void Diagnostics::Warning(std::string file, int line, std::string message) {
  Add({Severity::Warning, std::move(file), line, std::move(message)});
}

std::vector<std::string> Diagnostics::Formatted() const {
  std::vector<std::string> lines;
  lines.reserve(m_diagnostics.size());
  for (const Diagnostic& diagnostic : m_diagnostics) {
    lines.push_back(Format(diagnostic));
  }
  return lines;
}
}  // namespace Infinitesia::Content

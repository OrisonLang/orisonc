#include "orison/diagnostics/diagnostic_bag.hpp"

#include <sstream>
#include <utility>

namespace orison::diagnostics {

void DiagnosticBag::error(std::size_t line, std::string message) {
    entries_.push_back(Diagnostic {.line = line, .message = std::move(message)});
}

auto DiagnosticBag::has_errors() const -> bool {
    return !entries_.empty();
}

auto DiagnosticBag::entries() const -> std::vector<Diagnostic> const& {
    return entries_;
}

auto DiagnosticBag::render(std::string_view path) const -> std::string {
    std::ostringstream output;
    for (auto const& entry : entries_) {
        output << path << ":" << entry.line << ": error: " << entry.message << '\n';
    }
    return output.str();
}

}  // namespace orison::diagnostics

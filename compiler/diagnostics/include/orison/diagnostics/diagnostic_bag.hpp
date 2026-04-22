#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace orison::diagnostics {

struct Diagnostic {
    std::size_t line = 0;
    std::string message;
};

class DiagnosticBag {
public:
    void error(std::size_t line, std::string message);

    auto has_errors() const -> bool;
    auto entries() const -> std::vector<Diagnostic> const&;
    auto render(std::string_view path) const -> std::string;

private:
    std::vector<Diagnostic> entries_;
};

}  // namespace orison::diagnostics

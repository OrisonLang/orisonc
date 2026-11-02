#include "orison/driver/build_plan.hpp"

#include "orison/link/host_linker.hpp"
#include "orison/pipeline/compile_pipeline.hpp"
#include "orison/semantics/package_interface.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <algorithm>
#include <fstream>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace orison::driver {
namespace {

constexpr auto build_plan_format_version = std::size_t {1};

struct ParsedBuildModule {
    std::filesystem::path source_path;
    syntax::ModuleSyntax module;
    semantics::PackageInterface package_interface;
};

auto parse_error(std::string message) -> BuildPlanParseResult {
    return BuildPlanParseResult {.error_message = std::move(message)};
}

void remove_carriage_return(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

auto trim_leading_spaces(std::string text) -> std::string {
    auto first = text.find_first_not_of(" \t");
    return first == std::string::npos ? std::string {} : text.substr(first);
}

auto read_text_file(std::filesystem::path const& path) -> std::optional<std::string> {
    auto input = std::ifstream(path, std::ios::in | std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    auto text = std::ostringstream {};
    text << input.rdbuf();
    if (input.bad()) {
        return std::nullopt;
    }
    return text.str();
}

auto build_plan_error(std::string message) -> CompileResult {
    return CompileResult {
        .exit_code = 1,
        .stderr_text = "error: " + std::move(message) + "\n",
    };
}

auto append_unique_libraries(
    std::vector<std::string>& libraries,
    std::vector<std::string> const& additions
) -> void {
    for (auto const& library : additions) {
        if (std::ranges::find(libraries, library) == libraries.end()) {
            libraries.push_back(library);
        }
    }
}

}  // namespace

auto parse_build_plan(std::string_view text, std::filesystem::path const& base_directory)
    -> BuildPlanParseResult {
    auto input = std::istringstream {std::string(text)};
    auto line = std::string {};
    if (!std::getline(input, line)) {
        return parse_error("build plan is empty");
    }
    remove_carriage_return(line);

    auto header = std::istringstream {line};
    auto marker = std::string {};
    auto version = std::size_t {};
    auto extra = std::string {};
    if (!(header >> marker >> version) || marker != "orison-build-plan" || header >> extra) {
        return parse_error("invalid build plan header");
    }
    if (version != build_plan_format_version) {
        return parse_error("unsupported build plan version '" + std::to_string(version) + "'");
    }

    if (!std::getline(input, line)) {
        return parse_error("build plan is missing its entry package");
    }
    remove_carriage_return(line);
    auto entry_line = std::istringstream {line};
    auto entry_keyword = std::string {};
    auto plan = BuildPlan {};
    if (!(entry_line >> entry_keyword >> plan.entry_package) || entry_keyword != "entry" ||
        entry_line >> extra) {
        return parse_error("invalid build plan entry declaration at line 2");
    }

    auto package_names = std::unordered_set<std::string> {};
    auto line_number = std::size_t {2};
    while (std::getline(input, line)) {
        ++line_number;
        remove_carriage_return(line);
        auto module_line = std::istringstream {line};
        auto module_keyword = std::string {};
        auto package_name = std::string {};
        if (!(module_line >> module_keyword >> package_name) || module_keyword != "module") {
            return parse_error("invalid build plan module declaration at line " + std::to_string(line_number));
        }
        auto source_path_text = std::string {};
        std::getline(module_line, source_path_text);
        source_path_text = trim_leading_spaces(std::move(source_path_text));
        if (source_path_text.empty()) {
            return parse_error("invalid build plan module declaration at line " + std::to_string(line_number));
        }
        if (!package_names.insert(package_name).second) {
            return parse_error(
                "duplicate build plan package '" + package_name + "' at line " +
                std::to_string(line_number)
            );
        }
        auto source_path = std::filesystem::path(source_path_text);
        if (source_path.is_relative()) {
            source_path = base_directory / source_path;
        }
        plan.modules.push_back(BuildPlanModule {
            .package_name = std::move(package_name),
            .source_path = source_path.lexically_normal(),
        });
    }

    if (!package_names.contains(plan.entry_package)) {
        return parse_error("build plan entry package '" + plan.entry_package + "' has no module");
    }
    return BuildPlanParseResult {.plan = std::move(plan)};
}

auto compile_build_plan(
    std::filesystem::path const& plan_path,
    std::filesystem::path const& output_path,
    pipeline::CompilePipelineOptions const& base_options
) -> CompileResult {
    auto plan_text = read_text_file(plan_path);
    if (!plan_text.has_value()) {
        return build_plan_error("unable to read build plan '" + plan_path.string() + "'");
    }
    auto parsed_plan = parse_build_plan(*plan_text, plan_path.parent_path());
    if (parsed_plan.has_error()) {
        return build_plan_error(
            "invalid build plan '" + plan_path.string() + "': " + parsed_plan.error_message
        );
    }
    auto const& plan = *parsed_plan.plan;

    auto plan_modules = std::unordered_map<std::string, std::filesystem::path> {};
    for (auto const& module : plan.modules) {
        plan_modules.emplace(module.package_name, module.source_path);
    }

    auto parsed_modules = std::unordered_map<std::string, ParsedBuildModule> {};
    auto reachable_packages = std::vector<std::string> {};
    auto pending_packages = std::queue<std::string> {};
    auto queued_packages = std::unordered_set<std::string> {plan.entry_package};
    pending_packages.push(plan.entry_package);
    while (!pending_packages.empty()) {
        auto package_name = std::move(pending_packages.front());
        pending_packages.pop();
        auto configured_module = plan_modules.find(package_name);
        if (configured_module == plan_modules.end()) {
            return build_plan_error("build plan has no module for package '" + package_name + "'");
        }
        auto source_file = source::SourceFile::read(configured_module->second);
        if (!source_file.has_value()) {
            return build_plan_error(
                "unable to read build plan source '" + configured_module->second.string() + "'"
            );
        }
        auto parse_result = syntax::ModuleParser {}.parse(*source_file);
        if (parse_result.diagnostics.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = parse_result.diagnostics.render(configured_module->second.string()),
            };
        }
        if (parse_result.module.package_name != package_name) {
            return build_plan_error(
                "module '" + configured_module->second.string() + "' declares package '" +
                parse_result.module.package_name + "' but the build plan maps it to '" + package_name + "'"
            );
        }

        auto imported_packages = std::vector<std::string> {};
        for (auto const& import : parse_result.module.imports) {
            if (std::ranges::find(imported_packages, import.from_package) == imported_packages.end()) {
                imported_packages.push_back(import.from_package);
            }
        }
        auto package_interface = semantics::build_package_interface(parse_result.module);
        parsed_modules.emplace(
            package_name,
            ParsedBuildModule {
                .source_path = configured_module->second,
                .module = std::move(parse_result.module),
                .package_interface = std::move(package_interface),
            }
        );
        reachable_packages.push_back(package_name);

        for (auto const& imported_package : imported_packages) {
            if (!plan_modules.contains(imported_package)) {
                return build_plan_error(
                    "build plan has no module for imported package '" + imported_package +
                    "' required by '" + package_name + "'"
                );
            }
            if (queued_packages.insert(imported_package).second) {
                pending_packages.push(imported_package);
            }
        }
    }

    auto package_interfaces = std::vector<semantics::PackageInterface> {};
    package_interfaces.reserve(reachable_packages.size());
    for (auto const& package_name : reachable_packages) {
        package_interfaces.push_back(parsed_modules.at(package_name).package_interface);
    }

    auto specialization_requests = base_options.generic_function_specialization_requests;
    auto request_state = semantics::serialize_generic_function_specialization_requests(
        specialization_requests
    );
    auto object_images = std::vector<std::string> {};
    auto link_libraries = std::vector<std::string> {};
    while (true) {
        object_images.clear();
        link_libraries.clear();
        auto discovered_requests = specialization_requests;
        for (auto const& package_name : reachable_packages) {
            auto options = base_options;
            options.available_package_interfaces = package_interfaces;
            for (auto const& package_interface : base_options.available_package_interfaces) {
                if (std::ranges::none_of(
                        package_interfaces,
                        [&](semantics::PackageInterface const& available) {
                            return available.package_name == package_interface.package_name;
                        }
                    )) {
                    options.available_package_interfaces.push_back(package_interface);
                }
            }
            options.generic_function_specialization_requests = specialization_requests;

            auto result = pipeline::CompilePipeline {}.emit_object(
                parsed_modules.at(package_name).source_path,
                options
            );
            if (result.has_errors()) {
                return CompileResult {
                    .exit_code = 1,
                    .stderr_text = std::move(result.error_text),
                };
            }
            object_images.push_back(std::move(result.object_bytes));
            append_unique_libraries(link_libraries, result.link_libraries);
            discovered_requests.insert(
                discovered_requests.end(),
                result.generic_function_specialization_requests.begin(),
                result.generic_function_specialization_requests.end()
            );
        }

        auto next_request_state = semantics::serialize_generic_function_specialization_requests(
            discovered_requests
        );
        if (next_request_state == request_state) {
            break;
        }
        auto parsed_requests = semantics::parse_generic_function_specialization_requests(next_request_state);
        specialization_requests = std::move(parsed_requests.requests);
        request_state = std::move(next_request_state);
    }

    auto object_views = std::vector<std::string_view> {};
    object_views.reserve(object_images.size());
    for (auto const& object_image : object_images) {
        object_views.push_back(object_image);
    }
    auto link_result = link::HostLinker {}.link(object_views, output_path, link_libraries);
    if (link_result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stderr_text = link_result.diagnostics.render(plan_path.string()),
        };
    }
    return CompileResult {.exit_code = 0};
}

}  // namespace orison::driver

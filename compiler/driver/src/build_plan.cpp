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
    semantics::PackageInterface package_interface;
};

struct LoadedPrebuiltPackage {
    semantics::PackageInterface package_interface;
    std::string object_bytes;
    std::vector<std::string> link_libraries;
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

auto resolve_plan_path(std::string const& path_text, std::filesystem::path const& base_directory)
    -> std::filesystem::path {
    auto path = std::filesystem::path(path_text);
    if (path.is_relative()) {
        path = base_directory / path;
    }
    return path.lexically_normal();
}

auto read_file_bytes(std::filesystem::path const& path) -> std::optional<std::string> {
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

    auto package_indices = std::unordered_map<std::string, std::size_t> {};
    auto package_for = [&](std::string const& package_name) -> BuildPlanPackage& {
        auto [position, inserted] = package_indices.emplace(package_name, plan.packages.size());
        if (inserted) {
            plan.packages.push_back(BuildPlanPackage {.package_name = package_name});
        }
        return plan.packages[position->second];
    };
    auto line_number = std::size_t {2};
    while (std::getline(input, line)) {
        ++line_number;
        remove_carriage_return(line);
        auto record_line = std::istringstream {line};
        auto record_kind = std::string {};
        auto package_name = std::string {};
        if (!(record_line >> record_kind >> package_name)) {
            return parse_error("invalid build plan record at line " + std::to_string(line_number));
        }
        auto& package = package_for(package_name);

        if (record_kind == "depends") {
            auto dependency = std::string {};
            if (!(record_line >> dependency) || record_line >> extra) {
                return parse_error("invalid build plan dependency at line " + std::to_string(line_number));
            }
            if (std::ranges::find(package.dependencies, dependency) != package.dependencies.end()) {
                return parse_error(
                    "duplicate build plan dependency '" + package_name + "' -> '" + dependency +
                    "' at line " + std::to_string(line_number)
                );
            }
            package.dependencies.push_back(std::move(dependency));
            continue;
        }

        if (record_kind == "library") {
            auto library = std::string {};
            if (!(record_line >> library) || record_line >> extra) {
                return parse_error("invalid build plan library at line " + std::to_string(line_number));
            }
            if (std::ranges::find(package.link_libraries, library) != package.link_libraries.end()) {
                return parse_error(
                    "duplicate build plan library '" + library + "' for package '" + package_name +
                    "' at line " + std::to_string(line_number)
                );
            }
            package.link_libraries.push_back(std::move(library));
            continue;
        }

        auto path_text = std::string {};
        std::getline(record_line, path_text);
        path_text = trim_leading_spaces(std::move(path_text));
        if (path_text.empty()) {
            return parse_error("invalid build plan path record at line " + std::to_string(line_number));
        }
        auto path = resolve_plan_path(path_text, base_directory);
        auto destination = static_cast<std::optional<std::filesystem::path>*>(nullptr);
        if (record_kind == "module") {
            destination = &package.source_path;
        } else if (record_kind == "interface") {
            destination = &package.interface_path;
        } else if (record_kind == "object") {
            destination = &package.object_path;
        } else {
            return parse_error(
                "unknown build plan record '" + record_kind + "' at line " + std::to_string(line_number)
            );
        }
        if (destination->has_value()) {
            return parse_error(
                "duplicate build plan " + record_kind + " for package '" + package_name +
                "' at line " + std::to_string(line_number)
            );
        }
        *destination = std::move(path);
    }

    for (auto const& package : plan.packages) {
        auto has_source = package.source_path.has_value();
        auto has_interface = package.interface_path.has_value();
        auto has_object = package.object_path.has_value();
        if (has_source && (has_interface || has_object)) {
            return parse_error(
                "build plan package '" + package.package_name +
                "' mixes a source module with prebuilt artifacts"
            );
        }
        if (!has_source && (!has_interface || !has_object)) {
            return parse_error(
                "build plan package '" + package.package_name +
                "' requires either a source module or a prebuilt interface/object pair"
            );
        }
        if (has_source && (!package.dependencies.empty() || !package.link_libraries.empty())) {
            return parse_error(
                "source build plan package '" + package.package_name +
                "' cannot declare prebuilt dependencies or libraries"
            );
        }
    }
    if (!package_indices.contains(plan.entry_package)) {
        return parse_error("build plan entry package '" + plan.entry_package + "' has no package mapping");
    }
    for (auto const& package : plan.packages) {
        for (auto const& dependency : package.dependencies) {
            if (!package_indices.contains(dependency)) {
                return parse_error(
                    "build plan dependency '" + dependency + "' required by '" +
                    package.package_name + "' has no package mapping"
                );
            }
        }
    }
    return BuildPlanParseResult {.plan = std::move(plan)};
}

auto compile_build_plan(
    std::filesystem::path const& plan_path,
    std::filesystem::path const& output_path,
    pipeline::CompilePipelineOptions const& base_options
) -> CompileResult {
    auto plan_text = read_file_bytes(plan_path);
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

    auto plan_packages = std::unordered_map<std::string, BuildPlanPackage const*> {};
    for (auto const& package : plan.packages) {
        plan_packages.emplace(package.package_name, &package);
    }

    auto parsed_modules = std::unordered_map<std::string, ParsedBuildModule> {};
    auto prebuilt_packages = std::unordered_map<std::string, LoadedPrebuiltPackage> {};
    auto reachable_packages = std::vector<std::string> {};
    auto pending_packages = std::queue<std::string> {};
    auto queued_packages = std::unordered_set<std::string> {plan.entry_package};
    pending_packages.push(plan.entry_package);
    while (!pending_packages.empty()) {
        auto package_name = std::move(pending_packages.front());
        pending_packages.pop();
        auto configured_package = plan_packages.find(package_name);
        if (configured_package == plan_packages.end()) {
            return build_plan_error("build plan has no package mapping for '" + package_name + "'");
        }

        auto dependencies = std::vector<std::string> {};
        auto const& package = *configured_package->second;
        if (package.source_path.has_value()) {
            auto source_file = source::SourceFile::read(*package.source_path);
            if (!source_file.has_value()) {
                return build_plan_error(
                    "unable to read build plan source '" + package.source_path->string() + "'"
                );
            }
            auto parse_result = syntax::ModuleParser {}.parse(*source_file);
            if (parse_result.diagnostics.has_errors()) {
                return CompileResult {
                    .exit_code = 1,
                    .stderr_text = parse_result.diagnostics.render(package.source_path->string()),
                };
            }
            if (parse_result.module.package_name != package_name) {
                return build_plan_error(
                    "module '" + package.source_path->string() + "' declares package '" +
                    parse_result.module.package_name + "' but the build plan maps it to '" + package_name + "'"
                );
            }

            for (auto const& import : parse_result.module.imports) {
                if (std::ranges::find(dependencies, import.from_package) == dependencies.end()) {
                    dependencies.push_back(import.from_package);
                }
            }
            auto package_interface = semantics::build_package_interface(parse_result.module);
            parsed_modules.emplace(
                package_name,
                ParsedBuildModule {
                    .source_path = *package.source_path,
                    .package_interface = std::move(package_interface),
                }
            );
        } else {
            auto interface_text = read_file_bytes(*package.interface_path);
            if (!interface_text.has_value()) {
                return build_plan_error(
                    "unable to read prebuilt package interface '" + package.interface_path->string() + "'"
                );
            }
            auto parsed_interface = semantics::parse_package_interface(*interface_text);
            if (parsed_interface.has_error()) {
                return build_plan_error(
                    "invalid prebuilt package interface '" + package.interface_path->string() +
                    "': " + parsed_interface.error_message
                );
            }
            if (parsed_interface.package_interface->package_name != package_name) {
                return build_plan_error(
                    "prebuilt package interface '" + package.interface_path->string() +
                    "' declares package '" + parsed_interface.package_interface->package_name +
                    "' but the build plan maps it to '" + package_name + "'"
                );
            }
            auto object_bytes = read_file_bytes(*package.object_path);
            if (!object_bytes.has_value()) {
                return build_plan_error(
                    "unable to read prebuilt object '" + package.object_path->string() + "'"
                );
            }
            dependencies = package.dependencies;
            prebuilt_packages.emplace(
                package_name,
                LoadedPrebuiltPackage {
                    .package_interface = std::move(*parsed_interface.package_interface),
                    .object_bytes = std::move(*object_bytes),
                    .link_libraries = package.link_libraries,
                }
            );
        }
        reachable_packages.push_back(package_name);

        for (auto const& dependency : dependencies) {
            if (!plan_packages.contains(dependency)) {
                return build_plan_error(
                    "build plan has no package mapping for imported package '" + dependency +
                    "' required by '" + package_name + "'"
                );
            }
            if (queued_packages.insert(dependency).second) {
                pending_packages.push(dependency);
            }
        }
    }

    auto package_interfaces = std::vector<semantics::PackageInterface> {};
    package_interfaces.reserve(reachable_packages.size());
    for (auto const& package_name : reachable_packages) {
        auto parsed_module = parsed_modules.find(package_name);
        if (parsed_module != parsed_modules.end()) {
            package_interfaces.push_back(parsed_module->second.package_interface);
        } else {
            package_interfaces.push_back(prebuilt_packages.at(package_name).package_interface);
        }
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
            auto parsed_module = parsed_modules.find(package_name);
            if (parsed_module == parsed_modules.end()) {
                continue;
            }
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
                parsed_module->second.source_path,
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

    for (auto const& package_name : reachable_packages) {
        auto prebuilt_package = prebuilt_packages.find(package_name);
        if (prebuilt_package == prebuilt_packages.end()) {
            continue;
        }
        object_images.push_back(prebuilt_package->second.object_bytes);
        append_unique_libraries(link_libraries, prebuilt_package->second.link_libraries);
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

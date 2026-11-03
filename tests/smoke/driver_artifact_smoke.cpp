#include "orison/driver/compiler_app.hpp"
#include "orison/link/host_linker.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

void write_fixture(
    std::filesystem::path const& path,
    std::string_view package_name,
    std::initializer_list<std::string_view> lines
) {
    std::ofstream output(path);
    output << "package " << package_name << "\n";
    for (auto line : lines) {
        output << line << "\n";
    }
}

auto run_emit_object(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& source_path,
    std::filesystem::path const& output_path
) -> orison::driver::CompileResult {
    auto source_path_text = source_path.string();
    auto output_path_text = output_path.string();
    std::array<char const*, 5> argv {
        "orisonc",
        "--emit-object",
        source_path_text.c_str(),
        "-o",
        output_path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto run_emit_package_interface(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& source_path,
    std::filesystem::path const& output_path
) -> orison::driver::CompileResult {
    auto source_path_text = source_path.string();
    auto output_path_text = output_path.string();
    std::array<char const*, 5> argv {
        "orisonc",
        "--emit-package-interface",
        source_path_text.c_str(),
        "-o",
        output_path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto run_emit_specialization_requests(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& package_interface_path,
    std::filesystem::path const& source_path,
    std::filesystem::path const& output_path
) -> orison::driver::CompileResult {
    auto package_interface_path_text = package_interface_path.string();
    auto source_path_text = source_path.string();
    auto output_path_text = output_path.string();
    std::array<char const*, 7> argv {
        "orisonc",
        "--package-interface",
        package_interface_path_text.c_str(),
        "--emit-specialization-requests",
        source_path_text.c_str(),
        "-o",
        output_path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto run_build(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& source_path,
    std::filesystem::path const& output_path
) -> orison::driver::CompileResult {
    auto source_path_text = source_path.string();
    auto output_path_text = output_path.string();
    std::array<char const*, 5> argv {
        "orisonc",
        "--build",
        source_path_text.c_str(),
        "-o",
        output_path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto run_build_plan(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& plan_path,
    std::filesystem::path const& output_path
) -> orison::driver::CompileResult {
    auto plan_path_text = plan_path.string();
    auto output_path_text = output_path.string();
    std::array<char const*, 5> argv {
        "orisonc",
        "--build-plan",
        plan_path_text.c_str(),
        "-o",
        output_path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto run_program(orison::driver::CompilerApp const& app, std::filesystem::path const& source_path)
    -> orison::driver::CompileResult {
    auto source_path_text = source_path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        "run",
        source_path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

void assert_success_with_empty_stdout(orison::driver::CompileResult const& result) {
    assert(result.exit_code == 0);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.empty());
}

void assert_result_with_empty_output(orison::driver::CompileResult const& result, int expected_exit_code) {
    assert(result.exit_code == expected_exit_code);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.empty());
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_driver_artifact_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};
    auto scalar_path = smoke_temp_root / "artifact_scalar.or";
    write_fixture(
        scalar_path,
        "demo.artifact",
        {
            "function main() -> UInt32",
            "    42 as UInt32",
        }
    );

    auto object_path = smoke_temp_root / "artifact_scalar.o";
    auto object_result = run_emit_object(app, scalar_path, object_path);
    assert_success_with_empty_stdout(object_result);
    assert(std::filesystem::file_size(object_path) > 0);

    auto package_source_path = smoke_temp_root / "external_types.or";
    write_fixture(
        package_source_path,
        "external.types",
        {
            "public type Identifier = UInt64",
            "type InternalIdentifier = UInt64",
            "public record Pair<Left, Right>",
            "    public first: Left",
            "    public second: Right",
            "public choice Result<Value>",
            "    Ok(value: Value)",
            "public interface Reader",
            "    function read(this: shared This) -> UInt32",
        }
    );
    auto package_interface_path = smoke_temp_root / "external_types.opi";
    auto package_interface_result = run_emit_package_interface(app, package_source_path, package_interface_path);
    assert_success_with_empty_stdout(package_interface_result);
    {
        auto input = std::ifstream(package_interface_path);
        auto contents = std::string(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()
        );
        assert(
            contents ==
            "orison-package-interface 2\n"
            "package external.types\n"
            "type alias Identifier 0\n"
            "alias Identifier UInt64\n"
            "type record Pair 2 Left Right\n"
            "field Pair first Left\n"
            "field Pair second Right\n"
            "type interface Reader 0\n"
            "type choice Result 1 Value\n"
            "variant Result Ok 1 value Value\n"
        );
    }

    auto consumer_path = smoke_temp_root / "consumer.or";
    write_fixture(
        consumer_path,
        "demo.consumer",
        {
            "import",
            "    Pair from external.types",
            "function consume(value: Pair<UInt32>) -> Unit",
            "    return",
        }
    );
    auto package_interface_path_text = package_interface_path.string();
    auto consumer_path_text = consumer_path.string();
    std::array<char const*, 5> consume_interface_argv {
        "orisonc",
        "--package-interface",
        package_interface_path_text.c_str(),
        "--emit-llvm",
        consumer_path_text.c_str(),
    };
    auto consume_interface_result = app.run(
        std::span<char const* const>(consume_interface_argv.data(), consume_interface_argv.size())
    );
    assert(consume_interface_result.exit_code == 1);
    assert(consume_interface_result.stdout_text.empty());
    assert(
        consume_interface_result.stderr_text.find(
            "type 'Pair<UInt32>' supplies 1 type argument; type 'Pair' requires 2"
        ) != std::string::npos
    );

    auto generic_producer_path = smoke_temp_root / "generic producer.or";
    write_fixture(
        generic_producer_path,
        "external.generics",
        {
            "public function choose<Value>(left: Value, right: Value) -> Value",
            "    left",
        }
    );
    auto generic_interface_path = smoke_temp_root / "generic_producer.opi";
    auto generic_interface_result =
        run_emit_package_interface(app, generic_producer_path, generic_interface_path);
    assert_success_with_empty_stdout(generic_interface_result);

    auto generic_consumer_path = smoke_temp_root / "generic consumer.or";
    write_fixture(
        generic_consumer_path,
        "demo.genericconsumer",
        {
            "import",
            "    choose as select from external.generics",
            "function main() -> UInt32",
            "    select(40 as UInt32, 41 as UInt32) - 40 as UInt32",
        }
    );
    auto specialization_requests_path = smoke_temp_root / "generic_consumer.osr";
    auto specialization_requests_result = run_emit_specialization_requests(
        app,
        generic_interface_path,
        generic_consumer_path,
        specialization_requests_path
    );
    assert_success_with_empty_stdout(specialization_requests_result);
    {
        auto input = std::ifstream(specialization_requests_path);
        auto contents = std::string(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()
        );
        assert(
            contents ==
            "orison-specialization-requests 2\n"
            "request external.generics choose 1 UInt32 UInt32\n"
        );
    }

    auto specialization_requests_path_text = specialization_requests_path.string();
    auto generic_producer_path_text = generic_producer_path.string();
    auto generic_producer_object_path = smoke_temp_root / "generic_producer.o";
    auto generic_producer_object_path_text = generic_producer_object_path.string();
    std::array<char const*, 9> generic_producer_object_argv {
        "orisonc",
        "--specialization-requests",
        specialization_requests_path_text.c_str(),
        "--specialization-requests",
        specialization_requests_path_text.c_str(),
        "--emit-object",
        generic_producer_path_text.c_str(),
        "-o",
        generic_producer_object_path_text.c_str(),
    };
    auto generic_producer_object_result = app.run(
        std::span<char const* const>(
            generic_producer_object_argv.data(),
            generic_producer_object_argv.size()
        )
    );
    assert_success_with_empty_stdout(generic_producer_object_result);

    auto generic_interface_path_text = generic_interface_path.string();
    auto generic_consumer_path_text = generic_consumer_path.string();
    auto generic_consumer_object_path = smoke_temp_root / "generic_consumer.o";
    auto generic_consumer_object_path_text = generic_consumer_object_path.string();
    std::array<char const*, 7> generic_consumer_object_argv {
        "orisonc",
        "--package-interface",
        generic_interface_path_text.c_str(),
        "--emit-object",
        generic_consumer_path_text.c_str(),
        "-o",
        generic_consumer_object_path_text.c_str(),
    };
    auto generic_consumer_object_result = app.run(
        std::span<char const* const>(
            generic_consumer_object_argv.data(),
            generic_consumer_object_argv.size()
        )
    );
    assert_success_with_empty_stdout(generic_consumer_object_result);

    auto generic_producer_object_input = std::ifstream(
        generic_producer_object_path,
        std::ios::in | std::ios::binary
    );
    auto generic_producer_object = std::string(
        std::istreambuf_iterator<char>(generic_producer_object_input),
        std::istreambuf_iterator<char>()
    );
    auto generic_consumer_object_input = std::ifstream(
        generic_consumer_object_path,
        std::ios::in | std::ios::binary
    );
    auto generic_consumer_object = std::string(
        std::istreambuf_iterator<char>(generic_consumer_object_input),
        std::istreambuf_iterator<char>()
    );
    auto generic_object_images = std::array<std::string_view, 2> {
        generic_producer_object,
        generic_consumer_object,
    };
    auto generic_executable_path = smoke_temp_root / "generic_consumer";
    auto generic_link = orison::link::HostLinker {}.link(
        generic_object_images,
        generic_executable_path
    );
    assert(!generic_link.has_errors());
    assert(std::system(generic_executable_path.string().c_str()) == 0);

    auto unrelated_path = smoke_temp_root / "unrelated.or";
    write_fixture(
        unrelated_path,
        "demo.unrelated",
        {
            "function main() -> UInt32",
            "    99 as UInt32",
        }
    );
    auto build_plan_path = smoke_temp_root / "generic.obp";
    {
        auto output = std::ofstream(build_plan_path);
        output << "orison-build-plan 1\n";
        output << "entry demo.genericconsumer\n";
        output << "cache build cache\n";
        output << "module demo.unrelated unrelated.or\n";
        output << "module external.generics generic producer.or\n";
        output << "module demo.genericconsumer generic consumer.or\n";
    }
    auto build_plan_executable_path = smoke_temp_root / "generic_build_plan";
    auto build_plan_result = run_build_plan(
        app,
        build_plan_path,
        build_plan_executable_path
    );
    assert_success_with_empty_stdout(build_plan_result);
    assert(std::system(build_plan_executable_path.string().c_str()) == 0);

    auto build_cache_path = smoke_temp_root / "build cache";
    auto cached_write_times = std::vector<
        std::pair<std::filesystem::path, std::filesystem::file_time_type>
    > {};
    for (auto const& entry : std::filesystem::directory_iterator(build_cache_path)) {
        if (entry.is_regular_file()) {
            cached_write_times.emplace_back(entry.path(), entry.last_write_time());
        }
    }
    assert(cached_write_times.size() == 12);

    auto cached_build_plan_executable_path = smoke_temp_root / "generic_cached_build_plan";
    auto cached_build_plan_result = run_build_plan(
        app,
        build_plan_path,
        cached_build_plan_executable_path
    );
    assert_success_with_empty_stdout(cached_build_plan_result);
    assert(std::system(cached_build_plan_executable_path.string().c_str()) == 0);
    for (auto const& [cache_path, write_time] : cached_write_times) {
        assert(std::filesystem::last_write_time(cache_path) == write_time);
    }

    {
        auto output = std::ofstream(generic_producer_path, std::ios::app);
        output << "\n";
    }
    auto invalidated_build_plan_executable_path = smoke_temp_root / "generic_invalidated_build_plan";
    auto invalidated_build_plan_result = run_build_plan(
        app,
        build_plan_path,
        invalidated_build_plan_executable_path
    );
    assert_success_with_empty_stdout(invalidated_build_plan_result);
    assert(std::system(invalidated_build_plan_executable_path.string().c_str()) == 0);
    auto invalidated_cache_file_count = std::size_t {};
    for (auto const& entry : std::filesystem::directory_iterator(build_cache_path)) {
        if (entry.is_regular_file()) {
            ++invalidated_cache_file_count;
        }
    }
    assert(invalidated_cache_file_count == 20);

    auto prebuilt_support_path = smoke_temp_root / "prebuilt support.or";
    write_fixture(
        prebuilt_support_path,
        "external.support",
        {
            "public function identity(value: UInt32) -> UInt32",
            "    value",
        }
    );
    auto prebuilt_plan_path = smoke_temp_root / "generic_prebuilt.obp";
    {
        auto output = std::ofstream(prebuilt_plan_path);
        output << "orison-build-plan 1\n";
        output << "entry demo.genericconsumer\n";
        output << "interface external.generics generic_producer.opi\n";
        output << "object external.generics generic_producer.o\n";
        output << "depends external.generics external.support\n";
        output << "library external.generics m\n";
        output << "module external.support prebuilt support.or\n";
        output << "module demo.genericconsumer generic consumer.or\n";
    }
    auto prebuilt_plan_executable_path = smoke_temp_root / "generic_prebuilt_build_plan";
    auto prebuilt_plan_result = run_build_plan(
        app,
        prebuilt_plan_path,
        prebuilt_plan_executable_path
    );
    assert_success_with_empty_stdout(prebuilt_plan_result);
    assert(std::system(prebuilt_plan_executable_path.string().c_str()) == 0);

    auto invalid_build_plan_path = smoke_temp_root / "invalid.obp";
    {
        auto output = std::ofstream(invalid_build_plan_path);
        output << "orison-build-plan 2\n";
    }
    auto invalid_build_plan_result = run_build_plan(
        app,
        invalid_build_plan_path,
        smoke_temp_root / "invalid_build_plan"
    );
    assert(invalid_build_plan_result.exit_code == 1);
    assert(invalid_build_plan_result.stdout_text.empty());
    assert(
        invalid_build_plan_result.stderr_text.find(
            "unsupported build plan version '2'"
        ) != std::string::npos
    );

    auto incomplete_build_plan_path = smoke_temp_root / "incomplete.obp";
    {
        auto output = std::ofstream(incomplete_build_plan_path);
        output << "orison-build-plan 1\n";
        output << "entry demo.genericconsumer\n";
        output << "module demo.genericconsumer generic consumer.or\n";
    }
    auto incomplete_build_plan_result = run_build_plan(
        app,
        incomplete_build_plan_path,
        smoke_temp_root / "incomplete_build_plan"
    );
    assert(incomplete_build_plan_result.exit_code == 1);
    assert(incomplete_build_plan_result.stdout_text.empty());
    assert(
        incomplete_build_plan_result.stderr_text.find(
            "build plan has no package mapping for imported package 'external.generics' required by "
            "'demo.genericconsumer'"
        ) != std::string::npos
    );

    auto incomplete_prebuilt_plan_path = smoke_temp_root / "incomplete_prebuilt.obp";
    {
        auto output = std::ofstream(incomplete_prebuilt_plan_path);
        output << "orison-build-plan 1\n";
        output << "entry external.generics\n";
        output << "interface external.generics generic_producer.opi\n";
    }
    auto incomplete_prebuilt_plan_result = run_build_plan(
        app,
        incomplete_prebuilt_plan_path,
        smoke_temp_root / "incomplete_prebuilt_build_plan"
    );
    assert(incomplete_prebuilt_plan_result.exit_code == 1);
    assert(incomplete_prebuilt_plan_result.stdout_text.empty());
    assert(
        incomplete_prebuilt_plan_result.stderr_text.find(
            "build plan package 'external.generics' requires either a source module or a prebuilt "
            "interface/object pair"
        ) != std::string::npos
    );

    auto invalid_specialization_requests_path = smoke_temp_root / "invalid.osr";
    {
        auto output = std::ofstream(invalid_specialization_requests_path);
        output << "orison-specialization-requests 3\n";
    }
    auto invalid_specialization_requests_path_text = invalid_specialization_requests_path.string();
    std::array<char const*, 5> invalid_specialization_requests_argv {
        "orisonc",
        "--specialization-requests",
        invalid_specialization_requests_path_text.c_str(),
        "--emit-llvm",
        generic_producer_path_text.c_str(),
    };
    auto invalid_specialization_requests_result = app.run(
        std::span<char const* const>(
            invalid_specialization_requests_argv.data(),
            invalid_specialization_requests_argv.size()
        )
    );
    assert(invalid_specialization_requests_result.exit_code == 1);
    assert(invalid_specialization_requests_result.stdout_text.empty());
    assert(
        invalid_specialization_requests_result.stderr_text.find(
            "unsupported specialization request version '3'"
        ) != std::string::npos
    );

    auto invalid_package_source_path = smoke_temp_root / "invalid_external_types.or";
    write_fixture(
        invalid_package_source_path,
        "external.invalid",
        {
            "public record Invalid",
            "    public value: MissingType",
        }
    );
    auto invalid_package_interface_path = smoke_temp_root / "invalid_external_types.opi";
    auto invalid_package_interface_result = run_emit_package_interface(
        app,
        invalid_package_source_path,
        invalid_package_interface_path
    );
    assert(invalid_package_interface_result.exit_code == 1);
    assert(invalid_package_interface_result.stdout_text.empty());
    assert(invalid_package_interface_result.stderr_text.find("unknown type 'MissingType'") != std::string::npos);
    assert(!std::filesystem::exists(invalid_package_interface_path));

    auto missing_directory = smoke_temp_root / "orison_missing_object_directory";
    std::filesystem::remove_all(missing_directory);
    auto object_write_failure = run_emit_object(app, scalar_path, missing_directory / "output.o");
    assert(object_write_failure.exit_code == 1);
    assert(object_write_failure.stdout_text.empty());
    assert(object_write_failure.stderr_text == "error: unable to write object file\n");

    auto package_interface_write_path = missing_directory / "output.opi";
    auto package_interface_write_failure =
        run_emit_package_interface(app, package_source_path, package_interface_write_path);
    assert(package_interface_write_failure.exit_code == 1);
    assert(package_interface_write_failure.stdout_text.empty());
    assert(
        package_interface_write_failure.stderr_text ==
        "error: unable to write package interface '" + package_interface_write_path.string() + "'\n"
    );

    auto specialization_requests_write_path = missing_directory / "output.osr";
    auto specialization_requests_write_failure = run_emit_specialization_requests(
        app,
        generic_interface_path,
        generic_consumer_path,
        specialization_requests_write_path
    );
    assert(specialization_requests_write_failure.exit_code == 1);
    assert(specialization_requests_write_failure.stdout_text.empty());
    assert(
        specialization_requests_write_failure.stderr_text ==
        "error: unable to write specialization requests '" +
            specialization_requests_write_path.string() + "'\n"
    );

    auto view_descriptor_path = smoke_temp_root / "view_descriptor_reads.or";
    write_fixture(
        view_descriptor_path,
        "demo.viewdescriptor",
        {
            "function count(values: shared.View<UInt32>) -> IntSize",
            "    values.length()",
            "function first(values: exclusive.View<UInt32>) -> UInt32",
            "    values[0]",
            "function sum(values: shared.View<UInt32>) -> UInt32",
            "    var total = 0 as UInt32",
            "    for item in values",
            "        total = total + item",
            "    total",
            "function main() -> UInt32",
            "    0 as UInt32",
        }
    );
    auto view_descriptor_object_path = smoke_temp_root / "view_descriptor_reads.o";
    auto view_descriptor_object_result =
        run_emit_object(app, view_descriptor_path, view_descriptor_object_path);
    assert_success_with_empty_stdout(view_descriptor_object_result);
    assert(std::filesystem::file_size(view_descriptor_object_path) > 0);

    auto view_descriptor_executable_path = smoke_temp_root / "view_descriptor_reads";
    auto view_descriptor_build_result =
        run_build(app, view_descriptor_path, view_descriptor_executable_path);
    assert_success_with_empty_stdout(view_descriptor_build_result);
    auto view_descriptor_status = std::system(view_descriptor_executable_path.string().c_str());
    assert(WIFEXITED(view_descriptor_status));
    assert(WEXITSTATUS(view_descriptor_status) == 0);

    auto executable_path = smoke_temp_root / "artifact_scalar";
    auto build_result = run_build(app, scalar_path, executable_path);
    assert_success_with_empty_stdout(build_result);
    auto executable_status = std::system(executable_path.string().c_str());
    assert(WIFEXITED(executable_status));
    assert(WEXITSTATUS(executable_status) == 42);

    auto replacement_path = smoke_temp_root / "artifact_replacement.or";
    write_fixture(
        replacement_path,
        "demo.replacement",
        {
            "function main() -> UInt32",
            "    7 as UInt32",
        }
    );
    auto replacement_result = run_build(app, replacement_path, executable_path);
    assert_success_with_empty_stdout(replacement_result);
    auto replacement_status = std::system(executable_path.string().c_str());
    assert(WIFEXITED(replacement_status));
    assert(WEXITSTATUS(replacement_status) == 7);

    auto run_result = run_program(app, scalar_path);
    assert_result_with_empty_output(run_result, 42);

    auto run_concurrency_path = smoke_temp_root / "artifact_concurrency.or";
    write_fixture(
        run_concurrency_path,
        "demo.run",
        {
            "async function main() -> UInt32",
            "    let pending = task",
            "        42 as UInt32",
            "",
            "    await pending",
        }
    );
    auto run_concurrency = run_program(app, run_concurrency_path);
    assert_result_with_empty_output(run_concurrency, 42);

    auto link_failure = run_build(app, scalar_path, missing_directory / "output");
    assert(link_failure.exit_code == 1);
    assert(link_failure.stdout_text.empty());
    assert(link_failure.stderr_text.find("output directory does not exist") != std::string::npos);

    auto file_parent_path = smoke_temp_root / "orison_file_parent";
    {
        auto file_parent = std::ofstream(file_parent_path);
        file_parent << "not a directory\n";
    }
    auto file_parent_failure = run_build(app, scalar_path, file_parent_path / "output");
    assert(file_parent_failure.exit_code == 1);
    assert(file_parent_failure.stdout_text.empty());
    assert(file_parent_failure.stderr_text.find("output path parent is not a directory") != std::string::npos);

    auto directory_output_path = smoke_temp_root / "orison_directory_output";
    std::filesystem::create_directory(directory_output_path);
    auto directory_output_failure = run_build(app, scalar_path, directory_output_path);
    assert(directory_output_failure.exit_code == 1);
    assert(directory_output_failure.stdout_text.empty());
    assert(directory_output_failure.stderr_text.find("output path is a directory") != std::string::npos);

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}

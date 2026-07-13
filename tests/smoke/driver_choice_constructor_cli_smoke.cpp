#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

auto read_failing_command_output(std::string const& command) -> std::string {
    std::array<char, 256> buffer {};
    std::string output;

    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    assert(pipe != nullptr);

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    auto status = pclose(pipe);
    assert(status != 0);
    return output;
}

auto read_successful_command_output(std::string const& command) -> std::string {
    std::array<char, 256> buffer {};
    std::string output;

    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    assert(pipe != nullptr);

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    auto status = pclose(pipe);
    assert(status == 0);
    return output;
}

template <typename SourceLines>
void assert_cli_parse_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines,
    std::string_view expected_message
) {
    {
        std::ofstream output(path);
        for (auto line : lines) {
            output << line << '\n';
        }
    }

    auto command = executable.string() + " --parse " + path.string();
    auto output = read_failing_command_output(command);
    assert(output.find(expected_message) != std::string::npos);
}

template <typename SourceLines>
void assert_cli_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines,
    std::initializer_list<std::string_view> expected_fragments
) {
    {
        std::ofstream output(path);
        for (auto line : lines) {
            output << line << '\n';
        }
    }

    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_successful_command_output(command);
    for (auto expected_fragment : expected_fragments) {
        assert(output.find(expected_fragment) != std::string::npos);
    }
}

template <typename SourceLines>
void assert_cli_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines,
    std::string_view expected_message
) {
    {
        std::ofstream output(path);
        for (auto line : lines) {
            output << line << '\n';
        }
    }

    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_failing_command_output(command);
    assert(output.find(expected_message) != std::string::npos);
}

template <typename SourceLines>
void assert_cli_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines
) {
    {
        std::ofstream output(path);
        for (auto line : lines) {
            output << line << '\n';
        }
    }

    auto command = executable.string() + " run " + path.string();
    (void)read_successful_command_output(command);
}

auto status_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        "choice Status",
        "    Ready(code: UInt32)",
        "    Empty",
        initializer,
    };
}

auto maybe_choice_constant_lines_with_declarations(
    std::string_view initializer,
    std::initializer_list<std::string_view> declarations
) -> std::vector<std::string_view> {
    std::vector<std::string_view> lines {
        "package demo.cli",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
    };
    lines.insert(lines.end(), declarations.begin(), declarations.end());
    lines.push_back(initializer);
    return lines;
}

auto maybe_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(initializer, {});
}

auto boxed_maybe_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(
        initializer,
        {"choice Boxed<T>", "    Wrap(inner: T)"}
    );
}

auto maybe_result_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(
        initializer,
        {"choice Result<T>", "    Ok(value: T)", "    Error(message: Text)"}
    );
}

auto boxed_maybe_result_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(
        initializer,
        {
            "choice Boxed<T>",
            "    Wrap(inner: T)",
            "choice Result<T>",
            "    Ok(value: T)",
            "    Error(message: Text)",
        }
    );
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_choice_constructor_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";

    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_choice_constructor_emit.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Status",
            "    Ready(code: UInt32)",
            "    Empty",
            "function make_status() -> Status",
            "    Ready(7 as UInt32)",
            "function main() -> UInt32",
            "    return 0 as UInt32",
        },
        {
            "define { i32, i32 } @make_status()",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "insertvalue { i32, i32 }",
            "i32 7, 1",
            "ret { i32, i32 }",
        }
    );
    auto scalar_choice_switch_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice Status",
        "    Ready(code: UInt32)",
        "    Empty",
        "function make_status() -> Status",
        "    Ready(7 as UInt32)",
        "function classify_status() -> UInt32",
        "    switch make_status()",
        "        Ready(code) => code",
        "        Empty => 0 as UInt32",
        "function main() -> UInt32",
        "    return classify_status() - 7 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_choice_switch_emit.or",
        scalar_choice_switch_lines,
        {
            "define i32 @classify_status()",
            "extractvalue { i32, i32 }",
            "switch i32",
            "i32 0, label %switch.case",
            "i32 1, label %switch.case",
            "ret i32 %",
            "ret i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_choice_switch_run.or",
        scalar_choice_switch_lines
    );
    auto tag_only_choice_switch_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice IOError",
        "    Closed",
        "    EndOfInput",
        "    PermissionDenied",
        "function make_error() -> IOError",
        "    EndOfInput",
        "function classify_error() -> UInt32",
        "    switch make_error()",
        "        Closed => 1 as UInt32",
        "        EndOfInput => 2 as UInt32",
        "        PermissionDenied => 3 as UInt32",
        "function main() -> UInt32",
        "    return classify_error() - 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_tag_only_choice_switch_emit.or",
        tag_only_choice_switch_lines,
        {
            "define i32 @make_error()",
            "ret i32 1",
            "define i32 @classify_error()",
            "switch i32",
            "i32 0, label %switch.case",
            "i32 1, label %switch.case",
            "i32 2, label %switch.case",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_tag_only_choice_switch_run.or",
        tag_only_choice_switch_lines
    );
    auto sourced_tag_only_choice_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready",
        "    Empty",
        "choice RemoteStatus",
        "    Ready",
        "    Empty",
        "function make_status() -> LocalStatus",
        "    Ready",
        "function classify_status() -> UInt32",
        "    switch make_status()",
        "        Ready => 1 as UInt32",
        "        Empty => 0 as UInt32",
        "function main() -> UInt32",
        "    return classify_status() - 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_tag_only_choice_emit.or",
        sourced_tag_only_choice_lines,
        {
            "define i32 @make_status()",
            "ret i32 0",
            "define i32 @classify_status()",
            "switch i32",
            "i32 0, label %switch.case",
            "i32 1, label %switch.case",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_tag_only_choice_run.or",
        sourced_tag_only_choice_lines
    );
    auto sourced_tag_only_guard_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready",
        "    Empty",
        "choice RemoteStatus",
        "    Ready",
        "    Empty",
        "function choose(flag: Bool) -> LocalStatus",
        "    guard flag else",
        "        return Empty",
        "    return Ready",
        "function classify_status(flag: Bool) -> UInt32",
        "    switch choose(flag)",
        "        Ready => 1 as UInt32",
        "        Empty => 0 as UInt32",
        "function main() -> UInt32",
        "    return classify_status(false)",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_tag_only_guard_return_emit.or",
        sourced_tag_only_guard_return_lines,
        {
            "define i32 @choose",
            "guard.failure.",
            "ret i32 1",
            "ret i32 0",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_tag_only_guard_return_run.or",
        sourced_tag_only_guard_return_lines
    );
    auto sourced_tag_only_branch_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready",
        "    Empty",
        "choice RemoteStatus",
        "    Ready",
        "    Empty",
        "function choose(flag: Bool) -> LocalStatus",
        "    if flag",
        "        return Ready",
        "    return Empty",
        "function classify_status(flag: Bool) -> UInt32",
        "    switch choose(flag)",
        "        Ready => 1 as UInt32",
        "        Empty => 0 as UInt32",
        "function main() -> UInt32",
        "    return classify_status(true) - 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_tag_only_branch_return_emit.or",
        sourced_tag_only_branch_return_lines,
        {
            "define i32 @choose",
            "if.then.",
            "ret i32 0",
            "ret i32 1",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_tag_only_branch_return_run.or",
        sourced_tag_only_branch_return_lines
    );
    auto sourced_scalar_choice_guard_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function choose(flag: Bool) -> LocalStatus",
        "    guard flag else",
        "        return Empty",
        "    return Ready(7 as UInt32)",
        "function classify_status(flag: Bool) -> UInt32",
        "    switch choose(flag)",
        "        Ready(code) => code",
        "        Empty => 0 as UInt32",
        "function main() -> UInt32",
        "    return classify_status(false)",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_guard_return_emit.or",
        sourced_scalar_choice_guard_return_lines,
        {
            "define { i32, i32 } @choose",
            "guard.failure.",
            "insertvalue { i32, i32 } undef, i32 1, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 7, 1",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_guard_return_run.or",
        sourced_scalar_choice_guard_return_lines
    );
    auto sourced_scalar_choice_branch_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function choose(flag: Bool) -> LocalStatus",
        "    if flag",
        "        return Ready(7 as UInt32)",
        "    return Empty",
        "function classify_status(flag: Bool) -> UInt32",
        "    switch choose(flag)",
        "        Ready(code) => code",
        "        Empty => 0 as UInt32",
        "function main() -> UInt32",
        "    return classify_status(true) - 7 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_branch_return_emit.or",
        sourced_scalar_choice_branch_return_lines,
        {
            "define { i32, i32 } @choose",
            "if.then.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 7, 1",
            "insertvalue { i32, i32 } undef, i32 1, 0",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_branch_return_run.or",
        sourced_scalar_choice_branch_return_lines
    );
    auto sourced_scalar_choice_let_initializer_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function main() -> UInt32",
        "    let status: LocalStatus = Ready(9 as UInt32)",
        "    switch status",
        "        Ready(code) => code - 9 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_let_initializer_emit.or",
        sourced_scalar_choice_let_initializer_lines,
        {
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 9, 1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_let_initializer_run.or",
        sourced_scalar_choice_let_initializer_lines
    );
    auto sourced_scalar_choice_var_initializer_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function main() -> UInt32",
        "    var status: LocalStatus = Empty",
        "    switch status",
        "        Ready(code) => code",
        "        Empty => 0 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_var_initializer_emit.or",
        sourced_scalar_choice_var_initializer_lines,
        {
            "alloca { i32, i32 }",
            "insertvalue { i32, i32 } undef, i32 1, 0",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_var_initializer_run.or",
        sourced_scalar_choice_var_initializer_lines
    );
    auto sourced_scalar_choice_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function main() -> UInt32",
        "    var status: LocalStatus = Empty",
        "    status = Ready(11 as UInt32)",
        "    switch status",
        "        Ready(code) => code - 11 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_assignment_emit.or",
        sourced_scalar_choice_assignment_lines,
        {
            "alloca { i32, i32 }",
            "insertvalue { i32, i32 } undef, i32 1, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 11, 1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_assignment_run.or",
        sourced_scalar_choice_assignment_lines
    );
    auto sourced_scalar_choice_return_ternary_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function choose(flag: Bool) -> LocalStatus",
        "    flag ? Ready(13 as UInt32) : Empty",
        "function main() -> UInt32",
        "    switch choose(true)",
        "        Ready(code) => code - 13 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_return_ternary_emit.or",
        sourced_scalar_choice_return_ternary_lines,
        {
            "define { i32, i32 } @choose",
            "ternary.then.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 13, 1",
            "insertvalue { i32, i32 } undef, i32 1, 0",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_return_ternary_run.or",
        sourced_scalar_choice_return_ternary_lines
    );
    auto sourced_scalar_choice_let_ternary_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function main() -> UInt32",
        "    let status: LocalStatus = false ? Ready(17 as UInt32) : Empty",
        "    switch status",
        "        Ready(code) => code",
        "        Empty => 0 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_let_ternary_emit.or",
        sourced_scalar_choice_let_ternary_lines,
        {
            "ternary.then.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 17, 1",
            "insertvalue { i32, i32 } undef, i32 1, 0",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_let_ternary_run.or",
        sourced_scalar_choice_let_ternary_lines
    );
    auto sourced_scalar_choice_some_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function choose() -> Maybe<LocalStatus>",
        "    Some(Ready(19 as UInt32))",
        "function main() -> UInt32",
        "    switch choose()",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 19 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_some_return_emit.or",
        sourced_scalar_choice_some_return_lines,
        {
            "define { i1, { i32, i32 } } @choose",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 19, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_some_return_run.or",
        sourced_scalar_choice_some_return_lines
    );
    auto sourced_scalar_choice_some_let_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function main() -> UInt32",
        "    let maybe_status: Maybe<LocalStatus> = Some(Ready(23 as UInt32))",
        "    switch maybe_status",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 23 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_some_let_emit.or",
        sourced_scalar_choice_some_let_lines,
        {
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 23, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_some_let_run.or",
        sourced_scalar_choice_some_let_lines
    );
    auto sourced_scalar_choice_array_let_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function main() -> UInt32",
        "    let statuses: Array<LocalStatus, 2> = [Ready(29 as UInt32), Empty]",
        "    switch statuses[0 as UInt64]",
        "        Ready(code) => code - 29 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_array_let_emit.or",
        sourced_scalar_choice_array_let_lines,
        {
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 29, 1",
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_array_let_run.or",
        sourced_scalar_choice_array_let_lines
    );
    auto sourced_scalar_choice_maybe_array_let_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function main() -> UInt32",
        "    let statuses: Array<Maybe<LocalStatus>, 2> = [Some(Ready(31 as UInt32)), Empty]",
        "    switch statuses[0 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 31 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_array_let_emit.or",
        sourced_scalar_choice_maybe_array_let_lines,
        {
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 31, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_array_let_run.or",
        sourced_scalar_choice_maybe_array_let_lines
    );
    auto sourced_scalar_choice_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function observe(status: LocalStatus) -> UInt32",
        "    switch status",
        "        Ready(code) => code",
        "        Empty => 0 as UInt32",
        "function main() -> UInt32",
        "    observe(Ready(37 as UInt32)) - 37 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_call_argument_emit.or",
        sourced_scalar_choice_call_argument_lines,
        {
            "define i32 @observe({ i32, i32 } %status)",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 37, 1",
            "call i32 @observe({ i32, i32 }",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_call_argument_run.or",
        sourced_scalar_choice_call_argument_lines
    );
    auto sourced_scalar_choice_maybe_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function observe(maybe_status: Maybe<LocalStatus>) -> UInt32",
        "    switch maybe_status",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
        "function main() -> UInt32",
        "    observe(Some(Ready(41 as UInt32))) - 41 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_call_argument_emit.or",
        sourced_scalar_choice_maybe_call_argument_lines,
        {
            "define i32 @observe({ i1, { i32, i32 } } %maybe_status)",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 41, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "call i32 @observe({ i1, { i32, i32 } }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_call_argument_run.or",
        sourced_scalar_choice_maybe_call_argument_lines
    );
    auto sourced_scalar_choice_member_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "extend UInt32",
        "    function observe(this: shared This, status: LocalStatus) -> UInt32",
        "        switch status",
        "            Ready(code) => code",
        "            Empty => this",
        "function main() -> UInt32",
        "    let value: UInt32 = 0 as UInt32",
        "    value.observe(Ready(43 as UInt32)) - 43 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_member_call_argument_emit.or",
        sourced_scalar_choice_member_call_argument_lines,
        {
            "define i32 @method.UInt32.observe(i32 %this, { i32, i32 } %status)",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 43, 1",
            "call i32 @method.UInt32.observe(i32 %value, { i32, i32 }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_member_call_argument_run.or",
        sourced_scalar_choice_member_call_argument_lines
    );
    auto sourced_scalar_choice_maybe_member_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "extend UInt32",
        "    function observe(this: shared This, maybe_status: Maybe<LocalStatus>) -> UInt32",
        "        switch maybe_status",
        "            Some(status) =>",
        "                switch status",
        "                    Ready(code) => code",
        "                    Empty => this",
        "            Empty => 2 as UInt32",
        "function main() -> UInt32",
        "    let value: UInt32 = 0 as UInt32",
        "    value.observe(Some(Ready(47 as UInt32))) - 47 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_member_call_argument_emit.or",
        sourced_scalar_choice_maybe_member_call_argument_lines,
        {
            "define i32 @method.UInt32.observe(i32 %this, { i1, { i32, i32 } } %maybe_status)",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 47, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "call i32 @method.UInt32.observe(i32 %value, { i1, { i32, i32 } }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_member_call_argument_run.or",
        sourced_scalar_choice_maybe_member_call_argument_lines
    );
    auto sourced_scalar_choice_array_member_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "extend UInt32",
        "    function observe(this: shared This, statuses: Array<LocalStatus, 2>) -> UInt32",
        "        switch statuses[0 as UInt64]",
        "            Ready(code) => code - 131 as UInt32",
        "            Empty => this",
        "function main() -> UInt32",
        "    let value: UInt32 = 0 as UInt32",
        "    value.observe([Ready(131 as UInt32), Empty])",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_array_member_call_argument_emit.or",
        sourced_scalar_choice_array_member_call_argument_lines,
        {
            "define i32 @method.UInt32.observe(i32 %this, [2 x { i32, i32 }] %statuses)",
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "i32 131, 1",
            "call i32 @method.UInt32.observe(i32 %value, [2 x { i32, i32 }]",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_array_member_call_argument_run.or",
        sourced_scalar_choice_array_member_call_argument_lines
    );
    auto sourced_scalar_choice_maybe_array_member_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "extend UInt32",
        "    function observe(this: shared This, statuses: Array<Maybe<LocalStatus>, 2>) -> UInt32",
        "        switch statuses[0 as UInt64]",
        "            Some(status) =>",
        "                switch status",
        "                    Ready(code) => code - 133 as UInt32",
        "                    Empty => this",
        "            Empty => 2 as UInt32",
        "function main() -> UInt32",
        "    let value: UInt32 = 0 as UInt32",
        "    value.observe([Some(Ready(133 as UInt32)), Empty])",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_array_member_call_argument_emit.or",
        sourced_scalar_choice_maybe_array_member_call_argument_lines,
        {
            "define i32 @method.UInt32.observe(i32 %this, [2 x { i1, { i32, i32 } }] %statuses)",
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "i32 133, 1",
            "call i32 @method.UInt32.observe(i32 %value, [2 x { i1, { i32, i32 } }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_array_member_call_argument_run.or",
        sourced_scalar_choice_maybe_array_member_call_argument_lines
    );
    auto sourced_scalar_choice_nested_array_member_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "extend UInt32",
        "    function observe(this: shared This, statuses: Array<Array<LocalStatus, 2>, 2>) -> UInt32",
        "        switch statuses[0 as UInt64][1 as UInt64]",
        "            Ready(code) => code - 139 as UInt32",
        "            Empty => this",
        "function main() -> UInt32",
        "    let value: UInt32 = 0 as UInt32",
        "    value.observe([[Empty, Ready(139 as UInt32)], [Empty, Empty]])",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_nested_array_member_call_argument_emit.or",
        sourced_scalar_choice_nested_array_member_call_argument_lines,
        {
            "define i32 @method.UInt32.observe(i32 %this, [2 x [2 x { i32, i32 }]] %statuses)",
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "i32 139, 1",
            "call i32 @method.UInt32.observe(i32 %value, [2 x [2 x { i32, i32 }]]",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_nested_array_member_call_argument_run.or",
        sourced_scalar_choice_nested_array_member_call_argument_lines
    );
    auto sourced_scalar_choice_maybe_nested_array_member_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "extend UInt32",
        "    function observe(this: shared This, statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>) -> UInt32",
        "        switch statuses[0 as UInt64][1 as UInt64]",
        "            Some(status) =>",
        "                switch status",
        "                    Ready(code) => code - 141 as UInt32",
        "                    Empty => this",
        "            Empty => 2 as UInt32",
        "function main() -> UInt32",
        "    let value: UInt32 = 0 as UInt32",
        "    value.observe([[Empty, Some(Ready(141 as UInt32))], [Empty, Empty]])",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_nested_array_member_call_argument_emit.or",
        sourced_scalar_choice_maybe_nested_array_member_call_argument_lines,
        {
            "define i32 @method.UInt32.observe(i32 %this, [2 x [2 x { i1, { i32, i32 } }]] %statuses)",
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "i32 141, 1",
            "call i32 @method.UInt32.observe(i32 %value, [2 x [2 x { i1, { i32, i32 } }]]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_nested_array_member_call_argument_run.or",
        sourced_scalar_choice_maybe_nested_array_member_call_argument_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_member_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "extend UInt32",
        "    function observe(this: shared This, maybe_grid: Maybe<StatusGrid>) -> UInt32",
        "        switch maybe_grid",
        "            Some(grid) =>",
        "                switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                    Ready(code) => code - 179 as UInt32",
        "                    Empty => this",
        "            Empty => 2 as UInt32",
        "function main() -> UInt32",
        "    let value: UInt32 = 0 as UInt32",
        "    value.observe(Some(StatusGrid([[Empty, Ready(179 as UInt32)], [Empty, Empty]])))",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_member_call_argument_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_member_call_argument_lines,
        {
            "define i32 @method.UInt32.observe(i32 %this, { i1, %record.StatusGrid } %maybe_grid)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 179, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "call i32 @method.UInt32.observe(i32 %value, { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_member_call_argument_run.or",
        sourced_scalar_choice_maybe_record_nested_array_member_call_argument_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_member_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "extend UInt32",
        "    function observe(this: shared This, maybe_grid: Maybe<StatusGrid>) -> UInt32",
        "        switch maybe_grid",
        "            Some(grid) =>",
        "                switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                    Some(status) =>",
        "                        switch status",
        "                            Ready(code) => code - 181 as UInt32",
        "                            Empty => this",
        "                    Empty => 2 as UInt32",
        "            Empty => 3 as UInt32",
        "function main() -> UInt32",
        "    let value: UInt32 = 0 as UInt32",
        "    value.observe(Some(StatusGrid([[Empty, Some(Ready(181 as UInt32))], [Empty, Empty]])))",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_member_call_argument_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_member_call_argument_lines,
        {
            "define i32 @method.UInt32.observe(i32 %this, { i1, %record.StatusGrid } %maybe_grid)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 181, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "call i32 @method.UInt32.observe(i32 %value, { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_member_call_argument_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_member_call_argument_lines
    );
    auto sourced_scalar_choice_record_field_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusBox",
        "    status: LocalStatus",
        "function main() -> UInt32",
        "    let box: StatusBox = StatusBox(Ready(53 as UInt32))",
        "    switch box.status",
        "        Ready(code) => code - 53 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_field_emit.or",
        sourced_scalar_choice_record_field_lines,
        {
            "%record.StatusBox = type { { i32, i32 } }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 53, 1",
            "insertvalue %record.StatusBox undef, { i32, i32 }",
            "getelementptr %record.StatusBox",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_field_run.or",
        sourced_scalar_choice_record_field_lines
    );
    auto sourced_scalar_choice_maybe_record_field_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusBox",
        "    maybe_status: Maybe<LocalStatus>",
        "function main() -> UInt32",
        "    let box: StatusBox = StatusBox(Some(Ready(59 as UInt32)))",
        "    switch box.maybe_status",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 59 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_field_emit.or",
        sourced_scalar_choice_maybe_record_field_lines,
        {
            "%record.StatusBox = type { { i1, { i32, i32 } } }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 59, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue %record.StatusBox undef, { i1, { i32, i32 } }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_field_run.or",
        sourced_scalar_choice_maybe_record_field_lines
    );
    auto sourced_scalar_choice_nested_record_field_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusBox",
        "    status: LocalStatus",
        "record OuterBox",
        "    inner: StatusBox",
        "function main() -> UInt32",
        "    let outer: OuterBox = OuterBox(StatusBox(Ready(61 as UInt32)))",
        "    switch outer.inner.status",
        "        Ready(code) => code - 61 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_nested_record_field_emit.or",
        sourced_scalar_choice_nested_record_field_lines,
        {
            "%record.StatusBox = type { { i32, i32 } }",
            "%record.OuterBox = type { %record.StatusBox }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 61, 1",
            "insertvalue %record.StatusBox undef, { i32, i32 }",
            "insertvalue %record.OuterBox undef, %record.StatusBox",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_nested_record_field_run.or",
        sourced_scalar_choice_nested_record_field_lines
    );
    auto sourced_scalar_choice_maybe_nested_record_field_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusBox",
        "    maybe_status: Maybe<LocalStatus>",
        "record OuterBox",
        "    inner: StatusBox",
        "function main() -> UInt32",
        "    let outer: OuterBox = OuterBox(StatusBox(Some(Ready(67 as UInt32))))",
        "    switch outer.inner.maybe_status",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 67 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_nested_record_field_emit.or",
        sourced_scalar_choice_maybe_nested_record_field_lines,
        {
            "%record.StatusBox = type { { i1, { i32, i32 } } }",
            "%record.OuterBox = type { %record.StatusBox }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 67, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue %record.StatusBox undef, { i1, { i32, i32 } }",
            "insertvalue %record.OuterBox undef, %record.StatusBox",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_nested_record_field_run.or",
        sourced_scalar_choice_maybe_nested_record_field_lines
    );
    auto sourced_scalar_choice_record_array_field_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusList",
        "    statuses: Array<LocalStatus, 2>",
        "function main() -> UInt32",
        "    let list: StatusList = StatusList([Ready(71 as UInt32), Empty])",
        "    switch list.statuses[0 as UInt64]",
        "        Ready(code) => code - 71 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_array_field_emit.or",
        sourced_scalar_choice_record_array_field_lines,
        {
            "%record.StatusList = type { [2 x { i32, i32 }] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 71, 1",
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "insertvalue %record.StatusList undef, [2 x { i32, i32 }]",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_array_field_run.or",
        sourced_scalar_choice_record_array_field_lines
    );
    auto sourced_scalar_choice_maybe_record_array_field_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusList",
        "    statuses: Array<Maybe<LocalStatus>, 2>",
        "function main() -> UInt32",
        "    let list: StatusList = StatusList([Some(Ready(73 as UInt32)), Empty])",
        "    switch list.statuses[0 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 73 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_array_field_emit.or",
        sourced_scalar_choice_maybe_record_array_field_lines,
        {
            "%record.StatusList = type { [2 x { i1, { i32, i32 } }] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 73, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "insertvalue %record.StatusList undef, [2 x { i1, { i32, i32 } }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_array_field_run.or",
        sourced_scalar_choice_maybe_record_array_field_lines
    );
    auto sourced_scalar_choice_record_nested_array_field_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function main() -> UInt32",
        "    let grid: StatusGrid = StatusGrid([[Empty, Ready(143 as UInt32)], [Empty, Empty]])",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Ready(code) => code - 143 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_field_emit.or",
        sourced_scalar_choice_record_nested_array_field_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 143, 1",
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_field_run.or",
        sourced_scalar_choice_record_nested_array_field_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_field_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function main() -> UInt32",
        "    let grid: StatusGrid = StatusGrid([[Empty, Some(Ready(145 as UInt32))], [Empty, Empty]])",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 145 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_field_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_field_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 145, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_field_run.or",
        sourced_scalar_choice_maybe_record_nested_array_field_lines
    );
    auto sourced_scalar_choice_record_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusBox",
        "    status: LocalStatus",
        "function make_box() -> StatusBox",
        "    StatusBox(Ready(79 as UInt32))",
        "function main() -> UInt32",
        "    let box: StatusBox = make_box()",
        "    switch box.status",
        "        Ready(code) => code - 79 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_return_emit.or",
        sourced_scalar_choice_record_return_lines,
        {
            "define %record.StatusBox @make_box()",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 79, 1",
            "insertvalue %record.StatusBox undef, { i32, i32 }",
            "ret %record.StatusBox",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_return_run.or",
        sourced_scalar_choice_record_return_lines
    );
    auto sourced_scalar_choice_maybe_record_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusBox",
        "    maybe_status: Maybe<LocalStatus>",
        "function make_box() -> StatusBox",
        "    StatusBox(Some(Ready(83 as UInt32)))",
        "function main() -> UInt32",
        "    let box: StatusBox = make_box()",
        "    switch box.maybe_status",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 83 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_return_emit.or",
        sourced_scalar_choice_maybe_record_return_lines,
        {
            "define %record.StatusBox @make_box()",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 83, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue %record.StatusBox undef, { i1, { i32, i32 } }",
            "ret %record.StatusBox",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_return_run.or",
        sourced_scalar_choice_maybe_record_return_lines
    );
    auto sourced_scalar_choice_record_nested_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grid() -> StatusGrid",
        "    StatusGrid([[Empty, Ready(151 as UInt32)], [Empty, Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid()",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Ready(code) => code - 151 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_return_emit.or",
        sourced_scalar_choice_record_nested_array_return_lines,
        {
            "define %record.StatusGrid @make_grid()",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 151, 1",
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "ret %record.StatusGrid",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_return_run.or",
        sourced_scalar_choice_record_nested_array_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grid() -> StatusGrid",
        "    StatusGrid([[Empty, Some(Ready(153 as UInt32))], [Empty, Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid()",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 153 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_return_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_return_lines,
        {
            "define %record.StatusGrid @make_grid()",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 153, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "ret %record.StatusGrid",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_return_run.or",
        sourced_scalar_choice_maybe_record_nested_array_return_lines
    );
    auto sourced_scalar_choice_record_nested_array_explicit_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grid() -> StatusGrid",
        "    return StatusGrid([[Empty, Ready(155 as UInt32)], [Empty, Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid()",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Ready(code) => code - 155 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_explicit_return_emit.or",
        sourced_scalar_choice_record_nested_array_explicit_return_lines,
        {
            "define %record.StatusGrid @make_grid()",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 155, 1",
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "ret %record.StatusGrid",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_explicit_return_run.or",
        sourced_scalar_choice_record_nested_array_explicit_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_explicit_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grid() -> StatusGrid",
        "    return StatusGrid([[Empty, Some(Ready(157 as UInt32))], [Empty, Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid()",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 157 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_explicit_return_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_explicit_return_lines,
        {
            "define %record.StatusGrid @make_grid()",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 157, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "ret %record.StatusGrid",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_explicit_return_run.or",
        sourced_scalar_choice_maybe_record_nested_array_explicit_return_lines
    );
    auto sourced_scalar_choice_record_nested_array_ternary_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grid(flag: Bool) -> StatusGrid",
        "    flag ? StatusGrid([[Empty, Ready(159 as UInt32)], [Empty, Empty]]) : StatusGrid([[Empty, Empty], [Ready(1 as UInt32), Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid(true)",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Ready(code) => code - 159 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_ternary_return_emit.or",
        sourced_scalar_choice_record_nested_array_ternary_return_lines,
        {
            "define %record.StatusGrid @make_grid(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "ternary.then.",
            "ternary.else.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 159, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "phi %record.StatusGrid",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_ternary_return_run.or",
        sourced_scalar_choice_record_nested_array_ternary_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_ternary_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grid(flag: Bool) -> StatusGrid",
        "    flag ? StatusGrid([[Empty, Some(Ready(161 as UInt32))], [Empty, Empty]]) : StatusGrid([[Some(Ready(1 as UInt32)), Empty], [Empty, Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid(true)",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 161 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_ternary_return_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_ternary_return_lines,
        {
            "define %record.StatusGrid @make_grid(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "ternary.then.",
            "ternary.else.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 161, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "phi %record.StatusGrid",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_ternary_return_run.or",
        sourced_scalar_choice_maybe_record_nested_array_ternary_return_lines
    );
    auto sourced_scalar_choice_record_nested_array_branch_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grid(flag: Bool) -> StatusGrid",
        "    if flag",
        "        return StatusGrid([[Empty, Ready(163 as UInt32)], [Empty, Empty]])",
        "    StatusGrid([[Empty, Empty], [Ready(1 as UInt32), Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid(true)",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Ready(code) => code - 163 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_branch_return_emit.or",
        sourced_scalar_choice_record_nested_array_branch_return_lines,
        {
            "define %record.StatusGrid @make_grid(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "if.then.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 163, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "ret %record.StatusGrid",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_branch_return_run.or",
        sourced_scalar_choice_record_nested_array_branch_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_branch_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grid(flag: Bool) -> StatusGrid",
        "    if flag",
        "        return StatusGrid([[Empty, Some(Ready(165 as UInt32))], [Empty, Empty]])",
        "    StatusGrid([[Some(Ready(1 as UInt32)), Empty], [Empty, Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid(true)",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 165 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_branch_return_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_branch_return_lines,
        {
            "define %record.StatusGrid @make_grid(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "if.then.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 165, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "ret %record.StatusGrid",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_branch_return_run.or",
        sourced_scalar_choice_maybe_record_nested_array_branch_return_lines
    );
    auto sourced_scalar_choice_record_nested_array_guard_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grid(flag: Bool) -> StatusGrid",
        "    guard flag else",
        "        return StatusGrid([[Empty, Ready(167 as UInt32)], [Empty, Empty]])",
        "    StatusGrid([[Empty, Empty], [Ready(1 as UInt32), Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid(false)",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Ready(code) => code - 167 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_guard_return_emit.or",
        sourced_scalar_choice_record_nested_array_guard_return_lines,
        {
            "define %record.StatusGrid @make_grid(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "guard.failure.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 167, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "ret %record.StatusGrid",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_guard_return_run.or",
        sourced_scalar_choice_record_nested_array_guard_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_guard_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grid(flag: Bool) -> StatusGrid",
        "    guard flag else",
        "        return StatusGrid([[Empty, Some(Ready(169 as UInt32))], [Empty, Empty]])",
        "    StatusGrid([[Some(Ready(1 as UInt32)), Empty], [Empty, Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid(false)",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 169 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_guard_return_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_guard_return_lines,
        {
            "define %record.StatusGrid @make_grid(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "guard.failure.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 169, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "ret %record.StatusGrid",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_guard_return_run.or",
        sourced_scalar_choice_maybe_record_nested_array_guard_return_lines
    );
    auto sourced_scalar_choice_record_nested_array_switch_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grid(flag: Bool) -> StatusGrid",
        "    switch flag",
        "        true => return StatusGrid([[Empty, Ready(171 as UInt32)], [Empty, Empty]])",
        "        false =>",
        "            let marker: UInt32 = 0 as UInt32",
        "    StatusGrid([[Empty, Empty], [Ready(1 as UInt32), Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid(true)",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Ready(code) => code - 171 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_switch_return_emit.or",
        sourced_scalar_choice_record_nested_array_switch_return_lines,
        {
            "define %record.StatusGrid @make_grid(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "switch i1 %flag",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 171, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "ret %record.StatusGrid",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_switch_return_run.or",
        sourced_scalar_choice_record_nested_array_switch_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_switch_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grid(flag: Bool) -> StatusGrid",
        "    switch flag",
        "        true => return StatusGrid([[Empty, Some(Ready(173 as UInt32))], [Empty, Empty]])",
        "        false =>",
        "            let marker: UInt32 = 0 as UInt32",
        "    StatusGrid([[Some(Ready(1 as UInt32)), Empty], [Empty, Empty]])",
        "function main() -> UInt32",
        "    let grid: StatusGrid = make_grid(true)",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 173 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_return_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_return_lines,
        {
            "define %record.StatusGrid @make_grid(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "switch i1 %flag",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 173, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "ret %record.StatusGrid",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_return_run.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_payload_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grid() -> Maybe<StatusGrid>",
        "    Some(StatusGrid([[Empty, Ready(175 as UInt32)], [Empty, Empty]]))",
        "function main() -> UInt32",
        "    switch make_grid()",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 175 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_payload_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_payload_lines,
        {
            "define { i1, %record.StatusGrid } @make_grid()",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 175, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_payload_run.or",
        sourced_scalar_choice_maybe_record_nested_array_payload_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_payload_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grid() -> Maybe<StatusGrid>",
        "    Some(StatusGrid([[Empty, Some(Ready(177 as UInt32))], [Empty, Empty]]))",
        "function main() -> UInt32",
        "    switch make_grid()",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 177 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_payload_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_payload_lines,
        {
            "define { i1, %record.StatusGrid } @make_grid()",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 177, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_payload_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_payload_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    maybe_grid = Some(StatusGrid([[Empty, Ready(187 as UInt32)], [Empty, Empty]]))",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 187 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "alloca { i1, %record.StatusGrid }",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 187, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    maybe_grid = Some(StatusGrid([[Empty, Some(Ready(189 as UInt32))], [Empty, Empty]]))",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 189 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "alloca { i1, %record.StatusGrid }",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 189, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_while_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    var index: UInt32 = 0 as UInt32",
        "    while index < 1 as UInt32",
        "        maybe_grid = Some(StatusGrid([[Empty, Ready(235 as UInt32)], [Empty, Empty]]))",
        "        index = index + 1 as UInt32",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 235 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_while_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_while_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "alloca { i1, %record.StatusGrid }",
            "while.condition",
            "while.body",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 235, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_while_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_while_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_while_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    var index: UInt32 = 0 as UInt32",
        "    while index < 1 as UInt32",
        "        maybe_grid = Some(StatusGrid([[Empty, Some(Ready(237 as UInt32))], [Empty, Empty]]))",
        "        index = index + 1 as UInt32",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 237 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_while_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_while_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "alloca { i1, %record.StatusGrid }",
            "while.condition",
            "while.body",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 237, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_while_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_while_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_unsafe_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    unsafe",
        "        maybe_grid = Some(StatusGrid([[Empty, Ready(239 as UInt32)], [Empty, Empty]]))",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 239 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_unsafe_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_unsafe_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "alloca { i1, %record.StatusGrid }",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 239, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_unsafe_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_unsafe_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_unsafe_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    unsafe",
        "        maybe_grid = Some(StatusGrid([[Empty, Some(Ready(241 as UInt32))], [Empty, Empty]]))",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 241 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_unsafe_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_unsafe_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "alloca { i1, %record.StatusGrid }",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 241, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_unsafe_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_unsafe_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_repeat_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    repeat",
        "        maybe_grid = Some(StatusGrid([[Empty, Ready(243 as UInt32)], [Empty, Empty]]))",
        "    while false",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 243 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_repeat_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_repeat_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "alloca { i1, %record.StatusGrid }",
            "repeat.body",
            "repeat.condition",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 243, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_repeat_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_repeat_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_repeat_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    repeat",
        "        maybe_grid = Some(StatusGrid([[Empty, Some(Ready(245 as UInt32))], [Empty, Empty]]))",
        "    while false",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 245 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_repeat_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_repeat_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "alloca { i1, %record.StatusGrid }",
            "repeat.body",
            "repeat.condition",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 245, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_repeat_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_repeat_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_for_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    for item in [0 as UInt32]",
        "        maybe_grid = Some(StatusGrid([[Empty, Ready(247 as UInt32)], [Empty, Empty]]))",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 247 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_for_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_for_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "alloca { i1, %record.StatusGrid }",
            "for.iteration",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 247, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_for_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_for_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_for_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    for item in [0 as UInt32]",
        "        maybe_grid = Some(StatusGrid([[Empty, Some(Ready(249 as UInt32))], [Empty, Empty]]))",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 249 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_for_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_for_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "alloca { i1, %record.StatusGrid }",
            "for.iteration",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 249, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_for_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_for_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_if_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    if true",
        "        maybe_grid = Some(StatusGrid([[Empty, Ready(251 as UInt32)], [Empty, Empty]]))",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 251 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_if_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_if_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "alloca { i1, %record.StatusGrid }",
            "if.then.",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 251, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_if_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_if_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_if_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    if true",
        "        maybe_grid = Some(StatusGrid([[Empty, Some(Ready(253 as UInt32))], [Empty, Empty]]))",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 253 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_if_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_if_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "alloca { i1, %record.StatusGrid }",
            "if.then.",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 253, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_if_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_if_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_guard_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function observe(maybe_grid: Maybe<StatusGrid>) -> UInt32",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 255 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    guard false else",
        "        maybe_grid = Some(StatusGrid([[Empty, Ready(255 as UInt32)], [Empty, Empty]]))",
        "        return observe(maybe_grid)",
        "    3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_guard_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_guard_assignment_lines,
        {
            "define i32 @observe({ i1, %record.StatusGrid } %maybe_grid)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "guard.failure.",
            "alloca { i1, %record.StatusGrid }",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 255, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "call i32 @observe({ i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_guard_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_guard_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_guard_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function observe(maybe_grid: Maybe<StatusGrid>) -> UInt32",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 257 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    guard false else",
        "        maybe_grid = Some(StatusGrid([[Empty, Some(Ready(257 as UInt32))], [Empty, Empty]]))",
        "        return observe(maybe_grid)",
        "    4 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_guard_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_guard_assignment_lines,
        {
            "define i32 @observe({ i1, %record.StatusGrid } %maybe_grid)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "guard.failure.",
            "alloca { i1, %record.StatusGrid }",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 257, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "call i32 @observe({ i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_guard_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_guard_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_switch_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    switch true",
        "        true =>",
        "            maybe_grid = Some(StatusGrid([[Empty, Ready(259 as UInt32)], [Empty, Empty]]))",
        "        false =>",
        "            maybe_grid = Empty",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 259 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "alloca { i1, %record.StatusGrid }",
            "switch i1",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 259, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_switch_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function main() -> UInt32",
        "    var maybe_grid: Maybe<StatusGrid> = Empty",
        "    switch true",
        "        true =>",
        "            maybe_grid = Some(StatusGrid([[Empty, Some(Ready(261 as UInt32))], [Empty, Empty]]))",
        "        false =>",
        "            maybe_grid = Empty",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 261 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_switch_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_switch_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "alloca { i1, %record.StatusGrid }",
            "switch i1",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 261, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_switch_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_switch_assignment_lines
    );
    auto sourced_scalar_choice_record_field_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusBox",
        "    status: LocalStatus",
        "function main() -> UInt32",
        "    var box: StatusBox = StatusBox(Empty)",
        "    box.status = Ready(89 as UInt32)",
        "    switch box.status",
        "        Ready(code) => code - 89 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_field_assignment_emit.or",
        sourced_scalar_choice_record_field_assignment_lines,
        {
            "alloca %record.StatusBox",
            "getelementptr %record.StatusBox",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 89, 1",
            "store { i32, i32 }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_field_assignment_run.or",
        sourced_scalar_choice_record_field_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_field_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusBox",
        "    maybe_status: Maybe<LocalStatus>",
        "function main() -> UInt32",
        "    var box: StatusBox = StatusBox(Empty)",
        "    box.maybe_status = Some(Ready(97 as UInt32))",
        "    switch box.maybe_status",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 97 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_field_assignment_emit.or",
        sourced_scalar_choice_maybe_record_field_assignment_lines,
        {
            "alloca %record.StatusBox",
            "getelementptr %record.StatusBox",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 97, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "store { i1, { i32, i32 } }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_field_assignment_run.or",
        sourced_scalar_choice_maybe_record_field_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_field_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlot",
        "    maybe_grid: Maybe<StatusGrid>",
        "function main() -> UInt32",
        "    var slot: GridSlot = GridSlot(Empty)",
        "    slot.maybe_grid = Some(StatusGrid([[Empty, Ready(191 as UInt32)], [Empty, Empty]]))",
        "    switch slot.maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 191 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_field_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_field_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlot = type { { i1, %record.StatusGrid } }",
            "alloca %record.GridSlot",
            "getelementptr %record.GridSlot",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 191, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_field_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_field_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_field_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlot",
        "    maybe_grid: Maybe<StatusGrid>",
        "function main() -> UInt32",
        "    var slot: GridSlot = GridSlot(Empty)",
        "    slot.maybe_grid = Some(StatusGrid([[Empty, Some(Ready(193 as UInt32))], [Empty, Empty]]))",
        "    switch slot.maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 193 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_field_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_field_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlot = type { { i1, %record.StatusGrid } }",
            "alloca %record.GridSlot",
            "getelementptr %record.GridSlot",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 193, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_field_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_field_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_if_field_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlot",
        "    maybe_grid: Maybe<StatusGrid>",
        "function main() -> UInt32",
        "    var slot: GridSlot = GridSlot(Empty)",
        "    if true",
        "        slot.maybe_grid = Some(StatusGrid([[Empty, Ready(263 as UInt32)], [Empty, Empty]]))",
        "    switch slot.maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 263 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_if_field_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_if_field_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlot = type { { i1, %record.StatusGrid } }",
            "alloca %record.GridSlot",
            "if.then.",
            "getelementptr %record.GridSlot",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 263, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_if_field_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_if_field_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_if_field_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlot",
        "    maybe_grid: Maybe<StatusGrid>",
        "function main() -> UInt32",
        "    var slot: GridSlot = GridSlot(Empty)",
        "    if true",
        "        slot.maybe_grid = Some(StatusGrid([[Empty, Some(Ready(265 as UInt32))], [Empty, Empty]]))",
        "    switch slot.maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 265 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_if_field_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_if_field_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlot = type { { i1, %record.StatusGrid } }",
            "alloca %record.GridSlot",
            "if.then.",
            "getelementptr %record.GridSlot",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 265, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_if_field_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_if_field_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_switch_field_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlot",
        "    maybe_grid: Maybe<StatusGrid>",
        "function main() -> UInt32",
        "    var slot: GridSlot = GridSlot(Empty)",
        "    switch true",
        "        true =>",
        "            slot.maybe_grid = Some(StatusGrid([[Empty, Ready(267 as UInt32)], [Empty, Empty]]))",
        "        false =>",
        "            slot.maybe_grid = Empty",
        "    switch slot.maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 267 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_field_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_field_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlot = type { { i1, %record.StatusGrid } }",
            "alloca %record.GridSlot",
            "switch i1",
            "getelementptr %record.GridSlot",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 267, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_field_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_field_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_switch_field_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlot",
        "    maybe_grid: Maybe<StatusGrid>",
        "function main() -> UInt32",
        "    var slot: GridSlot = GridSlot(Empty)",
        "    switch true",
        "        true =>",
        "            slot.maybe_grid = Some(StatusGrid([[Empty, Some(Ready(269 as UInt32))], [Empty, Empty]]))",
        "        false =>",
        "            slot.maybe_grid = Empty",
        "    switch slot.maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 269 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_switch_field_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_switch_field_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlot = type { { i1, %record.StatusGrid } }",
            "alloca %record.GridSlot",
            "switch i1",
            "getelementptr %record.GridSlot",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 269, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_switch_field_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_switch_field_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Ready(195 as UInt32)], [Empty, Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 195 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 195, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Some(Ready(197 as UInt32))], [Empty, Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 197 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 197, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_element_assignment_alt_index_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Empty], [Ready(301 as UInt32), Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[1 as UInt64][0 as UInt64]",
        "                Ready(code) => code - 301 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_element_assignment_alt_index_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_element_assignment_alt_index_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 301, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_element_assignment_alt_index_run.or",
        sourced_scalar_choice_maybe_record_nested_array_element_assignment_alt_index_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_element_assignment_alt_index_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Empty], [Some(Ready(303 as UInt32)), Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[1 as UInt64][0 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 303 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_element_assignment_alt_index_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_element_assignment_alt_index_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 303, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_element_assignment_alt_index_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_element_assignment_alt_index_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_if_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    if true",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Ready(271 as UInt32)], [Empty, Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 271 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_if_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_if_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "if.then.",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 271, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_if_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_if_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_if_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    if true",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Some(Ready(273 as UInt32))], [Empty, Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 273 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_if_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_if_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "if.then.",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 273, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_if_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_if_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_if_element_assignment_alt_index_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    if true",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Empty], [Ready(305 as UInt32), Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[1 as UInt64][0 as UInt64]",
        "                Ready(code) => code - 305 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_if_element_assignment_alt_index_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_if_element_assignment_alt_index_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "if.then.",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 305, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_if_element_assignment_alt_index_run.or",
        sourced_scalar_choice_maybe_record_nested_array_if_element_assignment_alt_index_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_if_element_assignment_alt_index_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    if true",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Empty], [Some(Ready(307 as UInt32)), Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[1 as UInt64][0 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 307 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_if_element_assignment_alt_index_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_if_element_assignment_alt_index_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "if.then.",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 307, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_if_element_assignment_alt_index_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_if_element_assignment_alt_index_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_switch_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    switch true",
        "        true =>",
        "            slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Ready(277 as UInt32)], [Empty, Empty]]))",
        "        false =>",
        "            slots.grids[0 as UInt64] = Empty",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 277 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "switch i1",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 277, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_switch_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    switch true",
        "        true =>",
        "            slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Some(Ready(279 as UInt32))], [Empty, Empty]]))",
        "        false =>",
        "            slots.grids[0 as UInt64] = Empty",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 279 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_switch_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_switch_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "switch i1",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 279, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_switch_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_switch_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_switch_element_assignment_alt_index_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    switch true",
        "        true =>",
        "            slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Empty], [Ready(309 as UInt32), Empty]]))",
        "        false =>",
        "            slots.grids[0 as UInt64] = Empty",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[1 as UInt64][0 as UInt64]",
        "                Ready(code) => code - 309 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_element_assignment_alt_index_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_element_assignment_alt_index_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "switch i1",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 309, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_element_assignment_alt_index_run.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_element_assignment_alt_index_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_switch_element_assignment_alt_index_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    switch true",
        "        true =>",
        "            slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Empty], [Some(Ready(311 as UInt32)), Empty]]))",
        "        false =>",
        "            slots.grids[0 as UInt64] = Empty",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[1 as UInt64][0 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 311 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_switch_element_assignment_alt_index_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_switch_element_assignment_alt_index_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "switch i1",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 311, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_switch_element_assignment_alt_index_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_switch_element_assignment_alt_index_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_while_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    var index: UInt32 = 0 as UInt32",
        "    while index < 1 as UInt32",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Ready(281 as UInt32)], [Empty, Empty]]))",
        "        index = index + 1 as UInt32",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 281 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_while_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_while_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "while.condition",
            "while.body",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 281, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_while_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_while_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_while_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    var index: UInt32 = 0 as UInt32",
        "    while index < 1 as UInt32",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Some(Ready(283 as UInt32))], [Empty, Empty]]))",
        "        index = index + 1 as UInt32",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 283 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_while_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_while_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "while.condition",
            "while.body",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 283, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_while_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_while_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_while_element_assignment_alt_index_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    var index: UInt32 = 0 as UInt32",
        "    while index < 1 as UInt32",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Empty], [Ready(313 as UInt32), Empty]]))",
        "        index = index + 1 as UInt32",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[1 as UInt64][0 as UInt64]",
        "                Ready(code) => code - 313 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_while_element_assignment_alt_index_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_while_element_assignment_alt_index_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "while.condition",
            "while.body",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 313, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_while_element_assignment_alt_index_run.or",
        sourced_scalar_choice_maybe_record_nested_array_while_element_assignment_alt_index_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_while_element_assignment_alt_index_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    var index: UInt32 = 0 as UInt32",
        "    while index < 1 as UInt32",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Empty], [Some(Ready(315 as UInt32)), Empty]]))",
        "        index = index + 1 as UInt32",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[1 as UInt64][0 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 315 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_while_element_assignment_alt_index_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_while_element_assignment_alt_index_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "while.condition",
            "while.body",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 315, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_while_element_assignment_alt_index_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_while_element_assignment_alt_index_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_repeat_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    repeat",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Ready(285 as UInt32)], [Empty, Empty]]))",
        "    while false",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 285 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_repeat_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_repeat_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "repeat.body",
            "repeat.condition",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 285, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_repeat_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_repeat_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_repeat_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    repeat",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Some(Ready(287 as UInt32))], [Empty, Empty]]))",
        "    while false",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 287 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_repeat_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_repeat_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "repeat.body",
            "repeat.condition",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 287, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_repeat_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_repeat_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_for_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    for item in [0 as UInt32]",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Ready(289 as UInt32)], [Empty, Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 289 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_for_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_for_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "for.iteration",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 289, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_for_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_for_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_for_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    for item in [0 as UInt32]",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Some(Ready(291 as UInt32))], [Empty, Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 291 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_for_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_for_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "for.iteration",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 291, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_for_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_for_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_unsafe_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    unsafe",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Ready(293 as UInt32)], [Empty, Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 293 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_unsafe_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_unsafe_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 293, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_unsafe_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_unsafe_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_unsafe_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    unsafe",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Some(Ready(295 as UInt32))], [Empty, Empty]]))",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 295 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_unsafe_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_unsafe_element_assignment_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "alloca %record.GridSlots",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 295, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_unsafe_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_unsafe_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_guard_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function observe(slots: GridSlots) -> UInt32",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 297 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    guard false else",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Ready(297 as UInt32)], [Empty, Empty]]))",
        "        return observe(slots)",
        "    3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_guard_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_guard_element_assignment_lines,
        {
            "define i32 @observe(%record.GridSlots %slots)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "guard.failure.",
            "alloca %record.GridSlots",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 297, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "call i32 @observe(%record.GridSlots",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_guard_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_guard_element_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_guard_element_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "record GridSlots",
        "    grids: Array<Maybe<StatusGrid>, 2>",
        "function observe(slots: GridSlots) -> UInt32",
        "    switch slots.grids[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 299 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
        "function main() -> UInt32",
        "    var slots: GridSlots = GridSlots([Empty, Empty])",
        "    guard false else",
        "        slots.grids[0 as UInt64] = Some(StatusGrid([[Empty, Some(Ready(299 as UInt32))], [Empty, Empty]]))",
        "        return observe(slots)",
        "    4 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_guard_element_assignment_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_guard_element_assignment_lines,
        {
            "define i32 @observe(%record.GridSlots %slots)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "%record.GridSlots = type { [2 x { i1, %record.StatusGrid }] }",
            "guard.failure.",
            "alloca %record.GridSlots",
            "getelementptr %record.GridSlots",
            "getelementptr [2 x { i1, %record.StatusGrid }]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 false, 0",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 299, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "store { i1, %record.StatusGrid }",
            "call i32 @observe(%record.GridSlots",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_guard_element_assignment_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_guard_element_assignment_lines
    );
    auto sourced_scalar_choice_record_array_index_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusList",
        "    statuses: Array<LocalStatus, 2>",
        "function main() -> UInt32",
        "    var list: StatusList = StatusList([Empty, Empty])",
        "    list.statuses[0 as UInt64] = Ready(101 as UInt32)",
        "    switch list.statuses[0 as UInt64]",
        "        Ready(code) => code - 101 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_array_index_assignment_emit.or",
        sourced_scalar_choice_record_array_index_assignment_lines,
        {
            "alloca %record.StatusList",
            "getelementptr %record.StatusList",
            "getelementptr [2 x { i32, i32 }]",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 101, 1",
            "store { i32, i32 }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_array_index_assignment_run.or",
        sourced_scalar_choice_record_array_index_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_array_index_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusList",
        "    statuses: Array<Maybe<LocalStatus>, 2>",
        "function main() -> UInt32",
        "    var list: StatusList = StatusList([Empty, Empty])",
        "    list.statuses[0 as UInt64] = Some(Ready(103 as UInt32))",
        "    switch list.statuses[0 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 103 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_array_index_assignment_emit.or",
        sourced_scalar_choice_maybe_record_array_index_assignment_lines,
        {
            "alloca %record.StatusList",
            "getelementptr %record.StatusList",
            "getelementptr [2 x { i1, { i32, i32 } }]",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 103, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "store { i1, { i32, i32 } }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_array_index_assignment_run.or",
        sourced_scalar_choice_maybe_record_array_index_assignment_lines
    );
    auto sourced_scalar_choice_record_nested_array_index_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function main() -> UInt32",
        "    var grid: StatusGrid = StatusGrid([[Empty, Empty], [Empty, Empty]])",
        "    grid.statuses[0 as UInt64][1 as UInt64] = Ready(147 as UInt32)",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Ready(code) => code - 147 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_index_assignment_emit.or",
        sourced_scalar_choice_record_nested_array_index_assignment_lines,
        {
            "alloca %record.StatusGrid",
            "getelementptr %record.StatusGrid",
            "getelementptr [2 x [2 x { i32, i32 }]]",
            "getelementptr [2 x { i32, i32 }]",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 147, 1",
            "store { i32, i32 }",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_record_nested_array_index_assignment_run.or",
        sourced_scalar_choice_record_nested_array_index_assignment_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_index_assignment_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function main() -> UInt32",
        "    var grid: StatusGrid = StatusGrid([[Empty, Empty], [Empty, Empty]])",
        "    grid.statuses[0 as UInt64][1 as UInt64] = Some(Ready(149 as UInt32))",
        "    switch grid.statuses[0 as UInt64][1 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 149 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_index_assignment_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_index_assignment_lines,
        {
            "alloca %record.StatusGrid",
            "getelementptr %record.StatusGrid",
            "getelementptr [2 x [2 x { i1, { i32, i32 } }]]",
            "getelementptr [2 x { i1, { i32, i32 } }]",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 149, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "store { i1, { i32, i32 } }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_index_assignment_run.or",
        sourced_scalar_choice_maybe_record_nested_array_index_assignment_lines
    );
    auto sourced_scalar_choice_for_array_binding_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function main() -> UInt32",
        "    let statuses: Array<LocalStatus, 2> = [Ready(107 as UInt32), Empty]",
        "    var total: UInt32 = 0 as UInt32",
        "    for status in statuses",
        "        switch status",
        "            Ready(code) =>",
        "                total = total + code",
        "            Empty =>",
        "                total = total + 0 as UInt32",
        "    total - 107 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_for_array_binding_emit.or",
        sourced_scalar_choice_for_array_binding_lines,
        {
            "extractvalue [2 x { i32, i32 }]",
            "i32 107, 1",
            "switch i32",
            "load i32",
            "store i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_for_array_binding_run.or",
        sourced_scalar_choice_for_array_binding_lines
    );
    auto sourced_scalar_choice_maybe_for_array_binding_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function main() -> UInt32",
        "    let statuses: Array<Maybe<LocalStatus>, 2> = [Some(Ready(109 as UInt32)), Empty]",
        "    var total: UInt32 = 0 as UInt32",
        "    for maybe_status in statuses",
        "        switch maybe_status",
        "            Some(status) =>",
        "                switch status",
        "                    Ready(code) =>",
        "                        total = total + code",
        "                    Empty =>",
        "                        total = total + 0 as UInt32",
        "            Empty =>",
        "                total = total + 0 as UInt32",
        "    total - 109 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_for_array_binding_emit.or",
        sourced_scalar_choice_maybe_for_array_binding_lines,
        {
            "extractvalue [2 x { i1, { i32, i32 } }]",
            "i32 109, 1",
            "switch i1",
            "switch i32",
            "load i32",
            "store i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_for_array_binding_run.or",
        sourced_scalar_choice_maybe_for_array_binding_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_for_returned_array_binding_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grids() -> Array<Maybe<StatusGrid>, 2>",
        "    [Some(StatusGrid([[Empty, Ready(223 as UInt32)], [Empty, Empty]])), Empty]",
        "function main() -> UInt32",
        "    var total: UInt32 = 0 as UInt32",
        "    for maybe_grid in make_grids()",
        "        switch maybe_grid",
        "            Some(grid) =>",
        "                switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                    Ready(code) =>",
        "                        total = total + code",
        "                    Empty =>",
        "                        total = total + 0 as UInt32",
        "            Empty =>",
        "                total = total + 0 as UInt32",
        "    total - 223 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_for_returned_array_binding_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_for_returned_array_binding_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids()",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "i32 223, 1",
            "extractvalue [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
            "load i32",
            "store i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_for_returned_array_binding_run.or",
        sourced_scalar_choice_maybe_record_nested_array_for_returned_array_binding_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_for_returned_array_binding_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grids() -> Array<Maybe<StatusGrid>, 2>",
        "    [Some(StatusGrid([[Empty, Some(Ready(225 as UInt32))], [Empty, Empty]])), Empty]",
        "function main() -> UInt32",
        "    var total: UInt32 = 0 as UInt32",
        "    for maybe_grid in make_grids()",
        "        switch maybe_grid",
        "            Some(grid) =>",
        "                switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                    Some(status) =>",
        "                        switch status",
        "                            Ready(code) =>",
        "                                total = total + code",
        "                            Empty =>",
        "                                total = total + 0 as UInt32",
        "                    Empty =>",
        "                        total = total + 0 as UInt32",
        "            Empty =>",
        "                total = total + 0 as UInt32",
        "    total - 225 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_for_returned_array_binding_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_for_returned_array_binding_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids()",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "i32 225, 1",
            "extractvalue [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
            "load i32",
            "store i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_for_returned_array_binding_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_for_returned_array_binding_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_for_cast_array_literal_binding_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function main() -> UInt32",
        "    var total: UInt32 = 0 as UInt32",
        "    for maybe_grid in [Some(StatusGrid([[Empty, Ready(227 as UInt32)], [Empty, Empty]])), Empty] as Array<Maybe<StatusGrid>, 2>",
        "        switch maybe_grid",
        "            Some(grid) =>",
        "                switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                    Ready(code) =>",
        "                        total = total + code",
        "                    Empty =>",
        "                        total = total + 0 as UInt32",
        "            Empty =>",
        "                total = total + 0 as UInt32",
        "    total - 227 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_for_cast_array_literal_binding_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_for_cast_array_literal_binding_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "i32 227, 1",
            "extractvalue [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
            "load i32",
            "store i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_for_cast_array_literal_binding_run.or",
        sourced_scalar_choice_maybe_record_nested_array_for_cast_array_literal_binding_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_for_cast_array_literal_binding_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function main() -> UInt32",
        "    var total: UInt32 = 0 as UInt32",
        "    for maybe_grid in [Some(StatusGrid([[Empty, Some(Ready(229 as UInt32))], [Empty, Empty]])), Empty] as Array<Maybe<StatusGrid>, 2>",
        "        switch maybe_grid",
        "            Some(grid) =>",
        "                switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                    Some(status) =>",
        "                        switch status",
        "                            Ready(code) =>",
        "                                total = total + code",
        "                            Empty =>",
        "                                total = total + 0 as UInt32",
        "                    Empty =>",
        "                        total = total + 0 as UInt32",
        "            Empty =>",
        "                total = total + 0 as UInt32",
        "    total - 229 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_for_cast_array_literal_binding_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_for_cast_array_literal_binding_lines,
        {
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "i32 229, 1",
            "extractvalue [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
            "load i32",
            "store i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_for_cast_array_literal_binding_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_for_cast_array_literal_binding_lines
    );
    auto sourced_scalar_choice_for_cast_array_literal_binding_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function main() -> UInt32",
        "    var total: UInt32 = 0 as UInt32",
        "    for status in [Ready(111 as UInt32), Empty] as Array<LocalStatus, 2>",
        "        switch status",
        "            Ready(code) =>",
        "                total = total + code",
        "            Empty =>",
        "                total = total + 0 as UInt32",
        "    total - 111 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_for_cast_array_literal_binding_emit.or",
        sourced_scalar_choice_for_cast_array_literal_binding_lines,
        {
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "i32 111, 1",
            "extractvalue [2 x { i32, i32 }]",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_for_cast_array_literal_binding_run.or",
        sourced_scalar_choice_for_cast_array_literal_binding_lines
    );
    auto sourced_scalar_choice_maybe_for_cast_array_literal_binding_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function main() -> UInt32",
        "    var total: UInt32 = 0 as UInt32",
        "    for maybe_status in [Some(Ready(113 as UInt32)), Empty] as Array<Maybe<LocalStatus>, 2>",
        "        switch maybe_status",
        "            Some(status) =>",
        "                switch status",
        "                    Ready(code) =>",
        "                        total = total + code",
        "                    Empty =>",
        "                        total = total + 0 as UInt32",
        "            Empty =>",
        "                total = total + 0 as UInt32",
        "    total - 113 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_for_cast_array_literal_binding_emit.or",
        sourced_scalar_choice_maybe_for_cast_array_literal_binding_lines,
        {
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "i32 113, 1",
            "extractvalue [2 x { i1, { i32, i32 } }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_for_cast_array_literal_binding_run.or",
        sourced_scalar_choice_maybe_for_cast_array_literal_binding_lines
    );
    auto sourced_scalar_choice_cast_array_final_expression_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function make_statuses() -> Array<LocalStatus, 2>",
        "    [Ready(119 as UInt32), Empty] as Array<LocalStatus, 2>",
        "function main() -> UInt32",
        "    switch make_statuses()[0 as UInt64]",
        "        Ready(code) => code - 119 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_cast_array_final_expression_emit.or",
        sourced_scalar_choice_cast_array_final_expression_lines,
        {
            "define [2 x { i32, i32 }] @make_statuses()",
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "i32 119, 1",
            "ret [2 x { i32, i32 }]",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_cast_array_final_expression_run.or",
        sourced_scalar_choice_cast_array_final_expression_lines
    );
    auto sourced_scalar_choice_cast_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function make_statuses() -> Array<LocalStatus, 2>",
        "    return [Ready(121 as UInt32), Empty] as Array<LocalStatus, 2>",
        "function main() -> UInt32",
        "    switch make_statuses()[0 as UInt64]",
        "        Ready(code) => code - 121 as UInt32",
        "        Empty => 1 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_cast_array_return_emit.or",
        sourced_scalar_choice_cast_array_return_lines,
        {
            "define [2 x { i32, i32 }] @make_statuses()",
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "i32 121, 1",
            "ret [2 x { i32, i32 }]",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_cast_array_return_run.or",
        sourced_scalar_choice_cast_array_return_lines
    );
    auto sourced_scalar_choice_maybe_cast_array_final_expression_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function make_statuses() -> Array<Maybe<LocalStatus>, 2>",
        "    [Some(Ready(123 as UInt32)), Empty] as Array<Maybe<LocalStatus>, 2>",
        "function main() -> UInt32",
        "    switch make_statuses()[0 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 123 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_cast_array_final_expression_emit.or",
        sourced_scalar_choice_maybe_cast_array_final_expression_lines,
        {
            "define [2 x { i1, { i32, i32 } }] @make_statuses()",
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "i32 123, 1",
            "ret [2 x { i1, { i32, i32 } }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_cast_array_final_expression_run.or",
        sourced_scalar_choice_maybe_cast_array_final_expression_lines
    );
    auto sourced_scalar_choice_maybe_cast_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function make_statuses() -> Array<Maybe<LocalStatus>, 2>",
        "    return [Some(Ready(125 as UInt32)), Empty] as Array<Maybe<LocalStatus>, 2>",
        "function main() -> UInt32",
        "    switch make_statuses()[0 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 125 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_cast_array_return_emit.or",
        sourced_scalar_choice_maybe_cast_array_return_lines,
        {
            "define [2 x { i1, { i32, i32 } }] @make_statuses()",
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "i32 125, 1",
            "ret [2 x { i1, { i32, i32 } }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_cast_array_return_run.or",
        sourced_scalar_choice_maybe_cast_array_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_final_expression_array_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grids() -> Array<Maybe<StatusGrid>, 2>",
        "    [Some(StatusGrid([[Empty, Ready(203 as UInt32)], [Empty, Empty]])), Empty]",
        "function main() -> UInt32",
        "    switch make_grids()[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 203 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_final_expression_array_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_final_expression_array_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids()",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 203, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "ret [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_final_expression_array_run.or",
        sourced_scalar_choice_maybe_record_nested_array_final_expression_array_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_final_expression_array_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grids() -> Array<Maybe<StatusGrid>, 2>",
        "    [Some(StatusGrid([[Empty, Some(Ready(205 as UInt32))], [Empty, Empty]])), Empty]",
        "function main() -> UInt32",
        "    switch make_grids()[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 205 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_final_expression_array_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_final_expression_array_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids()",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 205, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "ret [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_final_expression_array_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_final_expression_array_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_ternary_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grids(flag: Bool) -> Array<Maybe<StatusGrid>, 2>",
        "    flag ? [Some(StatusGrid([[Empty, Ready(207 as UInt32)], [Empty, Empty]])), Empty] : [Empty, Some(StatusGrid([[Empty, Empty], [Ready(1 as UInt32), Empty]]))]",
        "function main() -> UInt32",
        "    switch make_grids(true)[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 207 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_ternary_array_return_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_ternary_array_return_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "ternary.then.",
            "ternary.else.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 207, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "phi [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_ternary_array_return_run.or",
        sourced_scalar_choice_maybe_record_nested_array_ternary_array_return_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_ternary_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grids(flag: Bool) -> Array<Maybe<StatusGrid>, 2>",
        "    flag ? [Some(StatusGrid([[Empty, Some(Ready(209 as UInt32))], [Empty, Empty]])), Empty] : [Empty, Some(StatusGrid([[Some(Ready(1 as UInt32)), Empty], [Empty, Empty]]))]",
        "function main() -> UInt32",
        "    switch make_grids(true)[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 209 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_ternary_array_return_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_ternary_array_return_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "ternary.then.",
            "ternary.else.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 209, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "phi [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_ternary_array_return_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_ternary_array_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_branch_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grids(flag: Bool) -> Array<Maybe<StatusGrid>, 2>",
        "    if flag",
        "        return [Some(StatusGrid([[Empty, Ready(211 as UInt32)], [Empty, Empty]])), Empty]",
        "    [Empty, Some(StatusGrid([[Empty, Empty], [Ready(1 as UInt32), Empty]]))]",
        "function main() -> UInt32",
        "    switch make_grids(true)[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 211 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_branch_array_return_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_branch_array_return_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "if.then.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 211, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "ret [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_branch_array_return_run.or",
        sourced_scalar_choice_maybe_record_nested_array_branch_array_return_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_branch_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grids(flag: Bool) -> Array<Maybe<StatusGrid>, 2>",
        "    if flag",
        "        return [Some(StatusGrid([[Empty, Some(Ready(213 as UInt32))], [Empty, Empty]])), Empty]",
        "    [Empty, Some(StatusGrid([[Some(Ready(1 as UInt32)), Empty], [Empty, Empty]]))]",
        "function main() -> UInt32",
        "    switch make_grids(true)[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 213 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_branch_array_return_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_branch_array_return_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "if.then.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 213, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "ret [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_branch_array_return_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_branch_array_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_guard_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grids(flag: Bool) -> Array<Maybe<StatusGrid>, 2>",
        "    guard flag else",
        "        return [Some(StatusGrid([[Empty, Ready(215 as UInt32)], [Empty, Empty]])), Empty]",
        "    [Empty, Some(StatusGrid([[Empty, Empty], [Ready(1 as UInt32), Empty]]))]",
        "function main() -> UInt32",
        "    switch make_grids(false)[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 215 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_guard_array_return_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_guard_array_return_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "guard.failure.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 215, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "ret [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_guard_array_return_run.or",
        sourced_scalar_choice_maybe_record_nested_array_guard_array_return_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_guard_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grids(flag: Bool) -> Array<Maybe<StatusGrid>, 2>",
        "    guard flag else",
        "        return [Some(StatusGrid([[Empty, Some(Ready(217 as UInt32))], [Empty, Empty]])), Empty]",
        "    [Empty, Some(StatusGrid([[Some(Ready(1 as UInt32)), Empty], [Empty, Empty]]))]",
        "function main() -> UInt32",
        "    switch make_grids(false)[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 217 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_guard_array_return_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_guard_array_return_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "guard.failure.",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 217, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "ret [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_guard_array_return_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_guard_array_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_switch_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grids(flag: Bool) -> Array<Maybe<StatusGrid>, 2>",
        "    switch flag",
        "        true => return [Some(StatusGrid([[Empty, Ready(219 as UInt32)], [Empty, Empty]])), Empty]",
        "        false =>",
        "            let marker: UInt32 = 0 as UInt32",
        "    [Empty, Some(StatusGrid([[Empty, Empty], [Ready(1 as UInt32), Empty]]))]",
        "function main() -> UInt32",
        "    switch make_grids(true)[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 219 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_array_return_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_array_return_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "switch i1 %flag",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 219, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "ret [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_switch_array_return_run.or",
        sourced_scalar_choice_maybe_record_nested_array_switch_array_return_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_switch_array_return_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grids(flag: Bool) -> Array<Maybe<StatusGrid>, 2>",
        "    switch flag",
        "        true => return [Some(StatusGrid([[Empty, Some(Ready(221 as UInt32))], [Empty, Empty]])), Empty]",
        "        false =>",
        "            let marker: UInt32 = 0 as UInt32",
        "    [Empty, Some(StatusGrid([[Some(Ready(1 as UInt32)), Empty], [Empty, Empty]]))]",
        "function main() -> UInt32",
        "    switch make_grids(true)[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 221 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_switch_array_return_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_switch_array_return_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids(i1 %flag)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "switch i1 %flag",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 221, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "ret [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_switch_array_return_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_switch_array_return_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_return_array_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function make_grids() -> Array<Maybe<StatusGrid>, 2>",
        "    return [Some(StatusGrid([[Empty, Ready(199 as UInt32)], [Empty, Empty]])), Empty]",
        "function main() -> UInt32",
        "    switch make_grids()[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 199 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_return_array_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_return_array_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids()",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 199, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "ret [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_return_array_run.or",
        sourced_scalar_choice_maybe_record_nested_array_return_array_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_return_array_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function make_grids() -> Array<Maybe<StatusGrid>, 2>",
        "    return [Some(StatusGrid([[Empty, Some(Ready(201 as UInt32))], [Empty, Empty]])), Empty]",
        "function main() -> UInt32",
        "    switch make_grids()[0 as UInt64]",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 201 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_return_array_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_return_array_lines,
        {
            "define [2 x { i1, %record.StatusGrid }] @make_grids()",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 201, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "insertvalue [2 x { i1, %record.StatusGrid }] undef, { i1, %record.StatusGrid }",
            "ret [2 x { i1, %record.StatusGrid }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_return_array_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_return_array_lines
    );
    auto sourced_scalar_choice_array_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function consume(statuses: Array<LocalStatus, 2>) -> UInt32",
        "    switch statuses[0 as UInt64]",
        "        Ready(code) => code - 127 as UInt32",
        "        Empty => 1 as UInt32",
        "function main() -> UInt32",
        "    consume([Ready(127 as UInt32), Empty])",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_array_call_argument_emit.or",
        sourced_scalar_choice_array_call_argument_lines,
        {
            "define i32 @consume([2 x { i32, i32 }]",
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "i32 127, 1",
            "call i32 @consume([2 x { i32, i32 }]",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_array_call_argument_run.or",
        sourced_scalar_choice_array_call_argument_lines
    );
    auto sourced_scalar_choice_maybe_array_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function consume(statuses: Array<Maybe<LocalStatus>, 2>) -> UInt32",
        "    switch statuses[0 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 129 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
        "function main() -> UInt32",
        "    consume([Some(Ready(129 as UInt32)), Empty])",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_array_call_argument_emit.or",
        sourced_scalar_choice_maybe_array_call_argument_lines,
        {
            "define i32 @consume([2 x { i1, { i32, i32 } }]",
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "i32 129, 1",
            "call i32 @consume([2 x { i1, { i32, i32 } }]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_array_call_argument_run.or",
        sourced_scalar_choice_maybe_array_call_argument_lines
    );
    auto sourced_scalar_choice_nested_array_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "function consume(statuses: Array<Array<LocalStatus, 2>, 2>) -> UInt32",
        "    switch statuses[0 as UInt64][1 as UInt64]",
        "        Ready(code) => code - 135 as UInt32",
        "        Empty => 1 as UInt32",
        "function main() -> UInt32",
        "    consume([[Empty, Ready(135 as UInt32)], [Empty, Empty]])",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_nested_array_call_argument_emit.or",
        sourced_scalar_choice_nested_array_call_argument_lines,
        {
            "define i32 @consume([2 x [2 x { i32, i32 }]]",
            "insertvalue [2 x { i32, i32 }] undef, { i32, i32 }",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "i32 135, 1",
            "call i32 @consume([2 x [2 x { i32, i32 }]]",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_nested_array_call_argument_run.or",
        sourced_scalar_choice_nested_array_call_argument_lines
    );
    auto sourced_scalar_choice_maybe_nested_array_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function consume(statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>) -> UInt32",
        "    switch statuses[0 as UInt64][1 as UInt64]",
        "        Some(status) =>",
        "            switch status",
        "                Ready(code) => code - 137 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
        "function main() -> UInt32",
        "    consume([[Empty, Some(Ready(137 as UInt32))], [Empty, Empty]])",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_nested_array_call_argument_emit.or",
        sourced_scalar_choice_maybe_nested_array_call_argument_lines,
        {
            "define i32 @consume([2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue [2 x { i1, { i32, i32 } }] undef, { i1, { i32, i32 } }",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "i32 137, 1",
            "call i32 @consume([2 x [2 x { i1, { i32, i32 } }]]",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_nested_array_call_argument_run.or",
        sourced_scalar_choice_maybe_nested_array_call_argument_lines
    );
    auto sourced_scalar_choice_maybe_record_nested_array_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<LocalStatus, 2>, 2>",
        "function consume(maybe_grid: Maybe<StatusGrid>) -> UInt32",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Ready(code) => code - 183 as UInt32",
        "                Empty => 1 as UInt32",
        "        Empty => 2 as UInt32",
        "function main() -> UInt32",
        "    consume(Some(StatusGrid([[Empty, Ready(183 as UInt32)], [Empty, Empty]])))",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_call_argument_emit.or",
        sourced_scalar_choice_maybe_record_nested_array_call_argument_lines,
        {
            "define i32 @consume({ i1, %record.StatusGrid } %maybe_grid)",
            "%record.StatusGrid = type { [2 x [2 x { i32, i32 }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 183, 1",
            "insertvalue [2 x [2 x { i32, i32 }]] undef, [2 x { i32, i32 }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i32, i32 }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "call i32 @consume({ i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_nested_array_call_argument_run.or",
        sourced_scalar_choice_maybe_record_nested_array_call_argument_lines
    );
    auto sourced_scalar_choice_maybe_record_maybe_nested_array_call_argument_lines = std::vector<std::string_view> {
        "package demo.cli",
        "choice LocalStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice RemoteStatus",
        "    Ready(code: UInt32)",
        "    Empty",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record StatusGrid",
        "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
        "function consume(maybe_grid: Maybe<StatusGrid>) -> UInt32",
        "    switch maybe_grid",
        "        Some(grid) =>",
        "            switch grid.statuses[0 as UInt64][1 as UInt64]",
        "                Some(status) =>",
        "                    switch status",
        "                        Ready(code) => code - 185 as UInt32",
        "                        Empty => 1 as UInt32",
        "                Empty => 2 as UInt32",
        "        Empty => 3 as UInt32",
        "function main() -> UInt32",
        "    consume(Some(StatusGrid([[Empty, Some(Ready(185 as UInt32))], [Empty, Empty]])))",
    };
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_call_argument_emit.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_call_argument_lines,
        {
            "define i32 @consume({ i1, %record.StatusGrid } %maybe_grid)",
            "%record.StatusGrid = type { [2 x [2 x { i1, { i32, i32 } }]] }",
            "insertvalue { i32, i32 } undef, i32 0, 0",
            "i32 185, 1",
            "insertvalue { i1, { i32, i32 } } undef, i1 true, 0",
            "insertvalue [2 x [2 x { i1, { i32, i32 } }]] undef, [2 x { i1, { i32, i32 } }]",
            "insertvalue %record.StatusGrid undef, [2 x [2 x { i1, { i32, i32 } }]]",
            "insertvalue { i1, %record.StatusGrid } undef, i1 true, 0",
            "call i32 @consume({ i1, %record.StatusGrid }",
            "switch i1",
            "switch i32",
        }
    );
    assert_cli_run_success(
        executable,
        smoke_temp_root / "orison_cli_sourced_scalar_choice_maybe_record_maybe_nested_array_call_argument_run.or",
        sourced_scalar_choice_maybe_record_maybe_nested_array_call_argument_lines
    );
    assert_cli_emit_llvm_failure(
        executable,
        smoke_temp_root / "orison_cli_ambiguous_scalar_choice_for_bare_array_literal_binding_emit.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice LocalStatus",
            "    Ready(code: UInt32)",
            "    Empty",
            "choice RemoteStatus",
            "    Ready(code: UInt32)",
            "    Empty",
            "function main() -> UInt32",
            "    var total: UInt32 = 0 as UInt32",
            "    for status in [Ready(115 as UInt32), Empty]",
            "        switch status",
            "            Ready(code) =>",
            "                total = total + code",
            "            Empty =>",
            "                total = total + 0 as UInt32",
            "    total - 115 as UInt32",
        },
        "lowering array-literal for statements requires an explicit Array<T, N> source type"
    );
    assert_cli_emit_llvm_failure(
        executable,
        smoke_temp_root / "orison_cli_ambiguous_scalar_choice_maybe_for_bare_array_literal_binding_emit.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice LocalStatus",
            "    Ready(code: UInt32)",
            "    Empty",
            "choice RemoteStatus",
            "    Ready(code: UInt32)",
            "    Empty",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function main() -> UInt32",
            "    var total: UInt32 = 0 as UInt32",
            "    for maybe_status in [Some(Ready(117 as UInt32)), Empty]",
            "        switch maybe_status",
            "            Some(status) =>",
            "                switch status",
            "                    Ready(code) =>",
            "                        total = total + code",
            "                    Empty =>",
            "                        total = total + 0 as UInt32",
            "            Empty =>",
            "                total = total + 0 as UInt32",
            "    total - 117 as UInt32",
        },
        "lowering array-literal for statements requires an explicit Array<T, N> source type"
    );
    assert_cli_emit_llvm_failure(
        executable,
        smoke_temp_root / "orison_cli_ambiguous_scalar_choice_maybe_record_nested_array_for_bare_array_literal_binding_emit.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice LocalStatus",
            "    Ready(code: UInt32)",
            "    Empty",
            "choice RemoteStatus",
            "    Ready(code: UInt32)",
            "    Empty",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record StatusGrid",
            "    statuses: Array<Array<LocalStatus, 2>, 2>",
            "function main() -> UInt32",
            "    var total: UInt32 = 0 as UInt32",
            "    for maybe_grid in [Some(StatusGrid([[Empty, Ready(231 as UInt32)], [Empty, Empty]])), Empty]",
            "        switch maybe_grid",
            "            Some(grid) =>",
            "                switch grid.statuses[0 as UInt64][1 as UInt64]",
            "                    Ready(code) =>",
            "                        total = total + code",
            "                    Empty =>",
            "                        total = total + 0 as UInt32",
            "            Empty =>",
            "                total = total + 0 as UInt32",
            "    total - 231 as UInt32",
        },
        "lowering array-literal for statements requires an explicit Array<T, N> source type"
    );
    assert_cli_emit_llvm_failure(
        executable,
        smoke_temp_root / "orison_cli_ambiguous_scalar_choice_maybe_record_maybe_nested_array_for_bare_array_literal_binding_emit.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice LocalStatus",
            "    Ready(code: UInt32)",
            "    Empty",
            "choice RemoteStatus",
            "    Ready(code: UInt32)",
            "    Empty",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record StatusGrid",
            "    statuses: Array<Array<Maybe<LocalStatus>, 2>, 2>",
            "function main() -> UInt32",
            "    var total: UInt32 = 0 as UInt32",
            "    for maybe_grid in [Some(StatusGrid([[Empty, Some(Ready(233 as UInt32))], [Empty, Empty]])), Empty]",
            "        switch maybe_grid",
            "            Some(grid) =>",
            "                switch grid.statuses[0 as UInt64][1 as UInt64]",
            "                    Some(status) =>",
            "                        switch status",
            "                            Ready(code) =>",
            "                                total = total + code",
            "                            Empty =>",
            "                                total = total + 0 as UInt32",
            "                    Empty =>",
            "                        total = total + 0 as UInt32",
            "            Empty =>",
            "                total = total + 0 as UInt32",
            "    total - 233 as UInt32",
        },
        "lowering array-literal for statements requires an explicit Array<T, N> source type"
    );
    assert_cli_emit_llvm_failure(
        executable,
        smoke_temp_root / "orison_cli_multi_payload_choice_emit.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice PairStatus",
            "    Ready(left: UInt32, right: UInt32)",
            "    Empty",
            "function make_status() -> PairStatus",
            "    Ready(1 as UInt32, 2 as UInt32)",
        },
        "lowering does not yet support PairStatus as function return type: "
        "variants with multiple payloads do not yet have a lowered choice ABI"
    );
    assert_cli_emit_llvm_failure(
        executable,
        smoke_temp_root / "orison_cli_aggregate_payload_choice_emit.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Payload",
            "    code: UInt32",
            "choice PayloadStatus",
            "    Ready(payload: Payload)",
            "    Empty",
            "function make_status() -> PayloadStatus",
            "    Ready(Payload(7 as UInt32))",
        },
        "lowering does not yet support PayloadStatus as function return type: "
        "choice payload type 'Payload' does not yet have a scalar lowered choice ABI"
    );
    assert_cli_emit_llvm_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_choice_emit.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Boxed<T>",
            "    Wrap(value: T)",
            "    Empty",
            "function make_boxed() -> Boxed<UInt32>",
            "    Wrap(7 as UInt32)",
        },
        "lowering does not yet support Boxed<UInt32> as function return type: "
        "generic choices do not yet have a lowered choice ABI"
    );
    assert_cli_emit_llvm_success(
        executable,
        smoke_temp_root / "orison_cli_ambiguous_choice_layout_emit.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice LocalStatus",
            "    Ready(code: UInt32)",
            "    Empty",
            "choice RemoteStatus",
            "    Ready(code: UInt32)",
            "    Empty",
            "function make_status() -> LocalStatus",
            "    Ready(7 as UInt32)",
        },
        {
            "define { i32, i32 } @make_status()",
            "ret { i32, i32 }",
        }
    );

    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_unknown_choice_constant.or",
        maybe_choice_constant_lines("const DEFAULT_VALUE: Maybe<UInt32> = Missing(1)"),
        "choice constructor 'Missing' does not match any declared choice variant for constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_wrong_choice_constant.or",
        maybe_result_choice_constant_lines("const DEFAULT_VALUE: Maybe<UInt32> = Error(\"missing\")"),
        "choice constructor 'Error' does not belong to declared constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_constant_arity.or",
        status_choice_constant_lines("const DEFAULT_STATUS: Status = Ready()"),
        "choice constructor 'Ready' expects 1 payload value but received 0"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_zero_payload_choice_constant_arity.or",
        status_choice_constant_lines("const DEFAULT_STATUS: Status = Empty(1)"),
        "choice constructor 'Empty' expects 0 payload values but received 1"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_nested_zero_payload_choice_constant_arity.or",
        boxed_maybe_choice_constant_lines("const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Empty(1))"),
        "choice constructor 'Empty' expects 0 payload values but received 1"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_nested_wrong_choice_constant.or",
        boxed_maybe_result_choice_constant_lines(
            "const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Error(\"missing\"))"
        ),
        "choice constructor 'Error' does not belong to declared constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_nested_unknown_choice_constant.or",
        boxed_maybe_choice_constant_lines("const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Missing(1))"),
        "choice constructor 'Missing' does not match any declared choice variant for constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_constant_payload.or",
        status_choice_constant_lines("const DEFAULT_STATUS: Status = Ready(true)"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_constant_repeated_payload_type.or",
        maybe_choice_constant_lines_with_declarations(
            "const DEFAULT_VALUE: Both<Header> = Pair(Header([1, 2], 1), OtherHeader([1, 2], 1))",
            {
                "record Header",
                "    magic: Array<UInt32, 2>",
                "    version: UInt16",
                "record OtherHeader",
                "    magic: Array<UInt32, 2>",
                "    version: UInt16",
                "choice Both<T>",
                "    Pair(left: T, right: T)",
            }
        ),
        "choice constructor payload type 'OtherHeader' does not match expected payload type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_return_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    return Some(true)",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_final_expression_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    Some(true)",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}

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

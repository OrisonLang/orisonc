#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

auto lower_source(std::filesystem::path const& path, std::string_view source)
    -> orison::lowering::LlvmIrEmissionResult {
    {
        std::ofstream output(path);
        output << source;
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto semantic_result = analyzer.analyze(parse_result.module);
    assert(!semantic_result.has_errors());

    orison::lowering::LlvmIrEmitter emitter;
    return emitter.emit(parse_result.module, semantic_result);
}

void test_emit_constant_uint32_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_constant_uint32.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    1 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  ret i32 1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_let_bound_uint32_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_let_bound_uint32.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value = 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value = add i32 0, 1\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_uint32_add_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_uint32_add.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let left = 1 as UInt32\n"
        "    let right = 2 as UInt32\n"
        "    left + right\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %left = add i32 0, 1\n"
        "  %right = add i32 0, 2\n"
        "  %tmp0 = add i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_zero_argument_function_call_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_zero_argument_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function one() -> UInt32\n"
        "    1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    one()\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @one() {\n"
        "entry:\n"
        "  ret i32 1\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @one()\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_zero_argument_function_call_add_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_zero_argument_call_add.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function one() -> UInt32\n"
        "    1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    one() + 2 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @one() {\n"
        "entry:\n"
        "  ret i32 1\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @one()\n"
        "  %tmp1 = add i32 %tmp0, 2\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_single_uint32_parameter_function_call_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_single_uint32_parameter_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function increment(value: UInt32) -> UInt32\n"
        "    value + 1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    increment(41 as UInt32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @increment(i32 %value) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %value, 1\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @increment(i32 41)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_multi_uint32_parameter_function_call_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_multi_uint32_parameter_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function add(left: UInt32, right: UInt32) -> UInt32\n"
        "    left + right\n"
        "\n"
        "function main() -> UInt32\n"
        "    add(40 as UInt32, 2 as UInt32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @add(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @add(i32 40, i32 2)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_unsupported_return_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function flag() -> Bool\n"
        "    true\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(result.diagnostics.entries().front().message == "lowering does not yet support this return expression");
}

}  // namespace

auto main() -> int {
    test_emit_constant_uint32_return();
    test_emit_let_bound_uint32_return();
    test_emit_uint32_add_return();
    test_emit_zero_argument_function_call_return();
    test_emit_zero_argument_function_call_add_return();
    test_emit_single_uint32_parameter_function_call_return();
    test_emit_multi_uint32_parameter_function_call_return();
    test_reject_unsupported_return_expression();
    return 0;
}

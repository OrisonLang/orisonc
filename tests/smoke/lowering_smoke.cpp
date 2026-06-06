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

void test_emit_uint32_arithmetic_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_uint32_arithmetic.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let left = 8 as UInt32\n"
        "    let right = 2 as UInt32\n"
        "    left - right * right / right\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %left = add i32 0, 8\n"
        "  %right = add i32 0, 2\n"
        "  %tmp0 = mul i32 %right, %right\n"
        "  %tmp1 = udiv i32 %tmp0, %right\n"
        "  %tmp2 = sub i32 %left, %tmp1\n"
        "  ret i32 %tmp2\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_int32_division_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_int32_division.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function divide(left: Int32, right: Int32) -> Int32\n"
        "    left / right\n"
        "\n"
        "function main() -> Int32\n"
        "    divide(8 as Int32, 2 as Int32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @divide(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = sdiv i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @divide(i32 8, i32 2)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_uint32_comparison_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_uint32_comparisons.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function is_equal(left: UInt32, right: UInt32) -> Bool\n"
        "    left == right\n"
        "\n"
        "function is_different(left: UInt32, right: UInt32) -> Bool\n"
        "    left != right\n"
        "\n"
        "function is_less(left: UInt32, right: UInt32) -> Bool\n"
        "    left < right\n"
        "\n"
        "function is_at_most(left: UInt32, right: UInt32) -> Bool\n"
        "    left <= right\n"
        "\n"
        "function is_greater(left: UInt32, right: UInt32) -> Bool\n"
        "    left > right\n"
        "\n"
        "function is_at_least(left: UInt32, right: UInt32) -> Bool\n"
        "    left >= right\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i1 @is_equal(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp eq i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_different(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ne i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_less(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ult i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_most(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ule i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_greater(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ugt i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_least(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp uge i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_int32_comparison_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_int32_comparisons.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function is_less(left: Int32, right: Int32) -> Bool\n"
        "    left < right\n"
        "\n"
        "function is_at_most(left: Int32, right: Int32) -> Bool\n"
        "    left <= right\n"
        "\n"
        "function is_greater(left: Int32, right: Int32) -> Bool\n"
        "    left > right\n"
        "\n"
        "function is_at_least(left: Int32, right: Int32) -> Bool\n"
        "    left >= right\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i1 @is_less(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp slt i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_most(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp sle i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_greater(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp sgt i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_least(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp sge i32 %left, %right\n"
        "  ret i1 %tmp0\n"
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
    test_emit_uint32_arithmetic_return();
    test_emit_int32_division_return();
    test_emit_uint32_comparison_returns();
    test_emit_int32_comparison_returns();
    test_emit_zero_argument_function_call_return();
    test_emit_zero_argument_function_call_add_return();
    test_emit_single_uint32_parameter_function_call_return();
    test_emit_multi_uint32_parameter_function_call_return();
    test_reject_unsupported_return_expression();
    return 0;
}

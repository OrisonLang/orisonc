#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    auto path = std::filesystem::temp_directory_path() / "orison_expression_emitter_smoke.or";
    {
        auto output = std::ofstream(path);
        output << "package demo.expression_emitter\n"
                  "\n"
                  "record UartRegisters\n"
                  "    data: UInt32\n"
                  "    status: UInt32\n"
                  "\n"
                  "record Buffer\n"
                  "    bytes: Array<Byte, 4>\n"
                  "\n"
                  "record Log\n"
                  "    entries: Array<UartRegisters, 2>\n"
                  "\n"
                  "record Matrix\n"
                  "    rows: Array<Array<Byte, 4>, 2>\n"
                  "\n"
                  "record Device\n"
                  "    registers: UartRegisters\n"
                  "    buffer: Buffer\n"
                  "\n"
                  "function choose(flag: Bool, left: UInt32, right: UInt32) -> UInt32\n"
                  "    flag ? left + 1 as UInt32 : right + 2 as UInt32\n"
                  "\n"
                  "function sum_device_log(device: Device, log: Log) -> UInt32\n"
                  "    device.registers.status + log.entries[1 as UInt64].status\n"
                  "\n"
                  "extend UInt32\n"
                  "    function scale(this: shared This, amount: UInt32) -> UInt32\n"
                  "        this + amount\n"
                  "\n"
                  "function call_method(value: UInt32) -> UInt32\n"
                  "    value.scale(2 as UInt32)\n"
                  "\n"
                  "function bitwise_or(value: UInt32, mask: UInt32) -> UInt32\n"
                  "    value bit_or mask\n"
                  "\n"
                  "function bitwise_not(value: UInt32) -> UInt32\n"
                  "    bit_not value\n"
                  "\n"
                  "function shift_back(value: UInt32, amount: UInt32) -> UInt32\n"
                  "    value shift_right amount\n";
    }

    auto source = orison::source::SourceFile::read(path);
    assert(source.has_value());
    auto parser = orison::syntax::ModuleParser {};
    auto parse_result = parser.parse(*source);
    assert(!parse_result.diagnostics.has_errors());

    auto diagnostics = orison::diagnostics::DiagnosticBag {};
    auto lowering = orison::lowering::build_lowering_context(parse_result.module, diagnostics);
    assert(!diagnostics.has_errors());
    auto strings = orison::lowering::collect_string_constants(parse_result.module);
    auto context = orison::lowering::LoweringEmissionContext {
        .lowering = lowering,
        .string_constants = strings,
    };
    auto state = orison::lowering::FunctionLoweringState {};
    auto failures = orison::lowering::LoweringFailures {};
    auto session = orison::lowering::FunctionLoweringSession {
        .state = state,
        .failures = failures,
    };
    state.immutable_bindings.emplace("flag", orison::lowering::LoweredExpression {
        .type = "i1",
        .value = "%flag",
    });
    state.immutable_bindings.emplace("left", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%left",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    state.immutable_bindings.emplace("right", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%right",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });

    auto output = std::ostringstream {};
    auto lowered = orison::lowering::lower_expression(
        parse_result.module.functions.front().body_statements.front().expression,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    assert(lowered.has_value());
    assert(lowered->type == "i32");
    assert(lowered->value == "%tmp2");
    assert(state.current_block == "ternary.merge.0");
    assert(
        output.str() ==
        "  br i1 %flag, label %ternary.then.0, label %ternary.else.0\n"
        "ternary.then.0:\n"
        "  %tmp0 = add i32 %left, 1\n"
        "  br label %ternary.merge.0\n"
        "ternary.else.0:\n"
        "  %tmp1 = add i32 %right, 2\n"
        "  br label %ternary.merge.0\n"
        "ternary.merge.0:\n"
        "  %tmp2 = phi i32 [%tmp0, %ternary.then.0], [%tmp1, %ternary.else.0]\n"
    );

    auto unknown = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "missing",
    };
    output = {};
    auto failed = orison::lowering::lower_expression(
        unknown,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    assert(!failed.has_value());
    assert(
        failures.expression.reason ==
        orison::lowering::ExpressionLoweringFailureReason::unknown_name
    );
    assert(
        orison::lowering::render_expression_lowering_failure(failures.expression) ==
        "unknown lowered name: missing"
    );

    auto call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "choose",
            }
        ),
    };
    output = {};
    failed = orison::lowering::lower_expression(
        call,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    assert(!failed.has_value());
    assert(
        failures.expression.reason ==
        orison::lowering::ExpressionLoweringFailureReason::call_arity_mismatch
    );
    assert(
        orison::lowering::render_expression_lowering_failure(failures.expression) ==
        "call arity mismatch: choose expects 3 arguments, got 0"
    );

    auto member_state = orison::lowering::FunctionLoweringState {};
    member_state.local_name_counts["value"] = 1;
    member_state.immutable_bindings.emplace("value", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%value",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    member_state.source_type_names.emplace("value", "UInt32");
    auto member_failures = orison::lowering::LoweringFailures {};
    auto member_session = orison::lowering::FunctionLoweringSession {
        .state = member_state,
        .failures = member_failures,
    };
    output = {};
    auto member_lowered = orison::lowering::lower_expression(
        parse_result.module.functions[2].body_statements.front().expression,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        member_session,
        output
    );
    assert(member_lowered.has_value());
    assert(member_lowered->type == "i32");
    assert(member_lowered->value == "%tmp0");
    assert(
        output.str() ==
        "  %tmp0 = call i32 @method.UInt32.scale(i32 %value, i32 2)\n"
    );

    auto bitwise_state = orison::lowering::FunctionLoweringState {};
    bitwise_state.immutable_bindings.emplace("value", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%value",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    bitwise_state.immutable_bindings.emplace("mask", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%mask",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    bitwise_state.immutable_bindings.emplace("amount", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%amount",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    auto bitwise_failures = orison::lowering::LoweringFailures {};
    auto bitwise_session = orison::lowering::FunctionLoweringSession {
        .state = bitwise_state,
        .failures = bitwise_failures,
    };
    output = {};
    auto bitwise_or_lowered = orison::lowering::lower_expression(
        parse_result.module.functions[3].body_statements.front().expression,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        bitwise_session,
        output
    );
    assert(bitwise_or_lowered.has_value());
    assert(bitwise_or_lowered->value == "%tmp0");
    assert(output.str() == "  %tmp0 = or i32 %value, %mask\n");

    output = {};
    auto bitwise_not_lowered = orison::lowering::lower_expression(
        parse_result.module.functions[4].body_statements.front().expression,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        bitwise_session,
        output
    );
    assert(bitwise_not_lowered.has_value());
    assert(bitwise_not_lowered->value == "%tmp1");
    assert(output.str() == "  %tmp1 = xor i32 %value, -1\n");

    output = {};
    auto shift_back_lowered = orison::lowering::lower_expression(
        parse_result.module.functions[5].body_statements.front().expression,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        bitwise_session,
        output
    );
    assert(shift_back_lowered.has_value());
    assert(shift_back_lowered->value == "%tmp2");
    assert(output.str() == "  %tmp2 = lshr i32 %value, %amount\n");

    auto aggregate_binary_state = orison::lowering::FunctionLoweringState {};
    aggregate_binary_state.immutable_bindings.emplace("device", orison::lowering::LoweredExpression {
        .type = "%record.Device",
        .value = "%device",
        .signedness = orison::lowering::IntegerSignedness::not_integer,
    });
    aggregate_binary_state.immutable_bindings.emplace("log", orison::lowering::LoweredExpression {
        .type = "%record.Log",
        .value = "%log",
        .signedness = orison::lowering::IntegerSignedness::not_integer,
    });
    aggregate_binary_state.source_type_names.emplace("device", "Device");
    aggregate_binary_state.source_type_names.emplace("log", "Log");
    auto aggregate_binary_failures = orison::lowering::LoweringFailures {};
    auto aggregate_binary_session = orison::lowering::FunctionLoweringSession {
        .state = aggregate_binary_state,
        .failures = aggregate_binary_failures,
    };
    output = {};
    auto aggregate_binary_lowered = orison::lowering::lower_expression(
        parse_result.module.functions[1].body_statements.front().expression,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        aggregate_binary_session,
        output
    );
    assert(aggregate_binary_lowered.has_value());
    assert(aggregate_binary_lowered->type == "i32");
    assert(aggregate_binary_lowered->value == "%tmp5");
    assert(
        output.str() ==
        "  %tmp0 = extractvalue %record.Device %device, 0\n"
        "  %tmp1 = extractvalue %record.UartRegisters %tmp0, 1\n"
        "  %tmp2 = extractvalue %record.Log %log, 0\n"
        "  %tmp3 = extractvalue [2 x %record.UartRegisters] %tmp2, 1\n"
        "  %tmp4 = extractvalue %record.UartRegisters %tmp3, 1\n"
        "  %tmp5 = add i32 %tmp1, %tmp4\n"
    );

    auto missing_member_state = orison::lowering::FunctionLoweringState {};
    missing_member_state.immutable_bindings.emplace("value", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%value",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    auto missing_member_failures = orison::lowering::LoweringFailures {};
    auto missing_member_session = orison::lowering::FunctionLoweringSession {
        .state = missing_member_state,
        .failures = missing_member_failures,
    };
    output = {};
    auto missing_member_lowered = orison::lowering::lower_expression(
        parse_result.module.functions[2].body_statements.front().expression,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        missing_member_session,
        output
    );
    assert(!missing_member_lowered.has_value());
    assert(
        missing_member_session.failures.expression.reason ==
        orison::lowering::ExpressionLoweringFailureReason::unknown_member_call_receiver
    );
    assert(
        orison::lowering::render_expression_lowering_failure(
            missing_member_session.failures.expression
        ) == "unknown lowered member call receiver: value"
    );

    auto raw_state = orison::lowering::FunctionLoweringState {};
    raw_state.immutable_bindings.emplace("pointer", orison::lowering::LoweredExpression {
        .type = "ptr",
        .value = "%pointer",
    });
    raw_state.immutable_bindings.emplace("address", orison::lowering::LoweredExpression {
        .type = "i64",
        .value = "%address",
    });
    raw_state.immutable_bindings.emplace("index", orison::lowering::LoweredExpression {
        .type = "i64",
        .value = "%index",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    raw_state.immutable_bindings.emplace("inner", orison::lowering::LoweredExpression {
        .type = "i64",
        .value = "%inner",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    raw_state.immutable_bindings.emplace("value", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%value",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    raw_state.immutable_bindings.emplace("regs", orison::lowering::LoweredExpression {
        .type = "ptr",
        .value = "%regs",
    });
    raw_state.immutable_bindings.emplace("buffer", orison::lowering::LoweredExpression {
        .type = "ptr",
        .value = "%buffer",
    });
    raw_state.immutable_bindings.emplace("log", orison::lowering::LoweredExpression {
        .type = "ptr",
        .value = "%log",
    });
    raw_state.immutable_bindings.emplace("matrix", orison::lowering::LoweredExpression {
        .type = "ptr",
        .value = "%matrix",
    });
    raw_state.immutable_bindings.emplace("device", orison::lowering::LoweredExpression {
        .type = "ptr",
        .value = "%device",
    });
    raw_state.source_type_names.emplace("pointer", "Pointer<UInt32>");
    raw_state.source_type_names.emplace("address", "Address");
    raw_state.source_type_names.emplace("index", "UInt64");
    raw_state.source_type_names.emplace("inner", "UInt64");
    raw_state.source_type_names.emplace("value", "UInt32");
    raw_state.source_type_names.emplace("regs", "Pointer<UartRegisters>");
    raw_state.source_type_names.emplace("buffer", "Pointer<Buffer>");
    raw_state.source_type_names.emplace("log", "Pointer<Log>");
    raw_state.source_type_names.emplace("matrix", "Pointer<Matrix>");
    raw_state.source_type_names.emplace("device", "Pointer<Device>");
    auto raw_failures = orison::lowering::LoweringFailures {};
    auto raw_session = orison::lowering::FunctionLoweringSession {
        .state = raw_state,
        .failures = raw_failures,
    };

    auto raw_read_call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "raw_read",
            }
        ),
    };
    raw_read_call.arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "pointer",
    });
    output = {};
    auto raw_read = orison::lowering::lower_expression(
        raw_read_call,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        raw_session,
        output
    );
    assert(raw_read.has_value());
    assert(raw_read->value == "%tmp0");
    assert(output.str() == "  %tmp0 = load i32, ptr %pointer\n");

    auto raw_offset_call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "raw_offset",
            }
        ),
    };
    raw_offset_call.arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "pointer",
    });
    raw_offset_call.arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "index",
    });
    output = {};
    auto raw_offset = orison::lowering::lower_expression(
        raw_offset_call,
        "ptr",
        orison::lowering::IntegerSignedness::not_integer,
        context,
        raw_session,
        output
    );
    assert(raw_offset.has_value());
    assert(raw_offset->value == "%tmp1");
    assert(output.str() == "  %tmp1 = getelementptr i32, ptr %pointer, i64 %index\n");

    auto volatile_write_call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "volatile_write",
            }
        ),
    };
    volatile_write_call.arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "address",
    });
    volatile_write_call.arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "value",
    });
    output = {};
    auto volatile_write = orison::lowering::lower_expression(
        volatile_write_call,
        "void",
        orison::lowering::IntegerSignedness::not_integer,
        context,
        raw_session,
        output
    );
    assert(volatile_write.has_value());
    assert(
        output.str() ==
        "  %tmp2 = inttoptr i64 %address to ptr\n"
        "  store volatile i32 %value, ptr %tmp2\n"
    );

    auto field_address_call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "address_of",
            }
        ),
    };
    auto field_access = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::member_access,
        .text = "status",
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "regs",
            }
        ),
    };
    field_address_call.arguments.push_back(std::move(field_access));
    output = {};
    auto field_address = orison::lowering::lower_expression(
        field_address_call,
        "i64",
        orison::lowering::IntegerSignedness::not_integer,
        context,
        raw_session,
        output
    );
    assert(field_address.has_value());
    assert(field_address->value == "%tmp4");
    assert(
        output.str() ==
        "  %tmp3 = getelementptr %record.UartRegisters, ptr %regs, i32 0, i32 1\n"
        "  %tmp4 = ptrtoint ptr %tmp3 to i64\n"
    );

    auto nested_field_address_call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "address_of",
            }
        ),
    };
    auto nested_status_access = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::member_access,
        .text = "status",
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::member_access,
                .text = "registers",
                .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                    orison::syntax::ExpressionSyntax {
                        .kind = orison::syntax::ExpressionKind::name,
                        .text = "device",
                    }
                ),
            }
        ),
    };
    nested_field_address_call.arguments.push_back(std::move(nested_status_access));
    output = {};
    auto nested_field_address = orison::lowering::lower_expression(
        nested_field_address_call,
        "i64",
        orison::lowering::IntegerSignedness::not_integer,
        context,
        raw_session,
        output
    );
    assert(nested_field_address.has_value());
    assert(nested_field_address->value == "%tmp7");
    assert(
        output.str() ==
        "  %tmp5 = getelementptr %record.Device, ptr %device, i32 0, i32 0\n"
        "  %tmp6 = getelementptr %record.UartRegisters, ptr %tmp5, i32 0, i32 1\n"
        "  %tmp7 = ptrtoint ptr %tmp6 to i64\n"
    );

    auto array_field_address_call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "address_of",
            }
        ),
    };
    auto array_field_access = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::index_access,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::member_access,
                .text = "bytes",
                .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                    orison::syntax::ExpressionSyntax {
                        .kind = orison::syntax::ExpressionKind::name,
                        .text = "buffer",
                    }
                ),
            }
        ),
    };
    array_field_access.arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "index",
    });
    array_field_address_call.arguments.push_back(std::move(array_field_access));
    output = {};
    auto array_field_address = orison::lowering::lower_expression(
        array_field_address_call,
        "i64",
        orison::lowering::IntegerSignedness::not_integer,
        context,
        raw_session,
        output
    );
    assert(array_field_address.has_value());
    assert(array_field_address->value == "%tmp10");
    assert(
        output.str() ==
        "  %tmp8 = getelementptr %record.Buffer, ptr %buffer, i32 0, i32 0\n"
        "  %tmp9 = getelementptr [4 x i8], ptr %tmp8, i64 0, i64 %index\n"
        "  %tmp10 = ptrtoint ptr %tmp9 to i64\n"
    );

    auto nested_array_address_call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "address_of",
            }
        ),
    };
    auto nested_array_access = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::index_access,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::member_access,
                .text = "bytes",
                .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                    orison::syntax::ExpressionSyntax {
                        .kind = orison::syntax::ExpressionKind::member_access,
                        .text = "buffer",
                        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                            orison::syntax::ExpressionSyntax {
                                .kind = orison::syntax::ExpressionKind::name,
                                .text = "device",
                            }
                        ),
                    }
                ),
            }
        ),
    };
    nested_array_access.arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "index",
    });
    nested_array_address_call.arguments.push_back(std::move(nested_array_access));
    output = {};
    auto nested_array_address = orison::lowering::lower_expression(
        nested_array_address_call,
        "i64",
        orison::lowering::IntegerSignedness::not_integer,
        context,
        raw_session,
        output
    );
    assert(nested_array_address.has_value());
    assert(nested_array_address->value == "%tmp14");
    assert(
        output.str() ==
        "  %tmp11 = getelementptr %record.Device, ptr %device, i32 0, i32 1\n"
        "  %tmp12 = getelementptr %record.Buffer, ptr %tmp11, i32 0, i32 0\n"
        "  %tmp13 = getelementptr [4 x i8], ptr %tmp12, i64 0, i64 %index\n"
        "  %tmp14 = ptrtoint ptr %tmp13 to i64\n"
    );

    auto array_of_records_address_call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "address_of",
            }
        ),
    };
    auto array_of_records_access = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::member_access,
        .text = "status",
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::index_access,
                .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                    orison::syntax::ExpressionSyntax {
                        .kind = orison::syntax::ExpressionKind::member_access,
                        .text = "entries",
                        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                            orison::syntax::ExpressionSyntax {
                                .kind = orison::syntax::ExpressionKind::name,
                                .text = "log",
                            }
                        ),
                    }
                ),
            }
        ),
    };
    array_of_records_access.left->arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "index",
    });
    array_of_records_address_call.arguments.push_back(std::move(array_of_records_access));
    output = {};
    auto array_of_records_address = orison::lowering::lower_expression(
        array_of_records_address_call,
        "i64",
        orison::lowering::IntegerSignedness::not_integer,
        context,
        raw_session,
        output
    );
    assert(array_of_records_address.has_value());
    assert(array_of_records_address->value == "%tmp18");
    assert(
        output.str() ==
        "  %tmp15 = getelementptr %record.Log, ptr %log, i32 0, i32 0\n"
        "  %tmp16 = getelementptr [2 x %record.UartRegisters], ptr %tmp15, i64 0, i64 %index\n"
        "  %tmp17 = getelementptr %record.UartRegisters, ptr %tmp16, i32 0, i32 1\n"
        "  %tmp18 = ptrtoint ptr %tmp17 to i64\n"
    );

    auto nested_indices_address_call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "address_of",
            }
        ),
    };
    auto nested_indices_access = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::index_access,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::index_access,
                .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                    orison::syntax::ExpressionSyntax {
                        .kind = orison::syntax::ExpressionKind::member_access,
                        .text = "rows",
                        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                            orison::syntax::ExpressionSyntax {
                                .kind = orison::syntax::ExpressionKind::name,
                                .text = "matrix",
                            }
                        ),
                    }
                ),
            }
        ),
    };
    nested_indices_access.left->arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "index",
    });
    nested_indices_access.arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "inner",
    });
    nested_indices_address_call.arguments.push_back(std::move(nested_indices_access));
    output = {};
    auto nested_indices_address = orison::lowering::lower_expression(
        nested_indices_address_call,
        "i64",
        orison::lowering::IntegerSignedness::not_integer,
        context,
        raw_session,
        output
    );
    assert(nested_indices_address.has_value());
    assert(nested_indices_address->value == "%tmp22");
    assert(
        output.str() ==
        "  %tmp19 = getelementptr %record.Matrix, ptr %matrix, i32 0, i32 0\n"
        "  %tmp20 = getelementptr [2 x [4 x i8]], ptr %tmp19, i64 0, i64 %index\n"
        "  %tmp21 = getelementptr [4 x i8], ptr %tmp20, i64 0, i64 %inner\n"
        "  %tmp22 = ptrtoint ptr %tmp21 to i64\n"
    );

    std::filesystem::remove(path);
    return 0;
}

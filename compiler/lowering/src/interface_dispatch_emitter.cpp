#include "orison/lowering/interface_dispatch_emitter.hpp"

#include <cstddef>
#include <sstream>

namespace orison::lowering {
namespace {

auto can_emit_interface_dispatch_slot(LoweredInterfaceDispatchSlot const& slot) -> bool {
    auto const& concrete = slot.concrete_method_signature;
    auto const& thunk = slot.thunk_signature;
    if (slot.thunk_symbol_name.empty() || concrete.symbol_name.empty() ||
        concrete.return_type.empty() || concrete.parameter_types.empty() ||
        thunk.return_type != concrete.return_type || thunk.parameter_types.size() != concrete.parameter_types.size() ||
        thunk.parameter_types.empty() || thunk.parameter_types.front() != "ptr") {
        return false;
    }
    for (auto index = std::size_t {1}; index < concrete.parameter_types.size(); ++index) {
        if (thunk.parameter_types[index] != concrete.parameter_types[index]) {
            return false;
        }
    }
    return slot.receiver_access == LoweredInterfaceReceiverAccess::shared ||
        concrete.parameter_types.front() == "ptr";
}

void emit_thunk_parameters(
    std::ostringstream& output,
    LoweredFunctionSignature const& signature
) {
    for (auto index = std::size_t {0}; index < signature.parameter_types.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << signature.parameter_types[index] << " "
               << (index == 0 ? "%erased_receiver" : "%arg" + std::to_string(index));
    }
}

void emit_thunk_arguments(
    std::ostringstream& output,
    LoweredFunctionSignature const& signature,
    bool pointer_native_receiver
) {
    output << signature.parameter_types.front() << " "
           << (pointer_native_receiver ? "%erased_receiver" : "%concrete_receiver");
    for (auto index = std::size_t {1}; index < signature.parameter_types.size(); ++index) {
        output << ", " << signature.parameter_types[index] << " %arg" << index;
    }
}

void emit_interface_dispatch_thunk(
    std::ostringstream& output,
    LoweredInterfaceDispatchSlot const& slot
) {
    auto const& concrete = slot.concrete_method_signature;
    auto const& thunk = slot.thunk_signature;
    auto const pointer_native_receiver = concrete.parameter_types.front() == "ptr";

    output << "define internal " << thunk.return_type << " @" << slot.thunk_symbol_name << "(";
    emit_thunk_parameters(output, thunk);
    output << ") {\nentry:\n";
    if (!pointer_native_receiver) {
        output << "  %concrete_receiver = load " << concrete.parameter_types.front()
               << ", ptr %erased_receiver\n";
    }
    output << "  ";
    if (concrete.return_type != "void") {
        output << "%result = ";
    }
    output << "call " << concrete.return_type << " @" << concrete.symbol_name << "(";
    emit_thunk_arguments(output, concrete, pointer_native_receiver);
    output << ")\n";
    if (concrete.return_type == "void") {
        output << "  ret void\n";
    } else {
        output << "  ret " << concrete.return_type << " %result\n";
    }
    output << "}\n\n";
}

}  // namespace

auto can_emit_interface_dispatch_table(LoweredInterfaceDispatchTable const& table) -> bool {
    if (table.symbol_name.empty() || table.slots.empty()) {
        return false;
    }
    for (auto const& slot : table.slots) {
        if (!can_emit_interface_dispatch_slot(slot)) {
            return false;
        }
    }
    return true;
}

auto emit_interface_dispatch_definitions(LoweringContext const& context) -> std::string {
    auto output = std::ostringstream {};
    for (auto const& table : context.interface_dispatch_tables) {
        if (!can_emit_interface_dispatch_table(table)) {
            continue;
        }
        output << "@" << table.symbol_name << " = private constant [" << table.slots.size()
               << " x ptr] [";
        for (auto index = std::size_t {0}; index < table.slots.size(); ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << "ptr @" << table.slots[index].thunk_symbol_name;
        }
        output << "]\n\n";
        for (auto const& slot : table.slots) {
            emit_interface_dispatch_thunk(output, slot);
        }
    }
    return output.str();
}

auto emit_borrowed_interface_existential(
    LoweredInterfaceExistentialAbi const& abi,
    LoweredInterfaceDispatchTable const& table,
    std::string_view concrete_storage_pointer,
    std::string_view result_name
) -> std::string {
    if (!can_emit_interface_dispatch_table(table) || concrete_storage_pointer.empty() || result_name.empty()) {
        return {};
    }

    auto output = std::ostringstream {};
    output << "  %" << result_name << ".storage = insertvalue " << abi.llvm_type
           << " poison, ptr " << concrete_storage_pointer << ", "
           << abi.concrete_storage_pointer_index << "\n";
    output << "  %" << result_name << " = insertvalue " << abi.llvm_type << " %"
           << result_name << ".storage, ptr @" << table.symbol_name << ", "
           << abi.dispatch_table_pointer_index << "\n";
    return output.str();
}

auto emit_borrowed_interface_dispatch_call(
    LoweredInterfaceExistentialAbi const& abi,
    LoweredInterfaceDispatchTable const& table,
    std::size_t slot_index,
    std::string_view existential_value,
    std::vector<LoweredExpression> const& arguments,
    std::string_view instruction_stem
) -> std::string {
    if (!can_emit_interface_dispatch_table(table) || existential_value.empty() || instruction_stem.empty() ||
        slot_index >= table.slots.size()) {
        return {};
    }
    auto const& slot = table.slots[slot_index];
    if (slot.index != slot_index || slot.thunk_signature.parameter_types.size() != arguments.size() + 1) {
        return {};
    }
    for (auto index = std::size_t {0}; index < arguments.size(); ++index) {
        if (arguments[index].type != slot.thunk_signature.parameter_types[index + 1] ||
            arguments[index].value.empty()) {
            return {};
        }
    }

    auto output = std::ostringstream {};
    output << "  %" << instruction_stem << ".receiver = extractvalue " << abi.llvm_type << " "
           << existential_value << ", " << abi.concrete_storage_pointer_index << "\n";
    output << "  %" << instruction_stem << ".table = extractvalue " << abi.llvm_type << " "
           << existential_value << ", " << abi.dispatch_table_pointer_index << "\n";
    output << "  %" << instruction_stem << ".slot = getelementptr inbounds [" << table.slots.size()
           << " x ptr], ptr %" << instruction_stem << ".table, i64 0, i64 " << slot.index << "\n";
    output << "  %" << instruction_stem << ".method = load ptr, ptr %" << instruction_stem << ".slot\n";
    output << "  ";
    if (slot.thunk_signature.return_type != "void") {
        output << "%" << instruction_stem << ".result = ";
    }
    output << "call " << slot.thunk_signature.return_type << " %" << instruction_stem
           << ".method(ptr %" << instruction_stem << ".receiver";
    for (auto const& argument : arguments) {
        output << ", " << argument.type << " " << argument.value;
    }
    output << ")\n";
    return output.str();
}

}  // namespace orison::lowering

#include "orison/lowering/llvm_object_emitter.hpp"

#include "orison/lowering/llvm_ir_verifier.hpp"

#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <memory>
#include <optional>
#include <string>

namespace orison::lowering {
namespace {

auto initialize_native_codegen() -> bool {
    static auto const initialized = !llvm::InitializeNativeTarget() && !llvm::InitializeNativeTargetAsmPrinter();
    return initialized;
}

}  // namespace

auto LlvmObjectEmissionResult::has_errors() const -> bool {
    return diagnostics.has_errors();
}

auto LlvmObjectEmitter::emit(std::string_view ir_text) const -> LlvmObjectEmissionResult {
    auto result = LlvmObjectEmissionResult {};
    LlvmIrVerifier verifier;
    result.diagnostics = verifier.verify(ir_text);
    if (result.has_errors()) {
        return result;
    }
    if (!initialize_native_codegen()) {
        result.diagnostics.error(1, "LLVM native target initialization failed");
        return result;
    }

    auto context = llvm::LLVMContext {};
    auto parser_diagnostic = llvm::SMDiagnostic {};
    auto module = llvm::parseAssemblyString(
        llvm::StringRef(ir_text.data(), ir_text.size()),
        parser_diagnostic,
        context
    );
    if (!module) {
        result.diagnostics.error(
            parser_diagnostic.getLineNo(),
            "verified LLVM IR could not be reparsed for object emission: " + parser_diagnostic.getMessage().str()
        );
        return result;
    }

    auto triple = llvm::Triple(llvm::sys::getDefaultTargetTriple());
    auto target_error = std::string {};
    auto const* target = llvm::TargetRegistry::lookupTarget(triple, target_error);
    if (target == nullptr) {
        result.diagnostics.error(1, "LLVM target lookup failed for '" + triple.str() + "': " + target_error);
        return result;
    }

    auto options = llvm::TargetOptions {};
    auto target_machine = std::unique_ptr<llvm::TargetMachine>(
        target->createTargetMachine(triple, "generic", "", options, llvm::Reloc::PIC_)
    );
    if (!target_machine) {
        result.diagnostics.error(1, "LLVM could not create a target machine for '" + triple.str() + "'");
        return result;
    }

    module->setTargetTriple(triple);
    module->setDataLayout(target_machine->createDataLayout());

    auto object_buffer = llvm::SmallVector<char, 0> {};
    auto object_stream = llvm::raw_svector_ostream(object_buffer);
    auto pass_manager = llvm::legacy::PassManager {};
    if (target_machine->addPassesToEmitFile(
            pass_manager,
            object_stream,
            nullptr,
            llvm::CodeGenFileType::ObjectFile,
            false
        )) {
        result.diagnostics.error(1, "LLVM target does not support object-file emission");
        return result;
    }

    pass_manager.run(*module);
    result.object_bytes.assign(object_buffer.begin(), object_buffer.end());
    if (result.object_bytes.empty()) {
        result.diagnostics.error(1, "LLVM object emission produced no output");
    }
    return result;
}

}  // namespace orison::lowering

#include "orison/lowering/llvm_ir_verifier.hpp"

#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <string>

namespace orison::lowering {

auto LlvmIrVerifier::verify(std::string_view ir_text) const -> diagnostics::DiagnosticBag {
    auto diagnostics = diagnostics::DiagnosticBag {};
    auto context = llvm::LLVMContext {};
    auto parser_diagnostic = llvm::SMDiagnostic {};
    auto module = llvm::parseAssemblyString(
        llvm::StringRef(ir_text.data(), ir_text.size()),
        parser_diagnostic,
        context
    );
    if (!module) {
        diagnostics.error(
            parser_diagnostic.getLineNo(),
            "generated LLVM IR failed to parse: " + parser_diagnostic.getMessage().str()
        );
        return diagnostics;
    }

    auto verifier_message = std::string {};
    auto verifier_stream = llvm::raw_string_ostream(verifier_message);
    if (llvm::verifyModule(*module, &verifier_stream)) {
        verifier_stream.flush();
        diagnostics.error(1, "generated LLVM IR failed verification: " + verifier_message);
    }
    return diagnostics;
}

}  // namespace orison::lowering

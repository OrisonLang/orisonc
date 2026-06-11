#include "orison/pipeline/compile_pipeline.hpp"

#include <cassert>
#include <filesystem>
#include <string>

auto main() -> int {
    orison::pipeline::CompilePipeline pipeline;
    auto source_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "minimal.or";

    auto analysis = pipeline.analyze(source_path);
    assert(!analysis.has_errors());
    assert(analysis.source_file.has_value());
    assert(analysis.parse_result.module.package_name == "demo.minimal");
    assert(analysis.parse_result.module.functions.size() == 1);

    auto ir = pipeline.emit_llvm(source_path);
    assert(!ir.has_errors());
    assert(ir.ir_text.find("define i32 @main()") != std::string::npos);
    assert(ir.ir_text.find("ret i32 0") != std::string::npos);

    auto object = pipeline.emit_object(source_path);
    assert(!object.has_errors());
    assert(!object.object_bytes.empty());

    auto missing = pipeline.analyze(source_path.parent_path() / "missing.or");
    assert(missing.has_errors());
    assert(missing.error_text == "error: unable to read source file\n");
    return 0;
}

add_library(orison_syntax STATIC
    src/lexer.cpp
    src/module_parser.cpp
)

target_include_directories(orison_syntax
    PUBLIC
        include
)

target_link_libraries(orison_syntax
    PUBLIC
        orison_diagnostics
        orison_source
)

target_compile_features(orison_syntax PUBLIC cxx_std_23)

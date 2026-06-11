find_program(ORISON_HOST_LINK_DRIVER NAMES clang cc REQUIRED)

add_library(orison_link STATIC
    src/host_linker.cpp
)

target_include_directories(orison_link
    PUBLIC
        include
)

target_link_libraries(orison_link
    PUBLIC
        orison_diagnostics
)

target_compile_definitions(orison_link
    PRIVATE
        ORISON_HOST_LINK_DRIVER="${ORISON_HOST_LINK_DRIVER}"
)

target_compile_features(orison_link PUBLIC cxx_std_23)

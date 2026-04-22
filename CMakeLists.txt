cmake_minimum_required(VERSION 3.28)

project(
    Orison
    VERSION 0.1.0
    DESCRIPTION "Orison systems programming language toolchain"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ORISON_ENABLE_TESTS "Build Orison test targets" ON)

add_subdirectory(compiler)
add_subdirectory(tools)

if(ORISON_ENABLE_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

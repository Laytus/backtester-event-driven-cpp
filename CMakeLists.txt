cmake_minimum_required(VERSION 3.28)

project(
    backtester_event_driven_cpp
    VERSION 0.1.0
    DESCRIPTION "A learning-focused event-driven backtester"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BACKTESTER_BUILD_TESTS "Build the C++ test suite" ON)

add_library(backtester_core)
add_library(backtester::core ALIAS backtester_core)

target_sources(
    backtester_core
    PRIVATE
        src/version.cpp
        src/market/bar.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS 
            include
        FILES
            include/backtester/version.hpp
            include/backtester/market/bar.hpp
)

target_compile_features(
    backtester_core
    PUBLIC
    cxx_std_23
)

set_target_properties(
    backtester_core
    PROPERTIES
        CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(
        backtester_core
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        backtester_core
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
    )
endif()

add_executable(
    backtester
    app/main.cpp
)

target_link_libraries(
    backtester
    PRIVATE
        backtester::core
)

target_compile_features(
    backtester
    PRIVATE
        cxx_std_23
)

set_target_properties(
    backtester
    PROPERTIES
        CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(
        backtester
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        backtester
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
    )
endif()

if(BACKTESTER_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests/cpp)
endif()


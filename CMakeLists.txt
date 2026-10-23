cmake_minimum_required(VERSION 3.28)

project(
    backtester_event_driven_cpp
    VERSION 0.1.0
    DESCRIPTION "A deterministic event-driven backtesting engine in modern C++"
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
        src/statistics/statistics.cpp
        src/market/bar.cpp
        src/data/csv_bar_reader.cpp
        src/data/historical_data_feed.cpp
        src/events/event.cpp
        src/events/event_queue.cpp
        src/engine/backtest_engine.cpp
        src/strategy/buy_and_hold_strategy.cpp
        src/portfolio/portfolio.cpp
        src/portfolio/position_sizer.cpp
        src/portfolio/order_generator.cpp
        src/execution/execution_handler.cpp
        src/indicators/simple_moving_average.cpp
        src/indicators/exponential_moving_average.cpp
        src/indicators/momentum.cpp
        src/indicators/macd.cpp
        src/indicators/relative_strength_index.cpp
        src/indicators/rolling_min_max.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS 
            include
        FILES
            include/backtester/version.hpp
            include/backtester/market/bar.hpp
            include/backtester/data/csv_bar_reader.hpp
            include/backtester/data/historical_data_feed.hpp
            include/backtester/events/event.hpp
            include/backtester/events/event_queue.hpp
            include/backtester/engine/backtest_engine.hpp
            include/backtester/strategy/strategy.hpp
            include/backtester/strategy/buy_and_hold_strategy.hpp
            include/backtester/portfolio/portfolio.hpp
            include/backtester/portfolio/position_sizer.hpp
            include/backtester/portfolio/order_generator.hpp
            include/backtester/execution/execution_handler.hpp
            include/backtester/indicators/simple_moving_average.hpp
            include/backtester/indicators/exponential_moving_average.hpp
            include/backtester/indicators/momentum.hpp
            include/backtester/indicators/macd.hpp
            include/backtester/indicators/relative_strength_index.hpp
            include/backtester/indicators/rolling_min_max.hpp
            include/backtester/statistics/statistics.hpp
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


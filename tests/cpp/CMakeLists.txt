include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Catch2)

add_executable(
    backtester_tests
    test_version.cpp
    test_bar.cpp
    test_csv_bar_reader.cpp
    test_historical_data_feed.cpp
    test_csv_historical_data_feed.cpp
    test_event.cpp
    test_event_queue.cpp
    test_backtest_engine.cpp
    test_phase_2_integration.cpp
    test_buy_and_hold_strategy.cpp
    test_portfolio.cpp
    test_position_sizing.cpp
)

target_link_libraries(
    backtester_tests
    PRIVATE
        backtester::core
        Catch2::Catch2WithMain
)

target_compile_features(
    backtester_tests
    PRIVATE
        cxx_std_23
)

target_compile_definitions(
    backtester_tests
    PRIVATE
        BACKTESTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

include(Catch)

catch_discover_tests(backtester_tests)


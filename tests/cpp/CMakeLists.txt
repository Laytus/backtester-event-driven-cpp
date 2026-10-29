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
    test_execution_handler.cpp
    test_phase_3_engine_integration.cpp
    test_phase_3_end-to-end.cpp
    test_simple_moving_average.cpp
    test_exponential_moving_average.cpp
    test_momentum.cpp
    test_macd.cpp
    test_relative_strength_index.cpp
    test_rolling_min_max.cpp
    test_statistics.cpp
    test_bollinger_bands.cpp
    test_indicators_integration.cpp
    test_sma_crossover_strategy.cpp
    test_ema_crossover_strategy.cpp
    test_price_filter_strategy.cpp
    test_channel_breakout_strategy.cpp
    test_momentum_strategy.cpp
    test_macd_crossover_strategy.cpp
    test_rsi_based_strategy.cpp
    test_return_series.cpp
    test_performance_metrics.cpp
    test_active_return_series.cpp
    test_relative_metrics.cpp
    test_benchmark_comparison.cpp
    test_strategy_performance_integration.cpp
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


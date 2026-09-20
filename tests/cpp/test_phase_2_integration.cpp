#include "backtester/data/csv_bar_reader.hpp"
#include "backtester/data/historical_data_feed.hpp"
#include "backtester/engine/backtest_engine.hpp"
#include "backtester/events/event.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <optional>
#include <variant>
#include <vector>

namespace {

using backtester::data::CsvBarReader;
using backtester::data::HistoricalDataFeed;
using backtester::engine::BacktestEngine;
using backtester::events::Event;
using backtester::events::MarketEvent;
using backtester::market::Bar;

std::filesystem::path fixture_path(
    const std::string& filename
) {
    return std::filesystem::path{BACKTESTER_SOURCE_DIR}
        / "data"
        / "fixtures"
        / filename;
}

const MarketEvent& require_market_event(
    const std::optional<Event>& event
) {
    REQUIRE(event.has_value());
    REQUIRE(std::holds_alternative<MarketEvent>(*event));

    return std::get<MarketEvent>(*event);
}

void require_same_bar(
    const Bar& actual,
    const Bar& expected
) {
    REQUIRE(actual.session_date() == expected.session_date());
    REQUIRE(actual.symbol() == expected.symbol());
    REQUIRE(actual.open() == expected.open());
    REQUIRE(actual.high() == expected.high());
    REQUIRE(actual.low() == expected.low());
    REQUIRE(actual.close() == expected.close());
    REQUIRE(actual.volume() == expected.volume());
}

std::vector<Bar> run_engine(
    const std::vector<Bar>& bars
) {
    BacktestEngine engine{
        HistoricalDataFeed{
            bars
        }
    };

    std::vector<Bar> observed_bars;

    while (const auto event = engine.step()) {
        const auto& market_event =
            require_market_event(event);

        observed_bars.push_back(
            market_event.bar()
        );
    }

    REQUIRE(engine.finished());

    return observed_bars;
}

}  // namespace

TEST_CASE(
    "Canonical CSV flows through the Phase 2 engine without modification"
) {
    const auto expected_bars =
        CsvBarReader::read(
            fixture_path("daily_bars_valid.csv")
        );

    REQUIRE_FALSE(expected_bars.empty());

    BacktestEngine engine{
        HistoricalDataFeed{
            expected_bars
        }
    };

    for (const auto& expected_bar : expected_bars) {
        REQUIRE_FALSE(engine.finished());

        const auto event = engine.step();

        const auto& market_event =
            require_market_event(event);

        require_same_bar(
            market_event.bar(),
            expected_bar
        );
    }

    // The final MarketEvent has been returned, but the engine has not
    // yet observed that the feed is exhausted.
    REQUIRE_FALSE(engine.finished());

    const auto exhausted = engine.step();

    REQUIRE_FALSE(exhausted.has_value());
    REQUIRE(engine.finished());

    // Completion remains stable.
    REQUIRE_FALSE(engine.step().has_value());
    REQUIRE_FALSE(engine.step().has_value());
    REQUIRE(engine.finished());
}

TEST_CASE(
    "Phase 2 engine produces deterministic repeated runs from canonical CSV"
) {
    const auto canonical_bars =
        CsvBarReader::read(
            fixture_path("daily_bars_valid.csv")
        );

    REQUIRE_FALSE(canonical_bars.empty());

    const auto first_run =
        run_engine(canonical_bars);

    const auto second_run =
        run_engine(canonical_bars);

    REQUIRE(first_run.size() == canonical_bars.size());
    REQUIRE(second_run.size() == canonical_bars.size());
    REQUIRE(first_run.size() == second_run.size());

    for (std::size_t i = 0; i < first_run.size(); ++i) {
        require_same_bar(
            first_run[i],
            canonical_bars[i]
        );

        require_same_bar(
            second_run[i],
            canonical_bars[i]
        );

        require_same_bar(
            first_run[i],
            second_run[i]
        );
    }
}
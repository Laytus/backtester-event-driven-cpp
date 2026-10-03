#include "backtester/data/historical_data_feed.hpp"
#include "backtester/engine/backtest_engine.hpp"
#include "backtester/events/event.hpp"
#include "backtester/market/bar.hpp"
#include "backtester/portfolio/portfolio.hpp"
#include "backtester/portfolio/position_sizer.hpp"
#include "backtester/strategy/strategy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

namespace {

using backtester::data::HistoricalDataFeed;
using backtester::engine::BacktestEngine;
using backtester::events::Event;
using backtester::events::MarketEvent;
using backtester::events::SignalEvent;
using backtester::market::Bar;
using backtester::market::SessionDate;
using backtester::portfolio::Portfolio;
using backtester::portfolio::PositionSizer;
using backtester::strategy::Strategy;

class SilentStrategy final : public Strategy {
public:
    [[nodiscard]]
    std::vector<SignalEvent>
    on_market(const MarketEvent&) override {
        return {};
    }
};

const SessionDate first_date{
    std::chrono::year{2026},
    std::chrono::month{9},
    std::chrono::day{15}
};

const SessionDate second_date{
    std::chrono::year{2026},
    std::chrono::month{9},
    std::chrono::day{16}
};

const SessionDate third_date{
    std::chrono::year{2026},
    std::chrono::month{9},
    std::chrono::day{17}
};

std::vector<Bar> make_bars() {
    std::vector<Bar> bars;

    bars.emplace_back(
        first_date,
        "AAPL",
        100.0,
        105.0,
        99.0,
        103.0,
        1'000
    );

    bars.emplace_back(
        second_date,
        "AAPL",
        103.0,
        108.0,
        102.0,
        107.0,
        1'200
    );

    bars.emplace_back(
        third_date,
        "AAPL",
        107.0,
        110.0,
        106.0,
        109.0,
        1'500
    );

    return bars;
}

BacktestEngine make_engine() {
    return BacktestEngine{
        HistoricalDataFeed{
            make_bars()
        },
        std::make_unique<SilentStrategy>(),
        Portfolio{
            "AAPL",
            10'000.0
        },
        PositionSizer{
            1.0
        }
    };
}

const MarketEvent& require_market_event(
    const std::optional<Event>& event
) {
    REQUIRE(event.has_value());
    REQUIRE(std::holds_alternative<MarketEvent>(*event));

    return std::get<MarketEvent>(*event);
}

static_assert(!std::is_copy_constructible_v<BacktestEngine>);
static_assert(!std::is_copy_assignable_v<BacktestEngine>);

static_assert(std::is_move_constructible_v<BacktestEngine>);
static_assert(std::is_move_assignable_v<BacktestEngine>);

}  // namespace

TEST_CASE("BacktestEngine starts unfinished") {
    auto engine = make_engine();

    REQUIRE_FALSE(engine.finished());
}

TEST_CASE("BacktestEngine handles an empty historical feed") {
    BacktestEngine engine{
        HistoricalDataFeed{
            std::vector<Bar>{}
        },
        std::make_unique<SilentStrategy>(),
        Portfolio{
            "AAPL",
            10'000.0
        },
        PositionSizer{
            1.0
        }
    };

    REQUIRE_FALSE(engine.finished());

    const auto event = engine.step();

    REQUIRE_FALSE(event.has_value());
    REQUIRE(engine.finished());
}

TEST_CASE("BacktestEngine creates one MarketEvent per delivered Bar") {
    auto engine = make_engine();

    const auto first = engine.step();
    const auto second = engine.step();
    const auto third = engine.step();

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(third.has_value());

    REQUIRE(std::holds_alternative<MarketEvent>(*first));
    REQUIRE(std::holds_alternative<MarketEvent>(*second));
    REQUIRE(std::holds_alternative<MarketEvent>(*third));

    const auto exhausted = engine.step();

    REQUIRE_FALSE(exhausted.has_value());
    REQUIRE(engine.finished());
}

TEST_CASE("BacktestEngine preserves historical Bar order") {
    auto engine = make_engine();

    const auto first = engine.step();
    const auto second = engine.step();
    const auto third = engine.step();

    const auto& first_market =
        require_market_event(first);

    const auto& second_market =
        require_market_event(second);

    const auto& third_market =
        require_market_event(third);

    REQUIRE(
        first_market.bar().session_date()
        == first_date
    );

    REQUIRE(
        second_market.bar().session_date()
        == second_date
    );

    REQUIRE(
        third_market.bar().session_date()
        == third_date
    );
}

TEST_CASE("BacktestEngine preserves MarketEvent Bar payloads") {
    auto engine = make_engine();

    const auto event = engine.step();

    const auto& market_event =
        require_market_event(event);

    const auto& bar = market_event.bar();

    REQUIRE(bar.session_date() == first_date);
    REQUIRE(bar.symbol() == "AAPL");
    REQUIRE(bar.open() == 100.0);
    REQUIRE(bar.high() == 105.0);
    REQUIRE(bar.low() == 99.0);
    REQUIRE(bar.close() == 103.0);
    REQUIRE(bar.volume() == 1'000);
}

TEST_CASE(
    "BacktestEngine observes feed exhaustion only after the final event"
) {
    auto engine = make_engine();

    REQUIRE(engine.step().has_value());
    REQUIRE_FALSE(engine.finished());

    REQUIRE(engine.step().has_value());
    REQUIRE_FALSE(engine.finished());

    REQUIRE(engine.step().has_value());

    // The third Bar was delivered, but the engine has not yet observed
    // that the feed contains no subsequent Bar.
    REQUIRE_FALSE(engine.finished());

    const auto exhausted = engine.step();

    REQUIRE_FALSE(exhausted.has_value());
    REQUIRE(engine.finished());
}

TEST_CASE("BacktestEngine remains finished after exhaustion") {
    auto engine = make_engine();

    REQUIRE(engine.step().has_value());
    REQUIRE(engine.step().has_value());
    REQUIRE(engine.step().has_value());

    REQUIRE_FALSE(engine.step().has_value());
    REQUIRE(engine.finished());

    REQUIRE_FALSE(engine.step().has_value());
    REQUIRE_FALSE(engine.step().has_value());

    REQUIRE(engine.finished());
}

TEST_CASE("BacktestEngine produces deterministic repeated runs") {
    auto first_engine = make_engine();
    auto second_engine = make_engine();

    for (int i = 0; i < 3; ++i) {
        const auto first_event =
            first_engine.step();

        const auto second_event =
            second_engine.step();

        const auto& first_market =
            require_market_event(first_event);

        const auto& second_market =
            require_market_event(second_event);

        const auto& first_bar =
            first_market.bar();

        const auto& second_bar =
            second_market.bar();

        REQUIRE(
            first_bar.session_date()
            == second_bar.session_date()
        );

        REQUIRE(
            first_bar.symbol()
            == second_bar.symbol()
        );

        REQUIRE(
            first_bar.open()
            == second_bar.open()
        );

        REQUIRE(
            first_bar.high()
            == second_bar.high()
        );

        REQUIRE(
            first_bar.low()
            == second_bar.low()
        );

        REQUIRE(
            first_bar.close()
            == second_bar.close()
        );

        REQUIRE(
            first_bar.volume()
            == second_bar.volume()
        );
    }

    REQUIRE_FALSE(first_engine.step().has_value());
    REQUIRE_FALSE(second_engine.step().has_value());

    REQUIRE(first_engine.finished());
    REQUIRE(second_engine.finished());
}
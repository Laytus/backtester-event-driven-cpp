#include "backtester/data/historical_data_feed.hpp"
#include "backtester/engine/backtest_engine.hpp"
#include "backtester/events/event.hpp"
#include "backtester/market/bar.hpp"
#include "backtester/portfolio/portfolio.hpp"
#include "backtester/portfolio/position_sizer.hpp"
#include "backtester/strategy/strategy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace {

using backtester::data::HistoricalDataFeed;
using backtester::engine::BacktestEngine;

using backtester::events::Event;
using backtester::events::FillEvent;
using backtester::events::MarketEvent;
using backtester::events::OrderEvent;
using backtester::events::OrderSide;
using backtester::events::OrderType;
using backtester::events::SignalDirection;
using backtester::events::SignalEvent;

using backtester::market::Bar;
using backtester::market::SessionDate;

using backtester::portfolio::Portfolio;
using backtester::portfolio::PortfolioSnapshot;
using backtester::portfolio::PositionSizer;

using backtester::strategy::Strategy;

SessionDate make_date(
    int year,
    unsigned month,
    unsigned day
) {
    return SessionDate{
        std::chrono::year{year},
        std::chrono::month{month},
        std::chrono::day{day}
    };
}

const SessionDate first_date =
    make_date(2026, 10, 1);

const SessionDate second_date =
    make_date(2026, 10, 2);

const SessionDate third_date =
    make_date(2026, 10, 5);

const SessionDate fourth_date =
    make_date(2026, 10, 6);

class LongThenFlatStrategy final : public Strategy {
public:
    [[nodiscard]]
    std::vector<SignalEvent>
    on_market(
        const MarketEvent& event
    ) override {
        const auto& bar = event.bar();

        if (bar.session_date() == first_date) {
            return {
                SignalEvent{
                    bar.symbol(),
                    SignalDirection::Long,
                    bar.session_date()
                }
            };
        }

        if (bar.session_date() == third_date) {
            return {
                SignalEvent{
                    bar.symbol(),
                    SignalDirection::Flat,
                    bar.session_date()
                }
            };
        }

        return {};
    }
};

std::vector<Bar> make_bars() {
    return {
        Bar{
            first_date,
            "AAPL",
            100.0,
            105.0,
            95.0,
            100.0,
            1'000
        },
        Bar{
            second_date,
            "AAPL",
            110.0,
            125.0,
            108.0,
            120.0,
            1'200
        },
        Bar{
            third_date,
            "AAPL",
            115.0,
            132.0,
            112.0,
            130.0,
            1'400
        },
        Bar{
            fourth_date,
            "AAPL",
            125.0,
            130.0,
            120.0,
            128.0,
            1'600
        }
    };
}

BacktestEngine make_engine() {
    return BacktestEngine{
        HistoricalDataFeed{
            make_bars()
        },
        std::make_unique<LongThenFlatStrategy>(),
        Portfolio{
            "AAPL",
            1'000.0
        },
        PositionSizer{
            1.0
        }
    };
}

const MarketEvent& require_market_event(
    const Event& event
) {
    REQUIRE(
        std::holds_alternative<MarketEvent>(
            event
        )
    );

    return std::get<MarketEvent>(event);
}

const SignalEvent& require_signal_event(
    const Event& event
) {
    REQUIRE(
        std::holds_alternative<SignalEvent>(
            event
        )
    );

    return std::get<SignalEvent>(event);
}

const OrderEvent& require_order_event(
    const Event& event
) {
    REQUIRE(
        std::holds_alternative<OrderEvent>(
            event
        )
    );

    return std::get<OrderEvent>(event);
}

const FillEvent& require_fill_event(
    const Event& event
) {
    REQUIRE(
        std::holds_alternative<FillEvent>(
            event
        )
    );

    return std::get<FillEvent>(event);
}

void require_manual_oracle_events(
    const std::vector<Event>& events
) {
    REQUIRE(events.size() == 10);

    const auto& first_market =
        require_market_event(events[0]);

    REQUIRE(
        first_market.bar().session_date()
        == first_date
    );

    REQUIRE(
        first_market.bar().symbol()
        == "AAPL"
    );

    REQUIRE(
        first_market.bar().close()
        == 100.0
    );

    const auto& long_signal =
        require_signal_event(events[1]);

    REQUIRE(
        long_signal.symbol()
        == "AAPL"
    );

    REQUIRE(
        long_signal.direction()
        == SignalDirection::Long
    );

    REQUIRE(
        long_signal.signal_date()
        == first_date
    );

    const auto& buy_order =
        require_order_event(events[2]);

    REQUIRE(
        buy_order.symbol()
        == "AAPL"
    );

    REQUIRE(
        buy_order.side()
        == OrderSide::Buy
    );

    REQUIRE(
        buy_order.quantity()
        == 10
    );

    REQUIRE(
        buy_order.order_type()
        == OrderType::Market
    );

    REQUIRE(
        buy_order.submission_date()
        == first_date
    );

    const auto& buy_fill =
        require_fill_event(events[3]);

    REQUIRE(
        buy_fill.symbol()
        == "AAPL"
    );

    REQUIRE(
        buy_fill.side()
        == OrderSide::Buy
    );

    REQUIRE(
        buy_fill.quantity()
        == 9
    );

    REQUIRE(
        buy_fill.fill_price()
        == 110.0
    );

    REQUIRE(
        buy_fill.fill_date()
        == second_date
    );

    REQUIRE(
        buy_fill.fees()
        == 0.0
    );

    const auto& second_market =
        require_market_event(events[4]);

    REQUIRE(
        second_market.bar().session_date()
        == second_date
    );

    REQUIRE(
        second_market.bar().close()
        == 120.0
    );

    const auto& third_market =
        require_market_event(events[5]);

    REQUIRE(
        third_market.bar().session_date()
        == third_date
    );

    REQUIRE(
        third_market.bar().close()
        == 130.0
    );

    const auto& flat_signal =
        require_signal_event(events[6]);

    REQUIRE(
        flat_signal.symbol()
        == "AAPL"
    );

    REQUIRE(
        flat_signal.direction()
        == SignalDirection::Flat
    );

    REQUIRE(
        flat_signal.signal_date()
        == third_date
    );

    const auto& sell_order =
        require_order_event(events[7]);

    REQUIRE(
        sell_order.symbol()
        == "AAPL"
    );

    REQUIRE(
        sell_order.side()
        == OrderSide::Sell
    );

    REQUIRE(
        sell_order.quantity()
        == 9
    );

    REQUIRE(
        sell_order.order_type()
        == OrderType::Market
    );

    REQUIRE(
        sell_order.submission_date()
        == third_date
    );

    const auto& sell_fill =
        require_fill_event(events[8]);

    REQUIRE(
        sell_fill.symbol()
        == "AAPL"
    );

    REQUIRE(
        sell_fill.side()
        == OrderSide::Sell
    );

    REQUIRE(
        sell_fill.quantity()
        == 9
    );

    REQUIRE(
        sell_fill.fill_price()
        == 125.0
    );

    REQUIRE(
        sell_fill.fill_date()
        == fourth_date
    );

    REQUIRE(
        sell_fill.fees()
        == 0.0
    );

    const auto& fourth_market =
        require_market_event(events[9]);

    REQUIRE(
        fourth_market.bar().session_date()
        == fourth_date
    );

    REQUIRE(
        fourth_market.bar().close()
        == 128.0
    );
}

void require_manual_oracle_snapshots(
    const std::vector<PortfolioSnapshot>& snapshots
) {
    REQUIRE(snapshots.size() == 4);

    REQUIRE(
        snapshots[0].session_date()
        == first_date
    );

    REQUIRE(
        snapshots[0].cash()
        == 1'000.0
    );

    REQUIRE(
        snapshots[0].position_quantity()
        == 0
    );

    REQUIRE(
        snapshots[0].market_value()
        == 0.0
    );

    REQUIRE(
        snapshots[0].equity()
        == 1'000.0
    );

    REQUIRE(
        snapshots[1].session_date()
        == second_date
    );

    REQUIRE(
        snapshots[1].cash()
        == 10.0
    );

    REQUIRE(
        snapshots[1].position_quantity()
        == 9
    );

    REQUIRE(
        snapshots[1].market_value()
        == 1'080.0
    );

    REQUIRE(
        snapshots[1].equity()
        == 1'090.0
    );

    REQUIRE(
        snapshots[2].session_date()
        == third_date
    );

    REQUIRE(
        snapshots[2].cash()
        == 10.0
    );

    REQUIRE(
        snapshots[2].position_quantity()
        == 9
    );

    REQUIRE(
        snapshots[2].market_value()
        == 1'170.0
    );

    REQUIRE(
        snapshots[2].equity()
        == 1'180.0
    );

    REQUIRE(
        snapshots[3].session_date()
        == fourth_date
    );

    REQUIRE(
        snapshots[3].cash()
        == 1'135.0
    );

    REQUIRE(
        snapshots[3].position_quantity()
        == 0
    );

    REQUIRE(
        snapshots[3].market_value()
        == 0.0
    );

    REQUIRE(
        snapshots[3].equity()
        == 1'135.0
    );
}

void require_same_event(
    const Event& first,
    const Event& second
) {
    REQUIRE(first.index() == second.index());

    if (
        std::holds_alternative<MarketEvent>(
            first
        )
    ) {
        const auto& first_event =
            std::get<MarketEvent>(first);

        const auto& second_event =
            std::get<MarketEvent>(second);

        const auto& first_bar =
            first_event.bar();

        const auto& second_bar =
            second_event.bar();

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

        return;
    }

    if (
        std::holds_alternative<SignalEvent>(
            first
        )
    ) {
        const auto& first_event =
            std::get<SignalEvent>(first);

        const auto& second_event =
            std::get<SignalEvent>(second);

        REQUIRE(
            first_event.symbol()
            == second_event.symbol()
        );

        REQUIRE(
            first_event.direction()
            == second_event.direction()
        );

        REQUIRE(
            first_event.signal_date()
            == second_event.signal_date()
        );

        return;
    }

    if (
        std::holds_alternative<OrderEvent>(
            first
        )
    ) {
        const auto& first_event =
            std::get<OrderEvent>(first);

        const auto& second_event =
            std::get<OrderEvent>(second);

        REQUIRE(
            first_event.symbol()
            == second_event.symbol()
        );

        REQUIRE(
            first_event.side()
            == second_event.side()
        );

        REQUIRE(
            first_event.quantity()
            == second_event.quantity()
        );

        REQUIRE(
            first_event.order_type()
            == second_event.order_type()
        );

        REQUIRE(
            first_event.submission_date()
            == second_event.submission_date()
        );

        return;
    }

    const auto& first_event =
        std::get<FillEvent>(first);

    const auto& second_event =
        std::get<FillEvent>(second);

    REQUIRE(
        first_event.symbol()
        == second_event.symbol()
    );

    REQUIRE(
        first_event.side()
        == second_event.side()
    );

    REQUIRE(
        first_event.quantity()
        == second_event.quantity()
    );

    REQUIRE(
        first_event.fill_price()
        == second_event.fill_price()
    );

    REQUIRE(
        first_event.fill_date()
        == second_event.fill_date()
    );

    REQUIRE(
        first_event.fees()
        == second_event.fees()
    );
}

void require_same_snapshots(
    const std::vector<PortfolioSnapshot>& first,
    const std::vector<PortfolioSnapshot>& second
) {
    REQUIRE(first.size() == second.size());

    for (
        std::size_t i = 0;
        i < first.size();
        ++i
    ) {
        REQUIRE(
            first[i].session_date()
            == second[i].session_date()
        );

        REQUIRE(
            first[i].cash()
            == second[i].cash()
        );

        REQUIRE(
            first[i].position_quantity()
            == second[i].position_quantity()
        );

        REQUIRE(
            first[i].market_value()
            == second[i].market_value()
        );

        REQUIRE(
            first[i].equity()
            == second[i].equity()
        );
    }
}

std::vector<Event> run_to_completion(
    BacktestEngine& engine
) {
    std::vector<Event> events;

    while (const auto event = engine.step()) {
        events.push_back(*event);
    }

    return events;
}

}  // namespace

TEST_CASE(
    "Phase 3 end-to-end backtest matches the manual financial oracle"
) {
    auto engine = make_engine();

    std::vector<Event> events;

    // The manual oracle contains exactly ten observable events.
    for (std::size_t i = 0; i < 10; ++i) {
        const auto event = engine.step();

        REQUIRE(event.has_value());

        events.push_back(*event);
    }

    // The last MarketEvent has been processed, but exhaustion has not
    // yet been observed through HistoricalDataFeed::next().
    REQUIRE_FALSE(engine.finished());

    const auto exhausted = engine.step();

    REQUIRE_FALSE(exhausted.has_value());
    REQUIRE(engine.finished());

    require_manual_oracle_events(events);

    const auto& portfolio =
        engine.portfolio();

    require_manual_oracle_snapshots(
        portfolio.snapshots()
    );

    REQUIRE(
        portfolio.cash() == 1'135.0
    );

    REQUIRE(
        portfolio.position_quantity() == 0
    );

    REQUIRE(
        portfolio.latest_market_value() == 0.0
    );

    REQUIRE(
        portfolio.latest_equity() == 1'135.0
    );

    const auto cash_before = portfolio.cash();

    const auto position_before = portfolio.position_quantity();

    const auto market_value_before = portfolio.latest_market_value();

    const auto equity_before = portfolio.latest_equity();

    const auto snapshot_count_before = portfolio.snapshots().size();

    // Completion must remain stable.
    REQUIRE_FALSE(
        engine.step().has_value()
    );

    REQUIRE_FALSE(
        engine.step().has_value()
    );

    REQUIRE(engine.finished());

    REQUIRE(
        engine.portfolio().cash()
        == cash_before
    );

    REQUIRE(
        engine.portfolio().position_quantity()
        == position_before
    );

    REQUIRE(
        engine.portfolio().latest_market_value()
        == market_value_before
    );

    REQUIRE(
        engine.portfolio().latest_equity()
        == equity_before
    );

    REQUIRE(
        engine.portfolio().snapshots().size()
        == snapshot_count_before
    );
}

TEST_CASE(
    "Phase 3 end-to-end backtest is deterministic across repeated runs"
) {
    auto first_engine = make_engine();

    auto second_engine = make_engine();

    const auto first_events = run_to_completion(first_engine);

    const auto second_events = run_to_completion(second_engine);

    REQUIRE(first_engine.finished());
    REQUIRE(second_engine.finished());

    REQUIRE(
        first_events.size()
        == second_events.size()
    );

    REQUIRE(
        first_events.size()
        == 10
    );

    for (
        std::size_t i = 0;
        i < first_events.size();
        ++i
    ) {
        require_same_event(
            first_events[i],
            second_events[i]
        );
    }

    require_same_snapshots(
        first_engine.portfolio().snapshots(),
        second_engine.portfolio().snapshots()
    );

    REQUIRE(
        first_engine.portfolio().cash()
        == second_engine.portfolio().cash()
    );

    REQUIRE(
        first_engine.portfolio().position_quantity()
        == second_engine.portfolio().position_quantity()
    );

    REQUIRE(
        first_engine.portfolio().latest_market_value()
        == second_engine.portfolio().latest_market_value()
    );

    REQUIRE(
        first_engine.portfolio().latest_equity()
        == second_engine.portfolio().latest_equity()
    );
}
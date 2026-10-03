#include "backtester/data/historical_data_feed.hpp"
#include "backtester/engine/backtest_engine.hpp"
#include "backtester/events/event.hpp"
#include "backtester/market/bar.hpp"
#include "backtester/portfolio/portfolio.hpp"
#include "backtester/portfolio/position_sizer.hpp"
#include "backtester/strategy/buy_and_hold_strategy.hpp"
#include "backtester/strategy/strategy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
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
using backtester::portfolio::PositionSizer;

using backtester::strategy::BuyAndHoldStrategy;
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
            1'500
        }
    };
}

class SilentStrategy final : public Strategy {
public:
    [[nodiscard]]
    std::vector<SignalEvent>
    on_market(const MarketEvent&) override {
        return {};
    }
};

class SignalOnDateStrategy final : public Strategy {
public:
    explicit SignalOnDateStrategy(
        SessionDate signal_date
    )
        : signal_date_(signal_date) {}

    [[nodiscard]]
    std::vector<SignalEvent>
    on_market(
        const MarketEvent& event
    ) override {
        if (has_signaled_) {
            return {};
        }

        if (
            event.bar().session_date()
            != signal_date_
        ) {
            return {};
        }

        has_signaled_ = true;

        return {
            SignalEvent{
                event.bar().symbol(),
                SignalDirection::Long,
                event.bar().session_date()
            }
        };
    }

private:
    SessionDate signal_date_;
    bool has_signaled_{false};
};

BacktestEngine make_buy_and_hold_engine() {
    return BacktestEngine{
        HistoricalDataFeed{
            make_bars()
        },
        std::make_unique<BuyAndHoldStrategy>(
            "AAPL"
        ),
        Portfolio{
            "AAPL",
            1'000.0
        },
        PositionSizer{
            1.0
        }
    };
}

BacktestEngine make_silent_engine() {
    return BacktestEngine{
        HistoricalDataFeed{
            make_bars()
        },
        std::make_unique<SilentStrategy>(),
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
    const std::optional<Event>& event
) {
    REQUIRE(event.has_value());
    REQUIRE(
        std::holds_alternative<MarketEvent>(
            *event
        )
    );

    return std::get<MarketEvent>(*event);
}

const SignalEvent& require_signal_event(
    const std::optional<Event>& event
) {
    REQUIRE(event.has_value());
    REQUIRE(
        std::holds_alternative<SignalEvent>(
            *event
        )
    );

    return std::get<SignalEvent>(*event);
}

const OrderEvent& require_order_event(
    const std::optional<Event>& event
) {
    REQUIRE(event.has_value());
    REQUIRE(
        std::holds_alternative<OrderEvent>(
            *event
        )
    );

    return std::get<OrderEvent>(*event);
}

const FillEvent& require_fill_event(
    const std::optional<Event>& event
) {
    REQUIRE(event.has_value());
    REQUIRE(
        std::holds_alternative<FillEvent>(
            *event
        )
    );

    return std::get<FillEvent>(*event);
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

}  // namespace

TEST_CASE(
    "BacktestEngine rejects a null Strategy"
) {
    HistoricalDataFeed data_feed{
        make_bars()
    };

    std::unique_ptr<Strategy> strategy;

    Portfolio portfolio{
        "AAPL",
        1'000.0
    };

    PositionSizer position_sizer{
        1.0
    };

    REQUIRE_THROWS_AS(
        BacktestEngine(
            std::move(data_feed),
            std::move(strategy),
            std::move(portfolio),
            std::move(position_sizer)
        ),
        std::invalid_argument
    );
}

TEST_CASE(
    "BacktestEngine rejects market data for a different Portfolio symbol"
) {
    std::vector<Bar> bars{
        Bar{
            first_date,
            "MSFT",
            100.0,
            105.0,
            95.0,
            100.0,
            1'000
        }
    };

    BacktestEngine engine{
        HistoricalDataFeed{
            std::move(bars)
        },
        std::make_unique<SilentStrategy>(),
        Portfolio{
            "AAPL",
            1'000.0
        },
        PositionSizer{
            1.0
        }
    };

    REQUIRE_THROWS_AS(
        engine.step(),
        std::invalid_argument
    );
}

TEST_CASE(
    "BacktestEngine preserves market-only behavior with a silent Strategy"
) {
    auto engine = make_silent_engine();

    for (std::size_t i = 0; i < 3; ++i) {
        const auto event = engine.step();

        REQUIRE(event.has_value());
        REQUIRE(
            std::holds_alternative<MarketEvent>(
                *event
            )
        );

        REQUIRE(
            engine.portfolio()
                .snapshots()
                .size()
            == i + 1
        );
    }

    REQUIRE_FALSE(engine.step().has_value());
    REQUIRE(engine.finished());

    const auto& snapshots =
        engine.portfolio().snapshots();

    REQUIRE(snapshots.size() == 3);

    REQUIRE(
        snapshots[0].session_date()
        == first_date
    );

    REQUIRE(
        snapshots[1].session_date()
        == second_date
    );

    REQUIRE(
        snapshots[2].session_date()
        == third_date
    );
}

TEST_CASE(
    "BacktestEngine produces Market Signal Order in causal order"
) {
    auto engine = make_buy_and_hold_engine();

    const auto market = engine.step();

    const auto& market_event =
        require_market_event(market);

    REQUIRE(
        market_event.bar().session_date()
        == first_date
    );

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .empty()
    );

    const auto signal = engine.step();

    const auto& signal_event =
        require_signal_event(signal);

    REQUIRE(
        signal_event.symbol()
        == "AAPL"
    );

    REQUIRE(
        signal_event.direction()
        == SignalDirection::Long
    );

    REQUIRE(
        signal_event.signal_date()
        == first_date
    );

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .empty()
    );

    const auto order = engine.step();

    const auto& order_event =
        require_order_event(order);

    REQUIRE(
        order_event.symbol()
        == "AAPL"
    );

    REQUIRE(
        order_event.side()
        == OrderSide::Buy
    );

    REQUIRE(
        order_event.quantity()
        == 10
    );

    REQUIRE(
        order_event.order_type()
        == OrderType::Market
    );

    REQUIRE(
        order_event.submission_date()
        == first_date
    );

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .size()
        == 1
    );
}

TEST_CASE(
    "BacktestEngine executes the pending order at the next open before MarketEvent"
) {
    auto engine = make_buy_and_hold_engine();

    REQUIRE(
        std::holds_alternative<MarketEvent>(
            *engine.step()
        )
    );

    REQUIRE(
        std::holds_alternative<SignalEvent>(
            *engine.step()
        )
    );

    REQUIRE(
        std::holds_alternative<OrderEvent>(
            *engine.step()
        )
    );

    const auto fill = engine.step();

    const auto& fill_event =
        require_fill_event(fill);

    REQUIRE(
        fill_event.symbol()
        == "AAPL"
    );

    REQUIRE(
        fill_event.side()
        == OrderSide::Buy
    );

    // Requested quantity was 10 at Close(1) = 100.
    // At Open(2) = 110, only floor(1,000 / 110) = 9
    // shares are affordable.
    REQUIRE(
        fill_event.quantity()
        == 9
    );

    REQUIRE(
        fill_event.fill_price()
        == 110.0
    );

    REQUIRE(
        fill_event.fill_date()
        == second_date
    );

    REQUIRE(
        fill_event.fees()
        == 0.0
    );

    // FillEvent has already been processed before step() returns it.
    REQUIRE(
        engine.portfolio()
            .position_quantity()
        == 9
    );

    REQUIRE(
        engine.portfolio().cash()
        == 10.0
    );

    // Session 2 has not completed yet because MarketEvent(2)
    // is still pending.
    REQUIRE(
        engine.portfolio()
            .snapshots()
            .size()
        == 1
    );

    const auto market = engine.step();

    const auto& market_event =
        require_market_event(market);

    REQUIRE(
        market_event.bar().session_date()
        == second_date
    );

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .size()
        == 2
    );
}

TEST_CASE(
    "BacktestEngine creates the session snapshot only after its event chain completes"
) {
    auto engine = make_buy_and_hold_engine();

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .empty()
    );

    const auto market = engine.step();

    REQUIRE(
        std::holds_alternative<MarketEvent>(
            *market
        )
    );

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .empty()
    );

    const auto signal = engine.step();

    REQUIRE(
        std::holds_alternative<SignalEvent>(
            *signal
        )
    );

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .empty()
    );

    const auto order = engine.step();

    REQUIRE(
        std::holds_alternative<OrderEvent>(
            *order
        )
    );

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .size()
        == 1
    );

    const auto& first_snapshot =
        engine.portfolio()
            .snapshots()
            .front();

    REQUIRE(
        first_snapshot.session_date()
        == first_date
    );

    REQUIRE(
        first_snapshot.cash()
        == 1'000.0
    );

    REQUIRE(
        first_snapshot.position_quantity()
        == 0
    );

    REQUIRE(
        first_snapshot.market_value()
        == 0.0
    );

    REQUIRE(
        first_snapshot.equity()
        == 1'000.0
    );

    const auto fill = engine.step();

    REQUIRE(
        std::holds_alternative<FillEvent>(
            *fill
        )
    );

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .size()
        == 1
    );

    const auto second_market = engine.step();

    REQUIRE(
        std::holds_alternative<MarketEvent>(
            *second_market
        )
    );

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .size()
        == 2
    );

    const auto& second_snapshot =
        engine.portfolio()
            .snapshots()[1];

    REQUIRE(
        second_snapshot.session_date()
        == second_date
    );

    REQUIRE(
        second_snapshot.cash()
        == 10.0
    );

    REQUIRE(
        second_snapshot.position_quantity()
        == 9
    );

    REQUIRE(
        second_snapshot.market_value()
        == 1'080.0
    );

    REQUIRE(
        second_snapshot.equity()
        == 1'090.0
    );
}

TEST_CASE(
    "BacktestEngine discards an order submitted on the final session"
) {
    std::vector<Bar> bars{
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
        }
    };

    BacktestEngine engine{
        HistoricalDataFeed{
            std::move(bars)
        },
        std::make_unique<SignalOnDateStrategy>(
            second_date
        ),
        Portfolio{
            "AAPL",
            1'000.0
        },
        PositionSizer{
            1.0
        }
    };

    const auto first_market = engine.step();

    REQUIRE(
        std::holds_alternative<MarketEvent>(
            *first_market
        )
    );

    const auto second_market = engine.step();

    REQUIRE(
        std::holds_alternative<MarketEvent>(
            *second_market
        )
    );

    const auto signal = engine.step();

    REQUIRE(
        std::holds_alternative<SignalEvent>(
            *signal
        )
    );

    const auto order = engine.step();

    REQUIRE(
        std::holds_alternative<OrderEvent>(
            *order
        )
    );

    REQUIRE(
        engine.portfolio()
            .position_quantity()
        == 0
    );

    REQUIRE(
        engine.portfolio().cash()
        == 1'000.0
    );

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .size()
        == 2
    );

    // No later session exists, so the pending order must be
    // discarded instead of receiving a synthetic final-close fill.
    const auto exhausted = engine.step();

    REQUIRE_FALSE(exhausted.has_value());
    REQUIRE(engine.finished());

    REQUIRE(
        engine.portfolio()
            .position_quantity()
        == 0
    );

    REQUIRE(
        engine.portfolio().cash()
        == 1'000.0
    );
}

TEST_CASE(
    "BacktestEngine completes all current-session events before advancing"
) {
    auto engine = make_buy_and_hold_engine();

    const auto first = engine.step();
    const auto second = engine.step();
    const auto third = engine.step();
    const auto fourth = engine.step();
    const auto fifth = engine.step();

    REQUIRE(
        std::holds_alternative<MarketEvent>(
            *first
        )
    );

    REQUIRE(
        std::get<MarketEvent>(*first)
            .bar()
            .session_date()
        == first_date
    );

    REQUIRE(
        std::holds_alternative<SignalEvent>(
            *second
        )
    );

    REQUIRE(
        std::get<SignalEvent>(*second)
            .signal_date()
        == first_date
    );

    REQUIRE(
        std::holds_alternative<OrderEvent>(
            *third
        )
    );

    REQUIRE(
        std::get<OrderEvent>(*third)
            .submission_date()
        == first_date
    );

    REQUIRE(
        std::holds_alternative<FillEvent>(
            *fourth
        )
    );

    REQUIRE(
        std::get<FillEvent>(*fourth)
            .fill_date()
        == second_date
    );

    REQUIRE(
        std::holds_alternative<MarketEvent>(
            *fifth
        )
    );

    REQUIRE(
        std::get<MarketEvent>(*fifth)
            .bar()
            .session_date()
        == second_date
    );
}

TEST_CASE(
    "BacktestEngine produces deterministic event and Portfolio histories"
) {
    auto first_engine =
        make_buy_and_hold_engine();

    auto second_engine =
        make_buy_and_hold_engine();

    while (true) {
        const auto first_event =
            first_engine.step();

        const auto second_event =
            second_engine.step();

        REQUIRE(
            first_event.has_value()
            == second_event.has_value()
        );

        if (!first_event.has_value()) {
            break;
        }

        require_same_event(
            *first_event,
            *second_event
        );
    }

    REQUIRE(first_engine.finished());
    REQUIRE(second_engine.finished());

    const auto& first_portfolio =
        first_engine.portfolio();

    const auto& second_portfolio =
        second_engine.portfolio();

    REQUIRE(
        first_portfolio.cash()
        == second_portfolio.cash()
    );

    REQUIRE(
        first_portfolio.position_quantity()
        == second_portfolio.position_quantity()
    );

    REQUIRE(
        first_portfolio.latest_market_value()
        == second_portfolio.latest_market_value()
    );

    REQUIRE(
        first_portfolio.latest_equity()
        == second_portfolio.latest_equity()
    );

    const auto& first_snapshots =
        first_portfolio.snapshots();

    const auto& second_snapshots =
        second_portfolio.snapshots();

    REQUIRE(
        first_snapshots.size()
        == second_snapshots.size()
    );

    for (
        std::size_t i = 0;
        i < first_snapshots.size();
        ++i
    ) {
        REQUIRE(
            first_snapshots[i].session_date()
            == second_snapshots[i].session_date()
        );

        REQUIRE(
            first_snapshots[i].cash()
            == second_snapshots[i].cash()
        );

        REQUIRE(
            first_snapshots[i].position_quantity()
            == second_snapshots[i].position_quantity()
        );

        REQUIRE(
            first_snapshots[i].market_value()
            == second_snapshots[i].market_value()
        );

        REQUIRE(
            first_snapshots[i].equity()
            == second_snapshots[i].equity()
        );
    }
}

TEST_CASE(
    "BacktestEngine remains stable after complete exhaustion"
) {
    auto engine = make_buy_and_hold_engine();

    while (engine.step().has_value()) {
    }

    REQUIRE(engine.finished());

    const auto cash_before =
        engine.portfolio().cash();

    const auto position_before =
        engine.portfolio()
            .position_quantity();

    const auto equity_before =
        engine.portfolio()
            .latest_equity();

    const auto snapshot_count_before =
        engine.portfolio()
            .snapshots()
            .size();

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
        engine.portfolio()
            .position_quantity()
        == position_before
    );

    REQUIRE(
        engine.portfolio()
            .latest_equity()
        == equity_before
    );

    REQUIRE(
        engine.portfolio()
            .snapshots()
            .size()
        == snapshot_count_before
    );
}
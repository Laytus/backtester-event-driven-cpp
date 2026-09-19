#include "backtester/events/event_queue.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <type_traits>
#include <variant>

namespace {

using backtester::events::Event;
using backtester::events::EventQueue;
using backtester::events::FillEvent;
using backtester::events::MarketEvent;
using backtester::events::OrderEvent;
using backtester::events::OrderSide;
using backtester::events::OrderType;
using backtester::events::SignalDirection;
using backtester::events::SignalEvent;
using backtester::market::Bar;
using backtester::market::SessionDate;

const SessionDate date{
    std::chrono::year{2026},
    std::chrono::month{9},
    std::chrono::day{18}
};

Bar make_bar() {
    return Bar{
        date,
        "AAPL",
        100.0,
        105.0,
        99.0,
        103.0,
        1'000
    };
}

static_assert(!std::is_copy_constructible_v<EventQueue>);
static_assert(!std::is_copy_assignable_v<EventQueue>);

static_assert(std::is_move_constructible_v<EventQueue>);
static_assert(std::is_move_assignable_v<EventQueue>);

}  // namespace

TEST_CASE("EventQueue is initially empty") {
    EventQueue queue;

    REQUIRE(queue.empty());
}

TEST_CASE("EventQueue returns nullopt when empty") {
    EventQueue queue;

    const auto event = queue.pop();

    REQUIRE_FALSE(event.has_value());
    REQUIRE(queue.empty());
}

TEST_CASE("EventQueue remains deterministically empty after repeated pops") {
    EventQueue queue;

    REQUIRE_FALSE(queue.pop().has_value());
    REQUIRE_FALSE(queue.pop().has_value());
    REQUIRE_FALSE(queue.pop().has_value());

    REQUIRE(queue.empty());
}

TEST_CASE("EventQueue stores and returns one event") {
    EventQueue queue;

    queue.push(
        Event{
            SignalEvent{
                "AAPL",
                SignalDirection::Long,
                date
            }
        }
    );

    REQUIRE_FALSE(queue.empty());

    const auto event = queue.pop();

    REQUIRE(event.has_value());
    REQUIRE(std::holds_alternative<SignalEvent>(*event));

    const auto& signal = std::get<SignalEvent>(*event);

    REQUIRE(signal.symbol() == "AAPL");
    REQUIRE(signal.direction() == SignalDirection::Long);
    REQUIRE(signal.signal_date() == date);

    REQUIRE(queue.empty());
}

TEST_CASE("EventQueue removes each event exactly once") {
    EventQueue queue;

    queue.push(
        Event{
            OrderEvent{
                "AAPL",
                OrderSide::Buy,
                10,
                OrderType::Market,
                date
            }
        }
    );

    const auto first = queue.pop();
    const auto second = queue.pop();

    REQUIRE(first.has_value());
    REQUIRE_FALSE(second.has_value());

    REQUIRE(queue.empty());
}

TEST_CASE("EventQueue preserves FIFO ordering") {
    EventQueue queue;

    queue.push(
        Event{
            SignalEvent{
                "AAPL",
                SignalDirection::Long,
                date
            }
        }
    );

    queue.push(
        Event{
            SignalEvent{
                "MSFT",
                SignalDirection::Flat,
                date
            }
        }
    );

    queue.push(
        Event{
            SignalEvent{
                "GOOG",
                SignalDirection::Short,
                date
            }
        }
    );

    const auto first = queue.pop();
    const auto second = queue.pop();
    const auto third = queue.pop();

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(third.has_value());

    REQUIRE(
        std::get<SignalEvent>(*first).symbol()
        == "AAPL"
    );

    REQUIRE(
        std::get<SignalEvent>(*second).symbol()
        == "MSFT"
    );

    REQUIRE(
        std::get<SignalEvent>(*third).symbol()
        == "GOOG"
    );

    REQUIRE(queue.empty());
}

TEST_CASE("EventQueue preserves mixed event type ordering") {
    EventQueue queue;

    queue.push(
        Event{
            MarketEvent{
                make_bar()
            }
        }
    );

    queue.push(
        Event{
            SignalEvent{
                "AAPL",
                SignalDirection::Long,
                date
            }
        }
    );

    queue.push(
        Event{
            OrderEvent{
                "AAPL",
                OrderSide::Buy,
                10,
                OrderType::Market,
                date
            }
        }
    );

    queue.push(
        Event{
            FillEvent{
                "AAPL",
                OrderSide::Buy,
                10,
                101.25,
                date,
                0.0
            }
        }
    );

    const auto first = queue.pop();
    const auto second = queue.pop();
    const auto third = queue.pop();
    const auto fourth = queue.pop();

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(third.has_value());
    REQUIRE(fourth.has_value());

    REQUIRE(std::holds_alternative<MarketEvent>(*first));
    REQUIRE(std::holds_alternative<SignalEvent>(*second));
    REQUIRE(std::holds_alternative<OrderEvent>(*third));
    REQUIRE(std::holds_alternative<FillEvent>(*fourth));

    REQUIRE(queue.empty());
}

TEST_CASE("EventQueue preserves event payloads") {
    EventQueue queue;

    queue.push(
        Event{
            MarketEvent{
                make_bar()
            }
        }
    );

    queue.push(
        Event{
            FillEvent{
                "AAPL",
                OrderSide::Sell,
                25,
                104.75,
                date,
                1.25
            }
        }
    );

    const auto market_result = queue.pop();
    const auto fill_result = queue.pop();

    REQUIRE(market_result.has_value());
    REQUIRE(fill_result.has_value());

    const auto& market_event =
        std::get<MarketEvent>(*market_result);

    const auto& bar = market_event.bar();

    REQUIRE(bar.session_date() == date);
    REQUIRE(bar.symbol() == "AAPL");
    REQUIRE(bar.open() == 100.0);
    REQUIRE(bar.high() == 105.0);
    REQUIRE(bar.low() == 99.0);
    REQUIRE(bar.close() == 103.0);
    REQUIRE(bar.volume() == 1'000);

    const auto& fill_event =
        std::get<FillEvent>(*fill_result);

    REQUIRE(fill_event.symbol() == "AAPL");
    REQUIRE(fill_event.side() == OrderSide::Sell);
    REQUIRE(fill_event.quantity() == 25);
    REQUIRE(fill_event.fill_price() == 104.75);
    REQUIRE(fill_event.fill_date() == date);
    REQUIRE(fill_event.fees() == 1.25);
}
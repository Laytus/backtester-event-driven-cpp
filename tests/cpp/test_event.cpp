#include "backtester/events/event.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <limits>
#include <string>
#include <type_traits>
#include <variant>
#include <stdexcept>

namespace {

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

const SessionDate valid_date{
    std::chrono::year{2026},
    std::chrono::month{9},
    std::chrono::day{18}
};

const SessionDate invalid_date{
    std::chrono::year{2026},
    std::chrono::month{2},
    std::chrono::day{30}
};

Bar make_bar() {
    return Bar{
        valid_date,
        "AAPL",
        100.0,
        105.0,
        99.0,
        103.0,
        1'000
    };
}

}  // namespace

TEST_CASE("MarketEvent owns and exposes its Bar") {
    MarketEvent event{make_bar()};

    const Bar& bar = event.bar();

    REQUIRE(bar.session_date() == valid_date);
    REQUIRE(bar.symbol() == "AAPL");
    REQUIRE(bar.open() == 100.0);
    REQUIRE(bar.high() == 105.0);
    REQUIRE(bar.low() == 99.0);
    REQUIRE(bar.close() == 103.0);
    REQUIRE(bar.volume() == 1'000);
}

TEST_CASE("SignalEvent stores valid payload") {
    SignalEvent event{
        "AAPL",
        SignalDirection::Long,
        valid_date
    };

    REQUIRE(event.symbol() == "AAPL");
    REQUIRE(event.direction() == SignalDirection::Long);
    REQUIRE(event.signal_date() == valid_date);
}

TEST_CASE("SignalEvent rejects empty symbol") {
    REQUIRE_THROWS_AS(
        SignalEvent(
            "",
            SignalDirection::Long,
            valid_date
        ),
        std::invalid_argument
    );
}

TEST_CASE("SignalEvent rejects invalid date") {
    REQUIRE_THROWS_AS(
        SignalEvent(
            "AAPL",
            SignalDirection::Long,
            invalid_date
        ),
        std::invalid_argument
    );
}

TEST_CASE("OrderEvent stores valid payload") {
    OrderEvent event{
        "AAPL",
        OrderSide::Buy,
        10,
        OrderType::Market,
        valid_date
    };

    REQUIRE(event.symbol() == "AAPL");
    REQUIRE(event.side() == OrderSide::Buy);
    REQUIRE(event.quantity() == 10);
    REQUIRE(event.order_type() == OrderType::Market);
    REQUIRE(event.submission_date() == valid_date);
}

TEST_CASE("OrderEvent rejects empty symbol") {
    REQUIRE_THROWS_AS(
        OrderEvent(
            "",
            OrderSide::Buy,
            10,
            OrderType::Market,
            valid_date
        ),
        std::invalid_argument
    );
}

TEST_CASE("OrderEvent rejects zero quantity") {
    REQUIRE_THROWS_AS(
        OrderEvent(
            "AAPL",
            OrderSide::Buy,
            0,
            OrderType::Market,
            valid_date
        ),
        std::invalid_argument
    );
}

TEST_CASE("OrderEvent rejects invalid submission date") {
    REQUIRE_THROWS_AS(
        OrderEvent(
            "AAPL",
            OrderSide::Buy,
            10,
            OrderType::Market,
            invalid_date
        ),
        std::invalid_argument
    );
}

TEST_CASE("FillEvent stores valid payload") {
    FillEvent event{
        "AAPL",
        OrderSide::Buy,
        10,
        101.25,
        valid_date,
        1.50
    };

    REQUIRE(event.symbol() == "AAPL");
    REQUIRE(event.side() == OrderSide::Buy);
    REQUIRE(event.quantity() == 10);
    REQUIRE(event.fill_price() == 101.25);
    REQUIRE(event.fill_date() == valid_date);
    REQUIRE(event.fees() == 1.50);
}

TEST_CASE("FillEvent accepts zero fees") {
    FillEvent event{
        "AAPL",
        OrderSide::Buy,
        10,
        101.25,
        valid_date,
        0.0
    };

    REQUIRE(event.fees() == 0.0);
}

TEST_CASE("FillEvent rejects empty symbol") {
    REQUIRE_THROWS_AS(
        FillEvent(
            "",
            OrderSide::Buy,
            10,
            101.25,
            valid_date,
            0.0
        ),
        std::invalid_argument
    );
}

TEST_CASE("FillEvent rejects zero quantity") {
    REQUIRE_THROWS_AS(
        FillEvent(
            "AAPL",
            OrderSide::Buy,
            0,
            101.25,
            valid_date,
            0.0
        ),
        std::invalid_argument
    );
}

TEST_CASE("FillEvent rejects non-positive fill price") {
    REQUIRE_THROWS_AS(
        FillEvent(
            "AAPL",
            OrderSide::Buy,
            10,
            0.0,
            valid_date,
            0.0
        ),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        FillEvent(
            "AAPL",
            OrderSide::Buy,
            10,
            -1.0,
            valid_date,
            0.0
        ),
        std::invalid_argument
    );
}

TEST_CASE("FillEvent rejects non-finite fill price") {
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    REQUIRE_THROWS_AS(
        FillEvent(
            "AAPL",
            OrderSide::Buy,
            10,
            infinity,
            valid_date,
            0.0
        ),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        FillEvent(
            "AAPL",
            OrderSide::Buy,
            10,
            nan,
            valid_date,
            0.0
        ),
        std::invalid_argument
    );
}

TEST_CASE("FillEvent rejects invalid fill date") {
    REQUIRE_THROWS_AS(
        FillEvent(
            "AAPL",
            OrderSide::Buy,
            10,
            101.25,
            invalid_date,
            0.0
        ),
        std::invalid_argument
    );
}

TEST_CASE("FillEvent rejects negative fees") {
    REQUIRE_THROWS_AS(
        FillEvent(
            "AAPL",
            OrderSide::Buy,
            10,
            101.25,
            valid_date,
            -0.01
        ),
        std::invalid_argument
    );
}

TEST_CASE("FillEvent rejects non-finite fees") {
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    REQUIRE_THROWS_AS(
        FillEvent(
            "AAPL",
            OrderSide::Buy,
            10,
            101.25,
            valid_date,
            infinity
        ),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        FillEvent(
            "AAPL",
            OrderSide::Buy,
            10,
            101.25,
            valid_date,
            nan
        ),
        std::invalid_argument
    );
}

TEST_CASE("Event preserves MarketEvent alternative") {
    Event event{MarketEvent{make_bar()}};

    REQUIRE(std::holds_alternative<MarketEvent>(event));

    const auto& market_event = std::get<MarketEvent>(event);

    REQUIRE(market_event.bar().symbol() == "AAPL");
}

TEST_CASE("Event preserves SignalEvent alternative") {
    Event event{
        SignalEvent{
            "AAPL",
            SignalDirection::Long,
            valid_date
        }
    };

    REQUIRE(std::holds_alternative<SignalEvent>(event));

    const auto& signal_event = std::get<SignalEvent>(event);

    REQUIRE(signal_event.symbol() == "AAPL");
    REQUIRE(signal_event.direction() == SignalDirection::Long);
}

TEST_CASE("Event preserves OrderEvent alternative") {
    Event event{
        OrderEvent{
            "AAPL",
            OrderSide::Buy,
            10,
            OrderType::Market,
            valid_date
        }
    };

    REQUIRE(std::holds_alternative<OrderEvent>(event));

    const auto& order_event = std::get<OrderEvent>(event);

    REQUIRE(order_event.quantity() == 10);
    REQUIRE(order_event.side() == OrderSide::Buy);
}

TEST_CASE("Event preserves FillEvent alternative") {
    Event event{
        FillEvent{
            "AAPL",
            OrderSide::Buy,
            10,
            101.25,
            valid_date,
            0.0
        }
    };

    REQUIRE(std::holds_alternative<FillEvent>(event));

    const auto& fill_event = std::get<FillEvent>(event);

    REQUIRE(fill_event.fill_price() == 101.25);
    REQUIRE(fill_event.fees() == 0.0);
}

TEST_CASE("Event dispatch through std::visit preserves concrete type") {
    const Event market_event{MarketEvent{make_bar()}};
    const Event signal_event{
        SignalEvent{
            "AAPL",
            SignalDirection::Long,
            valid_date
        }
    };
    const Event order_event{
        OrderEvent{
            "AAPL",
            OrderSide::Buy,
            10,
            OrderType::Market,
            valid_date
        }
    };
    const Event fill_event{
        FillEvent{
            "AAPL",
            OrderSide::Buy,
            10,
            101.25,
            valid_date,
            0.0
        }
    };

    const auto event_type = [](const Event& event) {
        return std::visit(
            [](const auto& concrete_event) {
                using T = std::decay_t<decltype(concrete_event)>;

                if constexpr (std::is_same_v<T, MarketEvent>) {
                    return 0;
                } else if constexpr (std::is_same_v<T, SignalEvent>) {
                    return 1;
                } else if constexpr (std::is_same_v<T, OrderEvent>) {
                    return 2;
                } else {
                    static_assert(std::is_same_v<T, FillEvent>);
                    return 3;
                }
            },
            event
        );
    };

    REQUIRE(event_type(market_event) == 0);
    REQUIRE(event_type(signal_event) == 1);
    REQUIRE(event_type(order_event) == 2);
    REQUIRE(event_type(fill_event) == 3);
}
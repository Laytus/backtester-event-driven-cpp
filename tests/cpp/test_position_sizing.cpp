#include "backtester/portfolio/order_generator.hpp"
#include "backtester/portfolio/position_sizer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using backtester::events::FillEvent;
using backtester::events::OrderSide;
using backtester::events::OrderType;
using backtester::events::SignalDirection;
using backtester::events::SignalEvent;
using backtester::market::SessionDate;
using backtester::portfolio::OrderGenerator;
using backtester::portfolio::Portfolio;
using backtester::portfolio::PositionQuantity;
using backtester::portfolio::PositionSizer;

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

SignalEvent make_signal(
    std::string symbol,
    SignalDirection direction,
    SessionDate date
) {
    return SignalEvent{
        std::move(symbol),
        direction,
        date
    };
}

FillEvent make_fill(
    std::string symbol,
    OrderSide side,
    std::uint64_t quantity,
    double fill_price,
    SessionDate fill_date,
    double fees = 0.0
) {
    return FillEvent{
        std::move(symbol),
        side,
        quantity,
        fill_price,
        fill_date,
        fees
    };
}

}  // namespace

TEST_CASE(
    "PositionSizer accepts valid target allocations",
    "[position_sizer]"
) {
    REQUIRE_NOTHROW(PositionSizer{1.0});
    REQUIRE_NOTHROW(PositionSizer{0.5});
    REQUIRE_NOTHROW(PositionSizer{0.01});
}

TEST_CASE(
    "PositionSizer rejects invalid target allocations",
    "[position_sizer]"
) {
    SECTION("zero") {
        REQUIRE_THROWS_AS(
            PositionSizer{0.0},
            std::invalid_argument
        );
    }

    SECTION("negative") {
        REQUIRE_THROWS_AS(
            PositionSizer{-0.1},
            std::invalid_argument
        );
    }

    SECTION("greater than one") {
        REQUIRE_THROWS_AS(
            PositionSizer{1.01},
            std::invalid_argument
        );
    }

    SECTION("infinity") {
        REQUIRE_THROWS_AS(
            PositionSizer{
                std::numeric_limits<double>::infinity()
            },
            std::invalid_argument
        );
    }

    SECTION("NaN") {
        REQUIRE_THROWS_AS(
            PositionSizer{
                std::numeric_limits<double>::quiet_NaN()
            },
            std::invalid_argument
        );
    }
}

TEST_CASE(
    "PositionSizer sizes a full Long allocation from flat",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};
    PositionSizer sizer{1.0};

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto target =
        sizer.target_position(
            signal,
            portfolio,
            100.0
        );

    REQUIRE(target == 100);
}

TEST_CASE(
    "PositionSizer sizes a partial Long allocation",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};
    PositionSizer sizer{0.5};

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto target =
        sizer.target_position(
            signal,
            portfolio,
            100.0
        );

    REQUIRE(target == 50);
}

TEST_CASE(
    "PositionSizer calculates reference equity from current executed state",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            20,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    REQUIRE(portfolio.cash() == 8'000.0);
    REQUIRE(portfolio.position_quantity() == 20);

    PositionSizer sizer{0.5};

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto target =
        sizer.target_position(
            signal,
            portfolio,
            100.0
        );

    // Reference equity:
    // 8,000 cash + 20 * 100 = 10,000
    //
    // 50% target:
    // 5,000 / 100 = 50 shares

    REQUIRE(target == 50);
}

TEST_CASE(
    "PositionSizer uses the supplied reference price rather than latest valuation",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            20,
            100.0,
            make_date(2026, 9, 29)
        )
    );

    portfolio.mark_to_market(
        make_date(2026, 9, 30),
        100.0
    );

    PositionSizer sizer{0.5};

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto target =
        sizer.target_position(
            signal,
            portfolio,
            200.0
        );

    // Current state:
    // cash = 8,000
    // position = 20
    //
    // Reference equity at supplied price:
    // 8,000 + 20 * 200 = 12,000
    //
    // 50% target:
    // 6,000 / 200 = 30

    REQUIRE(target == 30);

    REQUIRE(portfolio.latest_equity() == 10'000.0);
}

TEST_CASE(
    "PositionSizer floors fractional target quantities",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};
    PositionSizer sizer{1.0};

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto target =
        sizer.target_position(
            signal,
            portfolio,
            128.0
        );

    REQUIRE(target == 78);
}

TEST_CASE(
    "PositionSizer allows a Long target of zero shares",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 50.0};
    PositionSizer sizer{1.0};

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto target =
        sizer.target_position(
            signal,
            portfolio,
            100.0
        );

    REQUIRE(target == 0);
}

TEST_CASE(
    "PositionSizer maps Flat to a zero target",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            25,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    PositionSizer sizer{1.0};

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Flat,
        make_date(2026, 10, 1)
    );

    REQUIRE(
        sizer.target_position(
            signal,
            portfolio,
            100.0
        ) == 0
    );
}

TEST_CASE(
    "PositionSizer rejects Short under the MVP policy",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};
    PositionSizer sizer{1.0};

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Short,
        make_date(2026, 10, 1)
    );

    REQUIRE_THROWS_AS(
        sizer.target_position(
            signal,
            portfolio,
            100.0
        ),
        std::invalid_argument
    );
}

TEST_CASE(
    "PositionSizer rejects a symbol mismatch",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};
    PositionSizer sizer{1.0};

    const auto signal = make_signal(
        "MSFT",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    REQUIRE_THROWS_AS(
        sizer.target_position(
            signal,
            portfolio,
            100.0
        ),
        std::invalid_argument
    );
}

TEST_CASE(
    "PositionSizer rejects invalid reference prices",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};
    PositionSizer sizer{1.0};

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    SECTION("zero") {
        REQUIRE_THROWS_AS(
            sizer.target_position(
                signal,
                portfolio,
                0.0
            ),
            std::invalid_argument
        );
    }

    SECTION("negative") {
        REQUIRE_THROWS_AS(
            sizer.target_position(
                signal,
                portfolio,
                -1.0
            ),
            std::invalid_argument
        );
    }

    SECTION("infinity") {
        REQUIRE_THROWS_AS(
            sizer.target_position(
                signal,
                portfolio,
                std::numeric_limits<double>::infinity()
            ),
            std::invalid_argument
        );
    }

    SECTION("NaN") {
        REQUIRE_THROWS_AS(
            sizer.target_position(
                signal,
                portfolio,
                std::numeric_limits<double>::quiet_NaN()
            ),
            std::invalid_argument
        );
    }
}

TEST_CASE(
    "PositionSizer rejects non-positive reference equity for Long",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Sell,
            10,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    // cash = 2,000
    // position = -10
    //
    // At reference price 200:
    // equity = 2,000 - 10 * 200 = 0

    PositionSizer sizer{1.0};

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    REQUIRE_THROWS_AS(
        sizer.target_position(
            signal,
            portfolio,
            200.0
        ),
        std::invalid_argument
    );
}

TEST_CASE(
    "PositionSizer rejects target quantities outside PositionQuantity range",
    "[position_sizer]"
) {
    Portfolio portfolio{"AAPL", 1.0e20};
    PositionSizer sizer{1.0};

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    REQUIRE_THROWS_AS(
        sizer.target_position(
            signal,
            portfolio,
            1.0
        ),
        std::overflow_error
    );
}

TEST_CASE(
    "OrderGenerator creates a Buy order when target exceeds current position",
    "[order_generator]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};
    OrderGenerator generator;

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto order =
        generator.generate(
            signal,
            portfolio,
            100
        );

    REQUIRE(order.has_value());

    REQUIRE(order->symbol() == "AAPL");
    REQUIRE(order->side() == OrderSide::Buy);
    REQUIRE(order->quantity() == 100);
    REQUIRE(order->order_type() == OrderType::Market);
    REQUIRE(
        order->submission_date()
        == make_date(2026, 10, 1)
    );
}

TEST_CASE(
    "OrderGenerator buys only the required position delta",
    "[order_generator]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            40,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    OrderGenerator generator;

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto order =
        generator.generate(
            signal,
            portfolio,
            100
        );

    REQUIRE(order.has_value());
    REQUIRE(order->side() == OrderSide::Buy);
    REQUIRE(order->quantity() == 60);
}

TEST_CASE(
    "OrderGenerator sells only the required position delta",
    "[order_generator]"
) {
    Portfolio portfolio{"AAPL", 20'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            120,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    OrderGenerator generator;

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto order =
        generator.generate(
            signal,
            portfolio,
            100
        );

    REQUIRE(order.has_value());
    REQUIRE(order->side() == OrderSide::Sell);
    REQUIRE(order->quantity() == 20);
}

TEST_CASE(
    "OrderGenerator liquidates a positive position for a Flat target",
    "[order_generator]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            50,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    OrderGenerator generator;

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Flat,
        make_date(2026, 10, 1)
    );

    const auto order =
        generator.generate(
            signal,
            portfolio,
            0
        );

    REQUIRE(order.has_value());
    REQUIRE(order->side() == OrderSide::Sell);
    REQUIRE(order->quantity() == 50);
}

TEST_CASE(
    "OrderGenerator returns no order when current position already matches target",
    "[order_generator]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            50,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    OrderGenerator generator;

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto order =
        generator.generate(
            signal,
            portfolio,
            50
        );

    REQUIRE_FALSE(order.has_value());
}

TEST_CASE(
    "OrderGenerator can move a negative current position toward Flat",
    "[order_generator]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Sell,
            20,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    REQUIRE(portfolio.position_quantity() == -20);

    OrderGenerator generator;

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Flat,
        make_date(2026, 10, 1)
    );

    const auto order =
        generator.generate(
            signal,
            portfolio,
            0
        );

    REQUIRE(order.has_value());
    REQUIRE(order->side() == OrderSide::Buy);
    REQUIRE(order->quantity() == 20);
}

TEST_CASE(
    "OrderGenerator can cross from a negative position to a Long target",
    "[order_generator]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Sell,
            20,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    OrderGenerator generator;

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto order =
        generator.generate(
            signal,
            portfolio,
            100
        );

    REQUIRE(order.has_value());
    REQUIRE(order->side() == OrderSide::Buy);
    REQUIRE(order->quantity() == 120);
}

TEST_CASE(
    "OrderGenerator rejects negative target positions",
    "[order_generator]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};
    OrderGenerator generator;

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Short,
        make_date(2026, 10, 1)
    );

    REQUIRE_THROWS_AS(
        generator.generate(
            signal,
            portfolio,
            -1
        ),
        std::invalid_argument
    );
}

TEST_CASE(
    "OrderGenerator rejects a symbol mismatch",
    "[order_generator]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};
    OrderGenerator generator;

    const auto signal = make_signal(
        "MSFT",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    REQUIRE_THROWS_AS(
        generator.generate(
            signal,
            portfolio,
            100
        ),
        std::invalid_argument
    );
}

TEST_CASE(
    "OrderGenerator rejects a Buy delta outside PositionQuantity range",
    "[order_generator]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    const auto maximum_quantity =
        static_cast<std::uint64_t>(
            std::numeric_limits<PositionQuantity>::max()
        );

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Sell,
            maximum_quantity,
            1.0,
            make_date(2026, 9, 29)
        )
    );

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Sell,
            1,
            1.0,
            make_date(2026, 9, 30)
        )
    );

    REQUIRE(
        portfolio.position_quantity()
        == std::numeric_limits<PositionQuantity>::min()
    );

    OrderGenerator generator;

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Flat,
        make_date(2026, 10, 1)
    );

    REQUIRE_THROWS_AS(
        generator.generate(
            signal,
            portfolio,
            0
        ),
        std::overflow_error
    );
}

TEST_CASE(
    "Position sizing and order generation do not mutate Portfolio",
    "[position_sizer][order_generator]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            20,
            100.0,
            make_date(2026, 9, 29)
        )
    );

    portfolio.mark_to_market(
        make_date(2026, 9, 30),
        105.0
    );

    const double cash_before =
        portfolio.cash();

    const auto position_before =
        portfolio.position_quantity();

    const auto mark_before =
        portfolio.latest_mark_price();

    const double market_value_before =
        portfolio.latest_market_value();

    const double equity_before =
        portfolio.latest_equity();

    const auto snapshots_before =
        portfolio.snapshots().size();

    PositionSizer sizer{0.5};
    OrderGenerator generator;

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto target =
        sizer.target_position(
            signal,
            portfolio,
            110.0
        );

    const auto order =
        generator.generate(
            signal,
            portfolio,
            target
        );

    REQUIRE(order.has_value());

    REQUIRE(portfolio.cash() == cash_before);
    REQUIRE(
        portfolio.position_quantity()
        == position_before
    );
    REQUIRE(
        portfolio.latest_mark_price()
        == mark_before
    );
    REQUIRE(
        portfolio.latest_market_value()
        == market_value_before
    );
    REQUIRE(
        portfolio.latest_equity()
        == equity_before
    );
    REQUIRE(
        portfolio.snapshots().size()
        == snapshots_before
    );
}

TEST_CASE(
    "Position sizing and order generation are deterministic for identical inputs",
    "[position_sizer][order_generator]"
) {
    Portfolio first{"AAPL", 10'000.0};
    Portfolio second{"AAPL", 10'000.0};

    const auto fill = make_fill(
        "AAPL",
        OrderSide::Buy,
        20,
        100.0,
        make_date(2026, 9, 30)
    );

    first.apply_fill(fill);
    second.apply_fill(fill);

    PositionSizer first_sizer{0.5};
    PositionSizer second_sizer{0.5};

    OrderGenerator first_generator;
    OrderGenerator second_generator;

    const auto signal = make_signal(
        "AAPL",
        SignalDirection::Long,
        make_date(2026, 10, 1)
    );

    const auto first_target =
        first_sizer.target_position(
            signal,
            first,
            100.0
        );

    const auto second_target =
        second_sizer.target_position(
            signal,
            second,
            100.0
        );

    REQUIRE(first_target == second_target);

    const auto first_order =
        first_generator.generate(
            signal,
            first,
            first_target
        );

    const auto second_order =
        second_generator.generate(
            signal,
            second,
            second_target
        );

    REQUIRE(
        first_order.has_value()
        == second_order.has_value()
    );

    REQUIRE(first_order.has_value());

    REQUIRE(
        first_order->symbol()
        == second_order->symbol()
    );

    REQUIRE(
        first_order->side()
        == second_order->side()
    );

    REQUIRE(
        first_order->quantity()
        == second_order->quantity()
    );

    REQUIRE(
        first_order->order_type()
        == second_order->order_type()
    );

    REQUIRE(
        first_order->submission_date()
        == second_order->submission_date()
    );
}
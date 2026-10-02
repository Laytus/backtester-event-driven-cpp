#include "backtester/execution/execution_handler.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using backtester::events::FillEvent;
using backtester::events::OrderEvent;
using backtester::events::OrderSide;
using backtester::events::OrderType;
using backtester::execution::ExecutionHandler;
using backtester::market::SessionDate;
using backtester::portfolio::Portfolio;

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

OrderEvent make_order(
    std::string symbol,
    OrderSide side,
    std::uint64_t quantity,
    SessionDate submission_date
) {
    return OrderEvent{
        std::move(symbol),
        side,
        quantity,
        OrderType::Market,
        submission_date
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

static_assert(
    !std::is_copy_constructible_v<ExecutionHandler>
);

static_assert(
    !std::is_copy_assignable_v<ExecutionHandler>
);

static_assert(
    std::is_move_constructible_v<ExecutionHandler>
);

static_assert(
    std::is_move_assignable_v<ExecutionHandler>
);

TEST_CASE(
    "ExecutionHandler starts without a pending order",
    "[execution_handler]"
) {
    ExecutionHandler handler;

    REQUIRE_FALSE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler stores one submitted order",
    "[execution_handler]"
) {
    ExecutionHandler handler;

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            make_date(2026, 10, 1)
        )
    );

    REQUIRE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler rejects a second order while one is pending",
    "[execution_handler]"
) {
    ExecutionHandler handler;

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            make_date(2026, 10, 1)
        )
    );

    REQUIRE_THROWS_AS(
        handler.submit(
            make_order(
                "AAPL",
                OrderSide::Buy,
                50,
                make_date(2026, 10, 2)
            )
        ),
        std::logic_error
    );

    REQUIRE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler returns no fill when no order is pending",
    "[execution_handler]"
) {
    ExecutionHandler handler;
    Portfolio portfolio{"AAPL", 10'000.0};

    const auto fill =
        handler.execute_on_open(
            make_date(2026, 10, 2),
            100.0,
            portfolio
        );

    REQUIRE_FALSE(fill.has_value());
    REQUIRE_FALSE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler does not execute an order in its submission session",
    "[execution_handler]"
) {
    ExecutionHandler handler;
    Portfolio portfolio{"AAPL", 10'000.0};

    const auto date =
        make_date(2026, 10, 1);

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            date
        )
    );

    const auto fill =
        handler.execute_on_open(
            date,
            100.0,
            portfolio
        );

    REQUIRE_FALSE(fill.has_value());
    REQUIRE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler rejects an execution date before submission",
    "[execution_handler]"
) {
    ExecutionHandler handler;
    Portfolio portfolio{"AAPL", 10'000.0};

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            make_date(2026, 10, 2)
        )
    );

    REQUIRE_THROWS_AS(
        handler.execute_on_open(
            make_date(2026, 10, 1),
            100.0,
            portfolio
        ),
        std::logic_error
    );

    REQUIRE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler rejects invalid open inputs without consuming the order",
    "[execution_handler]"
) {
    ExecutionHandler handler;
    Portfolio portfolio{"AAPL", 10'000.0};

    const auto submission_date =
        make_date(2026, 10, 1);

    SECTION("invalid calendar date") {
        handler.submit(
            make_order(
                "AAPL",
                OrderSide::Buy,
                100,
                submission_date
            )
        );

        REQUIRE_THROWS_AS(
            handler.execute_on_open(
                make_date(2026, 2, 30),
                100.0,
                portfolio
            ),
            std::invalid_argument
        );

        REQUIRE(handler.has_pending_order());
    }

    SECTION("zero price") {
        handler.submit(
            make_order(
                "AAPL",
                OrderSide::Buy,
                100,
                submission_date
            )
        );

        REQUIRE_THROWS_AS(
            handler.execute_on_open(
                make_date(2026, 10, 2),
                0.0,
                portfolio
            ),
            std::invalid_argument
        );

        REQUIRE(handler.has_pending_order());
    }

    SECTION("negative price") {
        handler.submit(
            make_order(
                "AAPL",
                OrderSide::Buy,
                100,
                submission_date
            )
        );

        REQUIRE_THROWS_AS(
            handler.execute_on_open(
                make_date(2026, 10, 2),
                -100.0,
                portfolio
            ),
            std::invalid_argument
        );

        REQUIRE(handler.has_pending_order());
    }

    SECTION("infinite price") {
        handler.submit(
            make_order(
                "AAPL",
                OrderSide::Buy,
                100,
                submission_date
            )
        );

        REQUIRE_THROWS_AS(
            handler.execute_on_open(
                make_date(2026, 10, 2),
                std::numeric_limits<double>::infinity(),
                portfolio
            ),
            std::invalid_argument
        );

        REQUIRE(handler.has_pending_order());
    }

    SECTION("NaN price") {
        handler.submit(
            make_order(
                "AAPL",
                OrderSide::Buy,
                100,
                submission_date
            )
        );

        REQUIRE_THROWS_AS(
            handler.execute_on_open(
                make_date(2026, 10, 2),
                std::numeric_limits<double>::quiet_NaN(),
                portfolio
            ),
            std::invalid_argument
        );

        REQUIRE(handler.has_pending_order());
    }
}

TEST_CASE(
    "ExecutionHandler rejects a Portfolio symbol mismatch without consuming the order",
    "[execution_handler]"
) {
    ExecutionHandler handler;
    Portfolio portfolio{"MSFT", 10'000.0};

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            make_date(2026, 10, 1)
        )
    );

    REQUIRE_THROWS_AS(
        handler.execute_on_open(
            make_date(2026, 10, 2),
            100.0,
            portfolio
        ),
        std::invalid_argument
    );

    REQUIRE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler fully executes an affordable Buy order at the next open",
    "[execution_handler]"
) {
    ExecutionHandler handler;
    Portfolio portfolio{"AAPL", 10'000.0};

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            80,
            make_date(2026, 10, 1)
        )
    );

    const auto fill =
        handler.execute_on_open(
            make_date(2026, 10, 2),
            100.0,
            portfolio
        );

    REQUIRE(fill.has_value());

    REQUIRE(fill->symbol() == "AAPL");
    REQUIRE(fill->side() == OrderSide::Buy);
    REQUIRE(fill->quantity() == 80);
    REQUIRE(fill->fill_price() == 100.0);
    REQUIRE(
        fill->fill_date()
        == make_date(2026, 10, 2)
    );
    REQUIRE(fill->fees() == 0.0);

    REQUIRE_FALSE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler clips a Buy order to execution-time buying power",
    "[execution_handler]"
) {
    ExecutionHandler handler;
    Portfolio portfolio{"AAPL", 10'000.0};

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            make_date(2026, 10, 1)
        )
    );

    const auto fill =
        handler.execute_on_open(
            make_date(2026, 10, 2),
            110.0,
            portfolio
        );

    REQUIRE(fill.has_value());
    REQUIRE(fill->side() == OrderSide::Buy);

    // floor(10,000 / 110) = 90
    REQUIRE(fill->quantity() == 90);
    REQUIRE(fill->fill_price() == 110.0);

    REQUIRE_FALSE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler clears a clipped order without retaining a residual",
    "[execution_handler]"
) {
    ExecutionHandler handler;
    Portfolio portfolio{"AAPL", 10'000.0};

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            make_date(2026, 10, 1)
        )
    );

    const auto first_attempt =
        handler.execute_on_open(
            make_date(2026, 10, 2),
            110.0,
            portfolio
        );

    REQUIRE(first_attempt.has_value());
    REQUIRE(first_attempt->quantity() == 90);

    REQUIRE_FALSE(handler.has_pending_order());

    const auto second_attempt =
        handler.execute_on_open(
            make_date(2026, 10, 3),
            90.0,
            portfolio
        );

    REQUIRE_FALSE(second_attempt.has_value());
}

TEST_CASE(
    "ExecutionHandler produces no fill when Buy buying power is zero",
    "[execution_handler]"
) {
    Portfolio portfolio{"AAPL", 100.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            1,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    REQUIRE(portfolio.cash() == 0.0);

    ExecutionHandler handler;

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            10,
            make_date(2026, 10, 1)
        )
    );

    const auto fill =
        handler.execute_on_open(
            make_date(2026, 10, 2),
            100.0,
            portfolio
        );

    REQUIRE_FALSE(fill.has_value());
    REQUIRE_FALSE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler treats negative cash as zero Buy buying power",
    "[execution_handler]"
) {
    Portfolio portfolio{"AAPL", 100.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            2,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    REQUIRE(portfolio.cash() == -100.0);

    ExecutionHandler handler;

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            10,
            make_date(2026, 10, 1)
        )
    );

    const auto fill =
        handler.execute_on_open(
            make_date(2026, 10, 2),
            100.0,
            portfolio
        );

    REQUIRE_FALSE(fill.has_value());
    REQUIRE_FALSE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler fully executes Sell orders without cash clipping",
    "[execution_handler]"
) {
    ExecutionHandler handler;
    Portfolio portfolio{"AAPL", 100.0};

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Sell,
            250,
            make_date(2026, 10, 1)
        )
    );

    const auto fill =
        handler.execute_on_open(
            make_date(2026, 10, 2),
            125.0,
            portfolio
        );

    REQUIRE(fill.has_value());

    REQUIRE(fill->side() == OrderSide::Sell);
    REQUIRE(fill->quantity() == 250);
    REQUIRE(fill->fill_price() == 125.0);
    REQUIRE(fill->fees() == 0.0);

    REQUIRE_FALSE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler uses current Portfolio cash for Buy approval",
    "[execution_handler]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    portfolio.mark_to_market(
        make_date(2026, 9, 29),
        100.0
    );

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            50,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    REQUIRE(portfolio.cash() == 5'000.0);
    REQUIRE(portfolio.latest_equity() == 10'000.0);

    ExecutionHandler handler;

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            make_date(2026, 10, 1)
        )
    );

    const auto fill =
        handler.execute_on_open(
            make_date(2026, 10, 2),
            100.0,
            portfolio
        );

    REQUIRE(fill.has_value());

    // Current cash is 5,000, so only 50 shares
    // are affordable even though latest_equity is 10,000.
    REQUIRE(fill->quantity() == 50);
}

TEST_CASE(
    "ExecutionHandler does not mutate Portfolio during execution",
    "[execution_handler]"
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

    ExecutionHandler handler;

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            50,
            make_date(2026, 10, 1)
        )
    );

    const auto fill =
        handler.execute_on_open(
            make_date(2026, 10, 2),
            110.0,
            portfolio
        );

    REQUIRE(fill.has_value());

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
    "ExecutionHandler safely handles extremely large affordable quantities",
    "[execution_handler]"
) {
    Portfolio portfolio{
        "AAPL",
        std::numeric_limits<double>::max()
    };

    ExecutionHandler handler;

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            make_date(2026, 10, 1)
        )
    );

    const auto fill =
        handler.execute_on_open(
            make_date(2026, 10, 2),
            std::numeric_limits<double>::min(),
            portfolio
        );

    REQUIRE(fill.has_value());
    REQUIRE(fill->quantity() == 100);
}

TEST_CASE(
    "ExecutionHandler discard clears a pending order",
    "[execution_handler]"
) {
    ExecutionHandler handler;

    handler.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            make_date(2026, 10, 1)
        )
    );

    REQUIRE(handler.has_pending_order());

    handler.discard_pending();

    REQUIRE_FALSE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler discard is idempotent",
    "[execution_handler]"
) {
    ExecutionHandler handler;

    REQUIRE_FALSE(handler.has_pending_order());

    handler.discard_pending();
    handler.discard_pending();

    REQUIRE_FALSE(handler.has_pending_order());
}

TEST_CASE(
    "ExecutionHandler produces deterministic results for identical inputs",
    "[execution_handler]"
) {
    ExecutionHandler first;
    ExecutionHandler second;

    Portfolio first_portfolio{"AAPL", 10'000.0};
    Portfolio second_portfolio{"AAPL", 10'000.0};

    first.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            make_date(2026, 10, 1)
        )
    );

    second.submit(
        make_order(
            "AAPL",
            OrderSide::Buy,
            100,
            make_date(2026, 10, 1)
        )
    );

    const auto first_fill =
        first.execute_on_open(
            make_date(2026, 10, 2),
            110.0,
            first_portfolio
        );

    const auto second_fill =
        second.execute_on_open(
            make_date(2026, 10, 2),
            110.0,
            second_portfolio
        );

    REQUIRE(
        first_fill.has_value()
        == second_fill.has_value()
    );

    REQUIRE(first_fill.has_value());

    REQUIRE(
        first_fill->symbol()
        == second_fill->symbol()
    );

    REQUIRE(
        first_fill->side()
        == second_fill->side()
    );

    REQUIRE(
        first_fill->quantity()
        == second_fill->quantity()
    );

    REQUIRE(
        first_fill->fill_price()
        == second_fill->fill_price()
    );

    REQUIRE(
        first_fill->fill_date()
        == second_fill->fill_date()
    );

    REQUIRE(
        first_fill->fees()
        == second_fill->fees()
    );

    REQUIRE(
        first.has_pending_order()
        == second.has_pending_order()
    );
}
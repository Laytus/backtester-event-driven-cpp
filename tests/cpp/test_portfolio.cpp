#include "backtester/portfolio/portfolio.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {

using backtester::events::FillEvent;
using backtester::events::OrderSide;
using backtester::market::SessionDate;
using backtester::portfolio::Portfolio;
using backtester::portfolio::PortfolioSnapshot;
using backtester::portfolio::PositionQuantity;

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

static_assert(
    !std::is_copy_constructible_v<Portfolio>
);

static_assert(
    !std::is_copy_assignable_v<Portfolio>
);

static_assert(
    std::is_move_constructible_v<Portfolio>
);

static_assert(
    std::is_move_assignable_v<Portfolio>
);

}  // namespace

TEST_CASE(
    "Portfolio starts with initial capital and a flat position",
    "[portfolio]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    REQUIRE(portfolio.symbol() == "AAPL");
    REQUIRE(portfolio.initial_capital() == 10'000.0);
    REQUIRE(portfolio.cash() == 10'000.0);
    REQUIRE(portfolio.position_quantity() == 0);

    REQUIRE_FALSE(portfolio.latest_mark_price().has_value());
    REQUIRE(portfolio.latest_market_value() == 0.0);
    REQUIRE(portfolio.latest_equity() == 10'000.0);

    REQUIRE(portfolio.snapshots().empty());
}

TEST_CASE(
    "Portfolio rejects invalid initial configuration",
    "[portfolio]"
) {
    SECTION("empty symbol") {
        REQUIRE_THROWS_AS(
            Portfolio("", 10'000.0),
            std::invalid_argument
        );
    }

    SECTION("zero initial capital") {
        REQUIRE_THROWS_AS(
            Portfolio("AAPL", 0.0),
            std::invalid_argument
        );
    }

    SECTION("negative initial capital") {
        REQUIRE_THROWS_AS(
            Portfolio("AAPL", -1.0),
            std::invalid_argument
        );
    }

    SECTION("infinite initial capital") {
        REQUIRE_THROWS_AS(
            Portfolio(
                "AAPL",
                std::numeric_limits<double>::infinity()
            ),
            std::invalid_argument
        );
    }

    SECTION("NaN initial capital") {
        REQUIRE_THROWS_AS(
            Portfolio(
                "AAPL",
                std::numeric_limits<double>::quiet_NaN()
            ),
            std::invalid_argument
        );
    }
}

TEST_CASE(
    "PortfolioSnapshot validates its local state",
    "[portfolio][snapshot]"
) {
    const auto valid_date = make_date(2026, 9, 30);

    SECTION("invalid date") {
        REQUIRE_THROWS_AS(
            PortfolioSnapshot(
                make_date(2026, 2, 30),
                1'000.0,
                0,
                0.0,
                1'000.0
            ),
            std::invalid_argument
        );
    }

    SECTION("non-finite cash") {
        REQUIRE_THROWS_AS(
            PortfolioSnapshot(
                valid_date,
                std::numeric_limits<double>::infinity(),
                0,
                0.0,
                1'000.0
            ),
            std::invalid_argument
        );
    }

    SECTION("non-finite market value") {
        REQUIRE_THROWS_AS(
            PortfolioSnapshot(
                valid_date,
                1'000.0,
                0,
                std::numeric_limits<double>::infinity(),
                1'000.0
            ),
            std::invalid_argument
        );
    }

    SECTION("non-finite equity") {
        REQUIRE_THROWS_AS(
            PortfolioSnapshot(
                valid_date,
                1'000.0,
                0,
                0.0,
                std::numeric_limits<double>::infinity()
            ),
            std::invalid_argument
        );
    }
}

TEST_CASE(
    "Portfolio applies a buy fill to cash and position",
    "[portfolio]"
) {
    Portfolio portfolio{"AAPL", 10'000.0};

    const FillEvent fill = make_fill(
        "AAPL",
        OrderSide::Buy,
        10,
        100.0,
        make_date(2026, 9, 30),
        2.0
    );

    portfolio.apply_fill(fill);

    REQUIRE(portfolio.position_quantity() == 10);
    REQUIRE(portfolio.cash() == 8'998.0);
}

TEST_CASE(
    "Portfolio applies a sell fill to cash and position",
    "[portfolio]"
) {
    Portfolio portfolio{"AAPL", 2'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            10,
            100.0,
            make_date(2026, 9, 29)
        )
    );

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Sell,
            4,
            110.0,
            make_date(2026, 9, 30),
            2.0
        )
    );

    REQUIRE(portfolio.position_quantity() == 6);
    REQUIRE(portfolio.cash() == 1'438.0);
}

TEST_CASE(
    "Portfolio accounting supports a negative signed position",
    "[portfolio]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Sell,
            3,
            100.0,
            make_date(2026, 9, 30),
            2.0
        )
    );

    REQUIRE(portfolio.position_quantity() == -3);
    REQUIRE(portfolio.cash() == 1'298.0);
}

TEST_CASE(
    "Portfolio rejects a fill for another symbol without changing state",
    "[portfolio]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    const auto fill = make_fill(
        "MSFT",
        OrderSide::Buy,
        5,
        100.0,
        make_date(2026, 9, 30)
    );

    REQUIRE_THROWS_AS(
        portfolio.apply_fill(fill),
        std::invalid_argument
    );

    REQUIRE(portfolio.cash() == 1'000.0);
    REQUIRE(portfolio.position_quantity() == 0);
}

TEST_CASE(
    "Portfolio fill accounting does not update valuation state",
    "[portfolio]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    portfolio.mark_to_market(
        make_date(2026, 9, 29),
        100.0
    );

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            5,
            100.0,
            make_date(2026, 9, 30)
        )
    );

    REQUIRE(portfolio.cash() == 500.0);
    REQUIRE(portfolio.position_quantity() == 5);

    REQUIRE(portfolio.latest_mark_price().has_value());
    REQUIRE(*portfolio.latest_mark_price() == 100.0);
    REQUIRE(portfolio.latest_market_value() == 0.0);
    REQUIRE(portfolio.latest_equity() == 1'000.0);

    REQUIRE(portfolio.snapshots().size() == 1);
}

TEST_CASE(
    "Portfolio marks executed state to market and records a snapshot",
    "[portfolio][snapshot]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            5,
            100.0,
            make_date(2026, 9, 29),
            2.0
        )
    );

    const auto session_date = make_date(2026, 9, 30);

    portfolio.mark_to_market(
        session_date,
        110.0
    );

    REQUIRE(portfolio.cash() == 498.0);
    REQUIRE(portfolio.position_quantity() == 5);

    REQUIRE(portfolio.latest_mark_price().has_value());
    REQUIRE(*portfolio.latest_mark_price() == 110.0);
    REQUIRE(portfolio.latest_market_value() == 550.0);
    REQUIRE(portfolio.latest_equity() == 1'048.0);

    REQUIRE(portfolio.snapshots().size() == 1);

    const auto& snapshot = portfolio.snapshots().front();

    REQUIRE(snapshot.session_date() == session_date);
    REQUIRE(snapshot.cash() == 498.0);
    REQUIRE(snapshot.position_quantity() == 5);
    REQUIRE(snapshot.market_value() == 550.0);
    REQUIRE(snapshot.equity() == 1'048.0);
}

TEST_CASE(
    "Portfolio valuation does not modify cash or position",
    "[portfolio][snapshot]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    portfolio.apply_fill(
        make_fill(
            "AAPL",
            OrderSide::Buy,
            5,
            100.0,
            make_date(2026, 9, 29)
        )
    );

    const double cash_before = portfolio.cash();
    const auto position_before = portfolio.position_quantity();

    portfolio.mark_to_market(
        make_date(2026, 9, 30),
        120.0
    );

    REQUIRE(portfolio.cash() == cash_before);
    REQUIRE(portfolio.position_quantity() == position_before);
}

TEST_CASE(
    "Portfolio snapshots preserve chronological valuation history",
    "[portfolio][snapshot]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    portfolio.mark_to_market(
        make_date(2026, 9, 28),
        100.0
    );

    portfolio.mark_to_market(
        make_date(2026, 9, 29),
        101.0
    );

    portfolio.mark_to_market(
        make_date(2026, 9, 30),
        102.0
    );

    REQUIRE(portfolio.snapshots().size() == 3);

    REQUIRE(
        portfolio.snapshots()[0].session_date()
        == make_date(2026, 9, 28)
    );

    REQUIRE(
        portfolio.snapshots()[1].session_date()
        == make_date(2026, 9, 29)
    );

    REQUIRE(
        portfolio.snapshots()[2].session_date()
        == make_date(2026, 9, 30)
    );
}

TEST_CASE(
    "Portfolio rejects duplicate and backwards snapshot dates",
    "[portfolio][snapshot]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    portfolio.mark_to_market(
        make_date(2026, 9, 30),
        100.0
    );

    REQUIRE_THROWS_AS(
        portfolio.mark_to_market(
            make_date(2026, 9, 30),
            101.0
        ),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        portfolio.mark_to_market(
            make_date(2026, 9, 29),
            101.0
        ),
        std::invalid_argument
    );

    REQUIRE(portfolio.snapshots().size() == 1);
    REQUIRE(*portfolio.latest_mark_price() == 100.0);
    REQUIRE(portfolio.latest_equity() == 1'000.0);
}

TEST_CASE(
    "Portfolio rejects invalid mark-to-market inputs",
    "[portfolio][snapshot]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    SECTION("invalid session date") {
        REQUIRE_THROWS_AS(
            portfolio.mark_to_market(
                make_date(2026, 2, 30),
                100.0
            ),
            std::invalid_argument
        );
    }

    SECTION("zero mark price") {
        REQUIRE_THROWS_AS(
            portfolio.mark_to_market(
                make_date(2026, 9, 30),
                0.0
            ),
            std::invalid_argument
        );
    }

    SECTION("negative mark price") {
        REQUIRE_THROWS_AS(
            portfolio.mark_to_market(
                make_date(2026, 9, 30),
                -1.0
            ),
            std::invalid_argument
        );
    }

    SECTION("non-finite mark price") {
        REQUIRE_THROWS_AS(
            portfolio.mark_to_market(
                make_date(2026, 9, 30),
                std::numeric_limits<double>::infinity()
            ),
            std::invalid_argument
        );
    }
}

TEST_CASE(
    "Portfolio rejects fill quantities outside PositionQuantity range",
    "[portfolio]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    const auto invalid_quantity =
        static_cast<std::uint64_t>(
            std::numeric_limits<PositionQuantity>::max()
        ) + 1ULL;

    const auto fill = make_fill(
        "AAPL",
        OrderSide::Buy,
        invalid_quantity,
        1.0,
        make_date(2026, 9, 30)
    );

    REQUIRE_THROWS_AS(
        portfolio.apply_fill(fill),
        std::overflow_error
    );

    REQUIRE(portfolio.cash() == 1'000.0);
    REQUIRE(portfolio.position_quantity() == 0);
}

TEST_CASE(
    "Portfolio prevents signed position overflow and underflow",
    "[portfolio]"
) {
    const auto maximum_quantity =
        static_cast<std::uint64_t>(
            std::numeric_limits<PositionQuantity>::max()
        );

    SECTION("buy overflow") {
        Portfolio portfolio{"AAPL", 1'000.0};

        portfolio.apply_fill(
            make_fill(
                "AAPL",
                OrderSide::Buy,
                maximum_quantity,
                1.0,
                make_date(2026, 9, 29)
            )
        );

        const double cash_before = portfolio.cash();

        REQUIRE_THROWS_AS(
            portfolio.apply_fill(
                make_fill(
                    "AAPL",
                    OrderSide::Buy,
                    1,
                    1.0,
                    make_date(2026, 9, 30)
                )
            ),
            std::overflow_error
        );

        REQUIRE(
            portfolio.position_quantity()
            == std::numeric_limits<PositionQuantity>::max()
        );

        REQUIRE(portfolio.cash() == cash_before);
    }

    SECTION("sell underflow") {
        Portfolio portfolio{"AAPL", 1'000.0};

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

        const double cash_before = portfolio.cash();

        REQUIRE_THROWS_AS(
            portfolio.apply_fill(
                make_fill(
                    "AAPL",
                    OrderSide::Sell,
                    1,
                    1.0,
                    make_date(2026, 10, 1)
                )
            ),
            std::overflow_error
        );

        REQUIRE(
            portfolio.position_quantity()
            == std::numeric_limits<PositionQuantity>::min()
        );

        REQUIRE(portfolio.cash() == cash_before);
    }
}

TEST_CASE(
    "Portfolio remains unchanged when fill notional overflows",
    "[portfolio]"
) {
    Portfolio portfolio{"AAPL", 1'000.0};

    const auto fill = make_fill(
        "AAPL",
        OrderSide::Buy,
        2,
        std::numeric_limits<double>::max(),
        make_date(2026, 9, 30)
    );

    REQUIRE_THROWS_AS(
        portfolio.apply_fill(fill),
        std::overflow_error
    );

    REQUIRE(portfolio.cash() == 1'000.0);
    REQUIRE(portfolio.position_quantity() == 0);
}

TEST_CASE(
    "Portfolio produces deterministic state for identical operations",
    "[portfolio]"
) {
    Portfolio first{"AAPL", 2'000.0};
    Portfolio second{"AAPL", 2'000.0};

    const auto fill = make_fill(
        "AAPL",
        OrderSide::Buy,
        10,
        100.0,
        make_date(2026, 9, 29),
        2.0
    );

    first.apply_fill(fill);
    second.apply_fill(fill);

    const auto valuation_date = make_date(2026, 9, 30);

    first.mark_to_market(
        valuation_date,
        110.0
    );

    second.mark_to_market(
        valuation_date,
        110.0
    );

    REQUIRE(first.cash() == second.cash());
    REQUIRE(
        first.position_quantity()
        == second.position_quantity()
    );

    REQUIRE(
        first.latest_mark_price()
        == second.latest_mark_price()
    );

    REQUIRE(
        first.latest_market_value()
        == second.latest_market_value()
    );

    REQUIRE(
        first.latest_equity()
        == second.latest_equity()
    );

    REQUIRE(
        first.snapshots().size()
        == second.snapshots().size()
    );

    REQUIRE(
        first.snapshots().front().session_date()
        == second.snapshots().front().session_date()
    );

    REQUIRE(
        first.snapshots().front().cash()
        == second.snapshots().front().cash()
    );

    REQUIRE(
        first.snapshots().front().position_quantity()
        == second.snapshots().front().position_quantity()
    );

    REQUIRE(
        first.snapshots().front().market_value()
        == second.snapshots().front().market_value()
    );

    REQUIRE(
        first.snapshots().front().equity()
        == second.snapshots().front().equity()
    );
}
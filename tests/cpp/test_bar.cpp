#include "backtester/market/bar.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using backtester::market::Bar;

const Bar::SessionDate valid_date{
    std::chrono::year{2026},
    std::chrono::month{9},
    std::chrono::day{10}
};

}  // namespace

TEST_CASE("Bar stores a valid daily observation")
{
    const Bar bar{
        valid_date,
        "SPY",
        650.0,
        655.0,
        648.0,
        653.0,
        std::uint64_t{75'000'000}
    };

    REQUIRE(bar.session_date() == valid_date);
    REQUIRE(bar.symbol() == "SPY");
    REQUIRE(bar.open() == 650.0);
    REQUIRE(bar.high() == 655.0);
    REQUIRE(bar.low() == 648.0);
    REQUIRE(bar.close() == 653.0);
    REQUIRE(bar.volume() == 75'000'000);
}

TEST_CASE("Bar rejects an invalid calendar date")
{
    const Bar::SessionDate invalid_date{
        std::chrono::year{2026},
        std::chrono::month{2},
        std::chrono::day{30}
    };

    REQUIRE_THROWS_AS(
        Bar(invalid_date, "SPY", 100.0, 105.0, 95.0, 102.0, 1'000),
        std::invalid_argument
    );
}

TEST_CASE("Bar rejects an empty symbol")
{
    REQUIRE_THROWS_AS(
        Bar(valid_date, "", 100.0, 105.0, 95.0, 102.0, 1'000),
        std::invalid_argument
    );
}

TEST_CASE("Bar rejects non-finite prices")
{
    SECTION("NaN")
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();

        REQUIRE_THROWS_AS(
            Bar(valid_date, "SPY", nan, 105.0, 95.0, 102.0, 1'000),
            std::invalid_argument
        );
    }

    SECTION("positive infinity")
    {
        const double infinity = std::numeric_limits<double>::infinity();

        REQUIRE_THROWS_AS(
            Bar(valid_date, "SPY", 100.0, infinity, 95.0, 102.0, 1'000),
            std::invalid_argument
        );
    }
}

TEST_CASE("Bar rejects non-positive prices")
{
    SECTION("zero price")
    {
        REQUIRE_THROWS_AS(
            Bar(valid_date, "SPY", 0.0, 105.0, 95.0, 102.0, 1'000),
            std::invalid_argument
        );
    }

    SECTION("negative price")
    {
        REQUIRE_THROWS_AS(
            Bar(valid_date, "SPY", 100.0, 105.0, -1.0, 102.0, 1'000),
            std::invalid_argument
        );
    }
}

TEST_CASE("Bar rejects invalid OHLC relationships")
{
    SECTION("high lower than low")
    {
        REQUIRE_THROWS_AS(
            Bar(valid_date, "SPY", 100.0, 94.0, 95.0, 93.0, 1'000),
            std::invalid_argument
        );
    }

    SECTION("high lower than open")
    {
        REQUIRE_THROWS_AS(
            Bar(valid_date, "SPY", 106.0, 105.0, 95.0, 102.0, 1'000),
            std::invalid_argument
        );
    }

    SECTION("high lower than close")
    {
        REQUIRE_THROWS_AS(
            Bar(valid_date, "SPY", 100.0, 105.0, 95.0, 106.0, 1'000),
            std::invalid_argument
        );
    }

    SECTION("low higher than open")
    {
        REQUIRE_THROWS_AS(
            Bar(valid_date, "SPY", 94.0, 105.0, 95.0, 100.0, 1'000),
            std::invalid_argument
        );
    }

    SECTION("low higher than close")
    {
        REQUIRE_THROWS_AS(
            Bar(valid_date, "SPY", 100.0, 105.0, 95.0, 94.0, 1'000),
            std::invalid_argument
        );
    }
}

TEST_CASE("Bar allows zero volume")
{
    REQUIRE_NOTHROW(
        Bar(valid_date, "SPY", 100.0, 105.0, 95.0, 102.0, 0)
    );
}
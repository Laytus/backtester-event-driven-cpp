#include "backtester/data/historical_data_feed.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

using backtester::data::HistoricalDataFeed;
using backtester::market::Bar;

Bar make_bar(
    int year,
    unsigned month,
    unsigned day,
    double close
)
{
    return Bar{
        Bar::SessionDate{
            std::chrono::year{year},
            std::chrono::month{month},
            std::chrono::day{day}
        },
        "SPY",
        close,
        close + 1.0,
        close - 1.0,
        close,
        std::uint64_t{1'000'000}
    };
}

}  // namespace

TEST_CASE("HistoricalDataFeed delivers bars in input order")
{
    std::vector<Bar> bars;

    bars.push_back(make_bar(2026, 8, 24, 100.0));
    bars.push_back(make_bar(2026, 8, 25, 101.0));
    bars.push_back(make_bar(2026, 8, 26, 102.0));

    HistoricalDataFeed feed{std::move(bars)};

    const auto first = feed.next();
    const auto second = feed.next();
    const auto third = feed.next();

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(third.has_value());

    REQUIRE(
        first->session_date()
        == Bar::SessionDate{
            std::chrono::year{2026},
            std::chrono::month{8},
            std::chrono::day{24}
        }
    );

    REQUIRE(
        second->session_date()
        == Bar::SessionDate{
            std::chrono::year{2026},
            std::chrono::month{8},
            std::chrono::day{25}
        }
    );

    REQUIRE(
        third->session_date()
        == Bar::SessionDate{
            std::chrono::year{2026},
            std::chrono::month{8},
            std::chrono::day{26}
        }
    );

    REQUIRE(first->close() == 100.0);
    REQUIRE(second->close() == 101.0);
    REQUIRE(third->close() == 102.0);
}

TEST_CASE("HistoricalDataFeed delivers each bar only once")
{
    std::vector<Bar> bars;

    bars.push_back(make_bar(2026, 8, 24, 100.0));
    bars.push_back(make_bar(2026, 8, 25, 101.0));

    HistoricalDataFeed feed{std::move(bars)};

    const auto first = feed.next();
    const auto second = feed.next();

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());

    REQUIRE(first->session_date() != second->session_date());
    REQUIRE(first->close() == 100.0);
    REQUIRE(second->close() == 101.0);
}

TEST_CASE("HistoricalDataFeed reports exhaustion after the final bar")
{
    std::vector<Bar> bars;

    bars.push_back(make_bar(2026, 8, 24, 100.0));

    HistoricalDataFeed feed{std::move(bars)};

    const auto first = feed.next();
    const auto exhausted = feed.next();

    REQUIRE(first.has_value());
    REQUIRE_FALSE(exhausted.has_value());
}

TEST_CASE("HistoricalDataFeed remains exhausted after repeated reads")
{
    std::vector<Bar> bars;

    bars.push_back(make_bar(2026, 8, 24, 100.0));

    HistoricalDataFeed feed{std::move(bars)};

    REQUIRE(feed.next().has_value());

    REQUIRE_FALSE(feed.next().has_value());
    REQUIRE_FALSE(feed.next().has_value());
    REQUIRE_FALSE(feed.next().has_value());
}

TEST_CASE("HistoricalDataFeed with an empty dataset is immediately exhausted")
{
    HistoricalDataFeed feed{std::vector<Bar>{}};

    const auto result = feed.next();

    REQUIRE_FALSE(result.has_value());
}
#include "backtester/data/csv_bar_reader.hpp"
#include "backtester/data/historical_data_feed.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

namespace {

using backtester::data::CsvBarReader;
using backtester::data::HistoricalDataFeed;

std::filesystem::path fixture_path(const std::string& filename)
{
    return std::filesystem::path{BACKTESTER_SOURCE_DIR}
        / "data"
        / "fixtures"
        / filename;
}

}  // namespace

TEST_CASE(
    "Canonical CSV flows through CsvBarReader and HistoricalDataFeed "
    "without modification"
)
{
    const auto parsed_bars = CsvBarReader::read(
        fixture_path("daily_bars_valid.csv")
    );

    REQUIRE_FALSE(parsed_bars.empty());

    HistoricalDataFeed feed{parsed_bars};

    for (const auto& expected_bar : parsed_bars) {
        const auto actual_bar = feed.next();

        REQUIRE(actual_bar.has_value());

        REQUIRE(
            actual_bar->session_date()
            == expected_bar.session_date()
        );

        REQUIRE(
            actual_bar->symbol()
            == expected_bar.symbol()
        );

        REQUIRE(
            actual_bar->open()
            == expected_bar.open()
        );

        REQUIRE(
            actual_bar->high()
            == expected_bar.high()
        );

        REQUIRE(
            actual_bar->low()
            == expected_bar.low()
        );

        REQUIRE(
            actual_bar->close()
            == expected_bar.close()
        );

        REQUIRE(
            actual_bar->volume()
            == expected_bar.volume()
        );
    }

    REQUIRE_FALSE(feed.next().has_value());
    REQUIRE_FALSE(feed.next().has_value());
}
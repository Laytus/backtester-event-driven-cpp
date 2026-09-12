#include "backtester/data/csv_bar_reader.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

using backtester::data::CsvBarReader;
using backtester::market::Bar;
using Catch::Matchers::ContainsSubstring;

std::filesystem::path fixture_path(const std::string& filename)
{
    return std::filesystem::path{BACKTESTER_SOURCE_DIR}
        / "data"
        / "fixtures"
        / filename;
}

}  // namespace

TEST_CASE("CsvBarReader parses a valid canonical dataset")
{
    const auto bars = CsvBarReader::read(
        fixture_path("daily_bars_valid.csv")
    );

    REQUIRE(bars.size() == 5);

    const Bar::SessionDate first_date{
        std::chrono::year{2026},
        std::chrono::month{8},
        std::chrono::day{24}
    };

    REQUIRE(bars[0].session_date() == first_date);
    REQUIRE(bars[0].symbol() == "SPY");
    REQUIRE(bars[0].open() == 100.0);
    REQUIRE(bars[0].high() == 105.0);
    REQUIRE(bars[0].low() == 99.0);
    REQUIRE(bars[0].close() == 103.0);
    REQUIRE(bars[0].volume() == 1'000'000);

    const Bar::SessionDate last_date{
        std::chrono::year{2026},
        std::chrono::month{8},
        std::chrono::day{28}
    };

    REQUIRE(bars[4].session_date() == last_date);
    REQUIRE(bars[4].symbol() == "SPY");
    REQUIRE(bars[4].close() == 107.0);
    REQUIRE(bars[4].volume() == 1'300'000);
}

TEST_CASE("CsvBarReader rejects a file that cannot be opened")
{
    REQUIRE_THROWS_AS(
        CsvBarReader::read(
            fixture_path("does_not_exist.csv")
        ),
        std::runtime_error
    );
}

TEST_CASE("CsvBarReader rejects an invalid header")
{
    REQUIRE_THROWS_WITH(
        CsvBarReader::read(
            fixture_path("daily_bars_invalid_header.csv")
        ),
        ContainsSubstring(":1:")
    );
}

TEST_CASE("CsvBarReader rejects an invalid calendar date")
{
    REQUIRE_THROWS_WITH(
        CsvBarReader::read(
            fixture_path("daily_bars_invalid_date.csv")
        ),
        ContainsSubstring(":2:")
    );
}

TEST_CASE("CsvBarReader rejects a missing required value")
{
    REQUIRE_THROWS_WITH(
        CsvBarReader::read(
            fixture_path("daily_bars_missing_value.csv")
        ),
        ContainsSubstring(":2:")
    );
}

TEST_CASE("CsvBarReader rejects a malformed numeric value")
{
    REQUIRE_THROWS_WITH(
        CsvBarReader::read(
            fixture_path("daily_bars_malformed_numeric.csv")
        ),
        ContainsSubstring(":2:")
    );
}

TEST_CASE("CsvBarReader rejects invalid OHLC data")
{
    REQUIRE_THROWS_WITH(
        CsvBarReader::read(
            fixture_path("daily_bars_invalid_ohlc.csv")
        ),
        ContainsSubstring(":2:")
    );
}

TEST_CASE("CsvBarReader rejects duplicate session dates")
{
    REQUIRE_THROWS_WITH(
        CsvBarReader::read(
            fixture_path("daily_bars_duplicate_date.csv")
        ),
        ContainsSubstring("Duplicate session date")
    );
}

TEST_CASE("CsvBarReader rejects non-monotonic session dates")
{
    REQUIRE_THROWS_WITH(
        CsvBarReader::read(
            fixture_path("daily_bars_non_monotonic.csv")
        ),
        ContainsSubstring("Session dates must be strictly increasing")
    );
}

TEST_CASE("CsvBarReader rejects mixed symbols")
{
    REQUIRE_THROWS_WITH(
        CsvBarReader::read(
            fixture_path("daily_bars_mixed_symbols.csv")
        ),
        ContainsSubstring("Expected symbol 'SPY' but found 'QQQ'")
    );
}
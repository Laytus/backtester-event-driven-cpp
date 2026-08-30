#include <catch2/catch_test_macros.hpp>

#include "backtester/version.hpp"

TEST_CASE("the library exposes its version") {
    REQUIRE(backtester::version() == "0.1.0");
}


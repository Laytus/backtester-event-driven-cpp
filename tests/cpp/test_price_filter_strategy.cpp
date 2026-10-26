#include "backtester/strategy/price_filter_strategy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using backtester::events::MarketEvent;
using backtester::events::SignalDirection;
using backtester::events::SignalEvent;
using backtester::market::Bar;
using backtester::market::SessionDate;
using backtester::strategy::PriceFilterStrategy;
using backtester::strategy::PriceSource;

SessionDate session_date(std::size_t offset) {
    const std::chrono::sys_days first{
        std::chrono::year{2026} / std::chrono::October / 1
    };
    return SessionDate{first + std::chrono::days{static_cast<int>(offset)}};
}

Bar make_bar(
    std::size_t index,
    double price,
    const std::string& symbol = "AAPL"
) {
    return Bar{
        session_date(index), symbol, price, price, price, price,
        static_cast<std::uint64_t>(1'000 + index)
    };
}

// The selected field equals observation; all other OHLC fields are different
// while maintaining valid, positive, internally consistent market bars.
Bar make_source_bar(
    std::size_t index,
    double observation,
    PriceSource source,
    const std::string& symbol = "AAPL"
) {
    switch (source) {
        case PriceSource::Open:
            return Bar{
                session_date(index), symbol, observation,
                std::max(observation, 5.0) + 1.0,
                std::min(observation, 5.0) - 1.0,
                5.0, 1'000
            };
        case PriceSource::High:
            return Bar{
                session_date(index), symbol,
                4.0, observation, 3.0, 4.0, 1'000
            };
        case PriceSource::Low:
            return Bar{
                session_date(index), symbol,
                200.0, 201.0, observation, 200.0, 1'000
            };
        case PriceSource::Close:
            return Bar{
                session_date(index), symbol,
                5.0, std::max(observation, 5.0) + 1.0,
                4.0, observation, 1'000
            };
    }
    throw std::invalid_argument("Invalid test PriceSource");
}

std::vector<SignalEvent> deliver(
    PriceFilterStrategy& strategy,
    const Bar& bar
) {
    return strategy.on_market(MarketEvent{bar});
}

void require_no_signal(PriceFilterStrategy& strategy, const Bar& bar) {
    REQUIRE(deliver(strategy, bar).empty());
}

void require_signal(
    PriceFilterStrategy& strategy,
    const Bar& bar,
    SignalDirection expected
) {
    const auto signals = deliver(strategy, bar);
    REQUIRE(signals.size() == 1);
    REQUIRE(signals.front().symbol() == bar.symbol());
    REQUIRE(signals.front().signal_date() == bar.session_date());
    REQUIRE(signals.front().direction() == expected);
    REQUIRE(signals.front().direction() != SignalDirection::Short);
}

struct SignalRecord final {
    std::size_t index;
    SignalDirection direction;
    SessionDate date;

    bool operator==(const SignalRecord&) const = default;
};

std::vector<SignalRecord> replay(
    PriceFilterStrategy& strategy,
    const std::vector<double>& prices,
    const std::string& symbol = "AAPL",
    std::size_t first_date_offset = 0
) {
    std::vector<SignalRecord> records;
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(first_date_offset + i, prices[i], symbol);
        for (const auto& signal : deliver(strategy, bar)) {
            REQUIRE(signal.symbol() == symbol);
            REQUIRE(signal.signal_date() == bar.session_date());
            REQUIRE(signal.direction() != SignalDirection::Short);
            records.push_back({i, signal.direction(), signal.signal_date()});
        }
    }
    return records;
}

void require_same_records(
    const std::vector<SignalRecord>& actual,
    const std::vector<SignalRecord>& expected
) {
    REQUIRE(actual.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(actual[i] == expected[i]);
    }
}

// Independent O(N * lookback) reference. It scans prior prices directly,
// without using RollingMinMax or any production strategy implementation.
std::vector<SignalRecord> reference_replay(
    const std::vector<double>& prices,
    std::size_t lookback,
    double threshold
) {
    std::vector<SignalRecord> records;
    auto target = SignalDirection::Flat;

    for (std::size_t i = lookback; i < prices.size(); ++i) {
        const auto first = prices.begin() + static_cast<std::ptrdiff_t>(i - lookback);
        const auto last = prices.begin() + static_cast<std::ptrdiff_t>(i);
        const auto [minimum, maximum] = std::minmax_element(first, last);
        const double upper = std::fma(threshold, *maximum, *maximum);
        const double lower = std::fma(-threshold, *minimum, *minimum);

        auto desired = target;
        if (prices[i] > upper) {
            desired = SignalDirection::Long;
        } else if (prices[i] < lower) {
            desired = SignalDirection::Flat;
        }
        if (desired != target) {
            records.push_back({i, desired, session_date(i)});
            target = desired;
        }
    }
    return records;
}

}  // namespace

TEST_CASE("Price filter rejects an empty symbol") {
    REQUIRE_THROWS_AS((PriceFilterStrategy{""}), std::invalid_argument);
}

TEST_CASE("Price filter rejects a zero lookback period") {
    REQUIRE_THROWS_AS((PriceFilterStrategy{"AAPL", 0}), std::invalid_argument);
}

TEST_CASE("Price filter rejects negative and nonfinite thresholds") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double threshold : {-0.01, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            (PriceFilterStrategy{"AAPL", 1, threshold}),
            std::invalid_argument
        );
    }
}

TEST_CASE("Price filter rejects thresholds equal to or above one") {
    for (const double threshold : {1.0, 1.01, 2.0}) {
        REQUIRE_THROWS_AS(
            (PriceFilterStrategy{"AAPL", 1, threshold}),
            std::invalid_argument
        );
    }
}

TEST_CASE("Price filter accepts zero negative zero and thresholds near one") {
    PriceFilterStrategy zero{"AAPL", 1, 0.0};
    PriceFilterStrategy negative_zero{"AAPL", 1, -0.0};
    PriceFilterStrategy almost_one{"AAPL", 1, std::nextafter(1.0, 0.0)};

    require_no_signal(zero, make_bar(0, 100.0));
    require_no_signal(negative_zero, make_bar(0, 100.0));
    require_no_signal(almost_one, make_bar(0, 100.0));
    require_signal(zero, make_bar(1, 101.0), SignalDirection::Long);
    require_signal(negative_zero, make_bar(1, 101.0), SignalDirection::Long);
    require_no_signal(almost_one, make_bar(1, 101.0));
}

TEST_CASE("Price filter rejects invalid OHLC price sources") {
    REQUIRE_THROWS_AS(
        (PriceFilterStrategy{"AAPL", 1, 0.01, static_cast<PriceSource>(-1)}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        (PriceFilterStrategy{"AAPL", 1, 0.01, static_cast<PriceSource>(99)}),
        std::invalid_argument
    );
}

TEST_CASE("Price filter default configuration uses previous Close and one percent") {
    PriceFilterStrategy strategy{"AAPL"};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_signal(strategy, make_bar(1, 102.0), SignalDirection::Long);
    require_signal(strategy, make_bar(2, 98.0), SignalDirection::Flat);
}

TEST_CASE("Price filter first evaluation occurs after N historical observations") {
    PriceFilterStrategy strategy{"AAPL", 3, 0.01};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_no_signal(strategy, make_bar(1, 102.0));
    require_no_signal(strategy, make_bar(2, 104.0));
    require_signal(strategy, make_bar(3, 107.0), SignalDirection::Long);
}

TEST_CASE("Price filter matches approved ten-price one-bar oracle") {
    PriceFilterStrategy strategy{"AAPL", 1, 0.02};
    const std::array<double, 10> closes{
        100.0, 103.0, 104.0, 101.0, 98.0,
        99.0, 102.0, 100.0, 98.0, 95.0
    };
    for (std::size_t i = 0; i < closes.size(); ++i) {
        const auto bar = make_bar(i, closes[i]);
        if (i == 1 || i == 6) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 3 || i == 9) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Price filter matches approved two-bar rolling oracle") {
    PriceFilterStrategy strategy{"AAPL", 2, 0.01};
    const std::array<double, 9> closes{
        100.0, 102.0, 105.0, 104.0, 98.0,
        99.0, 107.0, 106.0, 97.0
    };
    for (std::size_t i = 0; i < closes.size(); ++i) {
        const auto bar = make_bar(i, closes[i]);
        if (i == 2 || i == 6) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4 || i == 8) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Price filter initially bearish regime remains Flat") {
    PriceFilterStrategy strategy{"AAPL", 1, 0.01};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_no_signal(strategy, make_bar(1, 95.0));
    require_no_signal(strategy, make_bar(2, 94.0));
    require_signal(strategy, make_bar(3, 97.0), SignalDirection::Long);
}

TEST_CASE("Price filter first eligible neutral price preserves Flat") {
    PriceFilterStrategy strategy{"AAPL", 1, 0.01};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_no_signal(strategy, make_bar(1, 100.0));
    require_signal(strategy, make_bar(2, 103.0), SignalDirection::Long);
}

TEST_CASE("Price filter neutral regime preserves an existing Long target") {
    PriceFilterStrategy strategy{"AAPL", 1, 0.1};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_signal(strategy, make_bar(1, 120.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(2, 119.0));
    require_no_signal(strategy, make_bar(3, 108.0));
    require_signal(strategy, make_bar(4, 95.0), SignalDirection::Flat);
}

TEST_CASE("Price filter emits no repeated Long or Flat targets") {
    PriceFilterStrategy strategy{"AAPL", 1, 0.02};
    const std::array<double, 7> closes{
        100.0, 110.0, 121.0, 133.1, 90.0, 80.0, 70.0
    };
    for (std::size_t i = 0; i < closes.size(); ++i) {
        const auto bar = make_bar(i, closes[i]);
        if (i == 1) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Price filter exact upper threshold equality is neutral") {
    PriceFilterStrategy strategy{"AAPL", 1, 0.25};
    require_no_signal(strategy, make_bar(0, 100.0));
    const double upper = std::fma(0.25, 100.0, 100.0);
    require_no_signal(strategy, make_bar(1, upper));
    require_signal(strategy, make_bar(2, 160.0), SignalDirection::Long);
}

TEST_CASE("Price filter exact lower threshold equality preserves Long") {
    PriceFilterStrategy strategy{"AAPL", 1, 0.2};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_signal(strategy, make_bar(1, 130.0), SignalDirection::Long);
    const double lower = std::fma(-0.2, 130.0, 130.0);
    require_no_signal(strategy, make_bar(2, lower));
    require_signal(strategy, make_bar(3, 80.0), SignalDirection::Flat);
}

TEST_CASE("Price filter zero threshold uses strict previous-price comparisons") {
    PriceFilterStrategy strategy{"AAPL", 1, 0.0};
    const std::array<double, 7> prices{10.0, 10.0, 11.0, 11.0, 10.0, 10.0, 12.0};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 2 || i == 6) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Price filter hysteresis retains target inside the neutral interval") {
    PriceFilterStrategy strategy{"AAPL", 1, 0.1};
    const std::array<double, 9> prices{
        100.0, 112.0, 102.0, 92.0, 80.0, 87.0, 89.0, 100.0, 99.0
    };
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 1 || i == 7) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Price filter compares with the previous full-window maximum") {
    PriceFilterStrategy strategy{"AAPL", 3, 0.0};
    const std::array<double, 5> prices{100.0, 120.0, 110.0, 115.0, 121.0};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Price filter compares with the previous full-window minimum") {
    PriceFilterStrategy strategy{"AAPL", 3, 0.0};
    const std::array<double, 5> prices{100.0, 90.0, 95.0, 110.0, 80.0};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 3) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Price filter expires historical highs before evaluating new breakouts") {
    PriceFilterStrategy strategy{"AAPL", 2, 0.0};
    const std::array<double, 5> prices{100.0, 150.0, 140.0, 141.0, 142.0};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Price filter expires historical lows before evaluating new breakdowns") {
    PriceFilterStrategy strategy{"AAPL", 2, 0.0};
    const std::array<double, 6> prices{100.0, 80.0, 90.0, 110.0, 90.0, 89.0};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 3) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 5) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Price filter distinct lookbacks can produce different signals") {
    PriceFilterStrategy one{"AAPL", 1, 0.0};
    PriceFilterStrategy two{"AAPL", 2, 0.0};
    const std::array<double, 3> prices{100.0, 105.0, 103.0};
    require_no_signal(one, make_bar(0, prices[0]));
    require_no_signal(two, make_bar(0, prices[0]));
    require_signal(one, make_bar(1, prices[1]), SignalDirection::Long);
    require_no_signal(two, make_bar(1, prices[1]));
    require_signal(one, make_bar(2, prices[2]), SignalDirection::Flat);
    require_no_signal(two, make_bar(2, prices[2]));
}

TEST_CASE("Price filter threshold measures relative not absolute changes") {
    PriceFilterStrategy low_scale{"AAPL", 1, 0.1};
    PriceFilterStrategy high_scale{"AAPL", 1, 0.1};
    require_no_signal(low_scale, make_bar(0, 10.0));
    require_no_signal(high_scale, make_bar(0, 100.0));
    require_signal(low_scale, make_bar(1, 11.5), SignalDirection::Long);
    require_signal(high_scale, make_bar(1, 115.0), SignalDirection::Long);
    require_signal(low_scale, make_bar(2, 9.0), SignalDirection::Flat);
    require_signal(high_scale, make_bar(2, 90.0), SignalDirection::Flat);
}

TEST_CASE("Price filter supports all configured OHLC price sources") {
    const std::array<double, 6> selected{100.0, 103.0, 102.0, 98.0, 100.0, 106.0};
    for (const auto source : {
        PriceSource::Open, PriceSource::High,
        PriceSource::Low, PriceSource::Close
    }) {
        PriceFilterStrategy strategy{"AAPL", 1, 0.01, source};
        for (std::size_t i = 0; i < selected.size(); ++i) {
            const auto bar = make_source_bar(i, selected[i], source);
            if (i == 1 || i == 4) {
                require_signal(strategy, bar, SignalDirection::Long);
            } else if (i == 3) {
                require_signal(strategy, bar, SignalDirection::Flat);
            } else {
                require_no_signal(strategy, bar);
            }
        }
    }
}

TEST_CASE("Price filter default price source is Close rather than Open") {
    PriceFilterStrategy close_strategy{"AAPL"};
    PriceFilterStrategy open_strategy{"AAPL", 1, 0.01, PriceSource::Open};
    const std::array<double, 3> closes{100.0, 105.0, 95.0};
    for (std::size_t i = 0; i < closes.size(); ++i) {
        const auto bar = Bar{
            session_date(i), "AAPL", 100.0, 110.0, 90.0, closes[i], 1'000
        };
        if (i == 1) {
            require_signal(close_strategy, bar, SignalDirection::Long);
        } else if (i == 2) {
            require_signal(close_strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(close_strategy, bar);
        }
        require_no_signal(open_strategy, bar);
    }
}

TEST_CASE("Price filter ignores changes in unselected OHLC fields") {
    PriceFilterStrategy plain{"AAPL", 2, 0.01};
    PriceFilterStrategy decorated{"AAPL", 2, 0.01};
    const std::array<double, 6> closes{100.0, 103.0, 106.0, 101.0, 95.0, 109.0};
    for (std::size_t i = 0; i < closes.size(); ++i) {
        const auto basic = make_bar(i, closes[i]);
        const double open = closes[i] + (i % 2 == 0 ? 2.0 : -2.0);
        const auto varied = Bar{
            session_date(i), "AAPL", open,
            std::max(open, closes[i]) + 5.0,
            std::min(open, closes[i]) - 5.0,
            closes[i], static_cast<std::uint64_t>(12'345 + i)
        };
        const auto first = deliver(plain, basic);
        const auto second = deliver(decorated, varied);
        REQUIRE(first.size() == second.size());
        for (std::size_t j = 0; j < first.size(); ++j) {
            REQUIRE(first[j].direction() == second[j].direction());
            REQUIRE(first[j].signal_date() == second[j].signal_date());
        }
    }
}

TEST_CASE("Price filter ignores foreign symbols throughout warm-up") {
    PriceFilterStrategy strategy{"AAPL", 2, 0.01};
    require_no_signal(strategy, make_bar(0, 10.0, "MSFT"));
    require_no_signal(strategy, make_bar(1, 100.0, "AAPL"));
    require_no_signal(strategy, make_bar(2, 50.0, "MSFT"));
    require_no_signal(strategy, make_bar(3, 102.0, "AAPL"));
    require_signal(strategy, make_bar(4, 105.0, "AAPL"), SignalDirection::Long);
}

TEST_CASE("Price filter foreign symbols cannot change a ready target") {
    PriceFilterStrategy strategy{"AAPL", 1, 0.01};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_signal(strategy, make_bar(1, 110.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(2, 1.0, "MSFT"));
    require_no_signal(strategy, make_bar(3, 110.0));
    require_signal(strategy, make_bar(4, 90.0), SignalDirection::Flat);
}

TEST_CASE("Price filter warm-up counts observed bars rather than calendar days") {
    PriceFilterStrategy strategy{"AAPL", 2, 0.01};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_no_signal(strategy, make_bar(9, 102.0));
    require_signal(strategy, make_bar(37, 105.0), SignalDirection::Long);
}

TEST_CASE("Price filter instances for different symbols maintain independent state") {
    PriceFilterStrategy aapl{"AAPL", 1, 0.01};
    PriceFilterStrategy msft{"MSFT", 1, 0.01};
    require_no_signal(aapl, make_bar(0, 100.0, "AAPL"));
    require_no_signal(msft, make_bar(0, 200.0, "MSFT"));
    require_signal(aapl, make_bar(1, 110.0, "AAPL"), SignalDirection::Long);
    require_no_signal(msft, make_bar(1, 190.0, "MSFT"));
    require_signal(msft, make_bar(2, 210.0, "MSFT"), SignalDirection::Long);
    require_signal(aapl, make_bar(3, 90.0, "AAPL"), SignalDirection::Flat);
}

TEST_CASE("Price filter instances for the same symbol maintain independent state") {
    PriceFilterStrategy first{"AAPL", 1, 0.01};
    PriceFilterStrategy second{"AAPL", 1, 0.01};
    require_no_signal(first, make_bar(0, 100.0));
    require_no_signal(second, make_bar(0, 200.0));
    require_signal(first, make_bar(1, 110.0), SignalDirection::Long);
    require_no_signal(second, make_bar(1, 180.0));
    require_signal(second, make_bar(2, 210.0), SignalDirection::Long);
    require_signal(first, make_bar(2, 90.0), SignalDirection::Flat);
}

TEST_CASE("Price filter processes identical event replays deterministically") {
    std::vector<double> prices;
    prices.reserve(400);
    for (std::size_t i = 0; i < 400; ++i) {
        const double p = 100.0 + 0.07 * static_cast<double>(i)
            + static_cast<double>((i * 17) % 31) - 15.0;
        prices.push_back(p);
    }
    PriceFilterStrategy first{"AAPL", 7, 0.035};
    PriceFilterStrategy second{"AAPL", 7, 0.035};
    require_same_records(replay(first, prices), replay(second, prices));
}

TEST_CASE("Price filter matches an independent historical-only oracle") {
    std::vector<double> prices;
    prices.reserve(650);
    for (std::size_t i = 0; i < 650; ++i) {
        const double p = 105.0 + 0.015 * static_cast<double>(i)
            + 0.8 * static_cast<double>((i * 13) % 23)
            - 0.6 * static_cast<double>((i * 29) % 17);
        prices.push_back(p);
    }

    for (const std::size_t lookback : {1U, 2U, 3U, 5U, 10U, 20U, 50U}) {
        for (const double threshold : {0.0, 0.01, 0.05, 0.2}) {
            PriceFilterStrategy strategy{"AAPL", lookback, threshold};
            require_same_records(
                replay(strategy, prices),
                reference_replay(prices, lookback, threshold)
            );
        }
    }
}

TEST_CASE("Price filter does not use future observations in earlier decisions") {
    PriceFilterStrategy prefix_only{"AAPL", 3, 0.01};
    PriceFilterStrategy extended{"AAPL", 3, 0.01};
    const std::vector<double> prefix{100.0, 102.0, 101.0, 104.0, 98.0, 103.0};
    const std::vector<double> extra{120.0, 70.0, 140.0};
    auto full = prefix;
    full.insert(full.end(), extra.begin(), extra.end());

    const auto first = replay(prefix_only, prefix);
    const auto second = replay(extended, full);
    REQUIRE(second.size() >= first.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        REQUIRE(second[i] == first[i]);
    }
}

TEST_CASE("Price filter large finite thresholds cannot cause premature bullish signals") {
    PriceFilterStrategy strategy{"AAPL", 1, 0.5};
    const double large = 1.0e308;
    require_no_signal(strategy, make_bar(0, large));
    // The upper threshold is +infinity; no finite observation crosses it.
    require_no_signal(strategy, make_bar(1, large));
    require_no_signal(strategy, make_bar(2, 1.0e307));
    require_signal(strategy, make_bar(3, 1.6e307), SignalDirection::Long);
    require_no_signal(strategy, make_bar(4, large));
    require_no_signal(strategy, make_bar(5, large));
    require_signal(strategy, make_bar(6, 1.0e307), SignalDirection::Flat);
}

TEST_CASE("Price filter remains aligned over thousands of rolling updates") {
    std::vector<double> prices;
    prices.reserve(5'000);
    for (std::size_t i = 0; i < 5'000; ++i) {
        prices.push_back(
            120.0 + 0.005 * static_cast<double>(i)
            + static_cast<double>((i * 31) % 61) - 25.0
        );
    }
    PriceFilterStrategy strategy{"AAPL", 21, 0.035};
    require_same_records(
        replay(strategy, prices),
        reference_replay(prices, 21, 0.035)
    );
}

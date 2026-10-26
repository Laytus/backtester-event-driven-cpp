
#include "backtester/strategy/momentum_strategy.hpp"

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
using backtester::indicators::MomentumMode;
using backtester::market::Bar;
using backtester::market::SessionDate;
using backtester::strategy::MomentumStrategy;
using backtester::strategy::PriceSource;

SessionDate session_date(std::size_t offset) {
    const std::chrono::sys_days start{
        std::chrono::year{2026} / std::chrono::October / 1
    };
    return SessionDate{start + std::chrono::days{static_cast<int>(offset)}};
}

Bar make_bar(
    std::size_t index,
    double close,
    const std::string& symbol = "AAPL"
) {
    return Bar{
        session_date(index), symbol, close, close, close, close,
        static_cast<std::uint64_t>(1'000 + index)
    };
}

Bar make_ohlc(
    std::size_t index,
    double open,
    double high,
    double low,
    double close,
    const std::string& symbol = "AAPL"
) {
    return Bar{
        session_date(index), symbol, open, high, low, close,
        static_cast<std::uint64_t>(1'000 + index)
    };
}

Bar make_source_bar(
    std::size_t index,
    double price,
    PriceSource source
) {
    switch (source) {
        case PriceSource::Open:
            return make_ohlc(index, price, std::max(price, 5.0) + 1.0,
                             4.0, 5.0);
        case PriceSource::High:
            return make_ohlc(index, 5.0, price, 4.0, 5.0);
        case PriceSource::Low:
            return make_ohlc(index, 200.0, 220.0, price, 200.0);
        case PriceSource::Close:
            return make_ohlc(index, 5.0, std::max(price, 5.0) + 1.0,
                             4.0, price);
    }
    throw std::invalid_argument("Invalid test price source");
}

std::vector<SignalEvent> deliver(MomentumStrategy& strategy, const Bar& bar) {
    return strategy.on_market(MarketEvent{bar});
}

void require_no_signal(MomentumStrategy& strategy, const Bar& bar) {
    REQUIRE(deliver(strategy, bar).empty());
}

void require_signal(
    MomentumStrategy& strategy,
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
    MomentumStrategy& strategy,
    const std::vector<Bar>& bars
) {
    std::vector<SignalRecord> records;
    for (std::size_t i = 0; i < bars.size(); ++i) {
        const auto signals = deliver(strategy, bars[i]);
        REQUIRE(signals.size() <= 1);
        for (const auto& signal : signals) {
            REQUIRE(signal.symbol() == bars[i].symbol());
            REQUIRE(signal.signal_date() == bars[i].session_date());
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

double selected_price(const Bar& bar, PriceSource source) {
    switch (source) {
        case PriceSource::Open: return bar.open();
        case PriceSource::High: return bar.high();
        case PriceSource::Low: return bar.low();
        case PriceSource::Close: return bar.close();
    }
    throw std::invalid_argument("Invalid test price source");
}

// Independent O(T) reference: directly indexes historical bars without
// using the Momentum indicator or any strategy state.
std::vector<SignalRecord> reference_replay(
    const std::vector<Bar>& bars,
    std::size_t period,
    MomentumMode mode,
    double threshold,
    PriceSource source
) {
    std::vector<SignalRecord> records;
    auto target = SignalDirection::Flat;
    for (std::size_t i = period; i < bars.size(); ++i) {
        const double current = selected_price(bars[i], source);
        const double previous = selected_price(bars[i - period], source);
        const double difference = current - previous;
        const double value = mode == MomentumMode::Absolute
            ? difference
            : (difference / previous) * 100.0;

        auto desired = target;
        if (value > threshold) {
            desired = SignalDirection::Long;
        } else if (value < -threshold) {
            desired = SignalDirection::Flat;
        }

        if (desired != target) {
            records.push_back({i, desired, bars[i].session_date()});
            target = desired;
        }
    }
    return records;
}

std::vector<Bar> make_bars(std::size_t count) {
    std::vector<Bar> bars;
    bars.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double close = 100.0
            + static_cast<double>((i * 17) % 29) - 14.0
            + 0.025 * static_cast<double>(i);
        const double open = close + (i % 2 == 0 ? 1.25 : -1.75);
        const double high = std::max(open, close) + 2.0;
        const double low = std::min(open, close) - 3.0;
        bars.push_back(make_ohlc(i, open, high, low, close));
    }
    return bars;
}

}  // namespace

TEST_CASE("Momentum strategy rejects an empty symbol") {
    REQUIRE_THROWS_AS((MomentumStrategy{""}), std::invalid_argument);
}

TEST_CASE("Momentum strategy rejects zero periods") {
    REQUIRE_THROWS_AS((MomentumStrategy{"AAPL", 0}), std::invalid_argument);
}

TEST_CASE("Momentum strategy rejects unrecognized modes") {
    REQUIRE_THROWS_AS(
        (MomentumStrategy{"AAPL", 2, static_cast<MomentumMode>(99)}),
        std::invalid_argument
    );
}

TEST_CASE("Momentum strategy rejects negative and nonfinite thresholds") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double value : {-0.1, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            (MomentumStrategy{"AAPL", 2, MomentumMode::Absolute, value}),
            std::invalid_argument
        );
    }
}

TEST_CASE("Momentum strategy percentage mode rejects thresholds at or above one hundred") {
    for (const double value : {100.0, 100.01, 200.0}) {
        REQUIRE_THROWS_AS(
            (MomentumStrategy{"AAPL", 2, MomentumMode::Percentage, value}),
            std::invalid_argument
        );
    }
}

TEST_CASE("Momentum strategy absolute mode permits thresholds above one hundred") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Absolute, 200.0};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_no_signal(strategy, make_bar(1, 250.0));
    require_signal(strategy, make_bar(2, 500.0), SignalDirection::Long);
}

TEST_CASE("Momentum strategy accepts zero negative zero and near-limit percentage thresholds") {
    MomentumStrategy zero{"AAPL", 1, MomentumMode::Percentage, 0.0};
    MomentumStrategy negative_zero{"AAPL", 1, MomentumMode::Percentage, -0.0};
    MomentumStrategy near_limit{
        "AAPL", 1, MomentumMode::Percentage, std::nextafter(100.0, 0.0)
    };
    require_no_signal(zero, make_bar(0, 100.0));
    require_no_signal(negative_zero, make_bar(0, 100.0));
    require_no_signal(near_limit, make_bar(0, 100.0));
    require_signal(zero, make_bar(1, 105.0), SignalDirection::Long);
    require_signal(negative_zero, make_bar(1, 105.0), SignalDirection::Long);
    require_no_signal(near_limit, make_bar(1, 105.0));
}

TEST_CASE("Momentum strategy rejects invalid OHLC price sources") {
    for (const auto source : {static_cast<PriceSource>(-1), static_cast<PriceSource>(99)}) {
        REQUIRE_THROWS_AS(
            (MomentumStrategy{"AAPL", 2, MomentumMode::Absolute, 0.0, source}),
            std::invalid_argument
        );
    }
}

TEST_CASE("Momentum strategy default period becomes ready on bar eleven") {
    MomentumStrategy strategy{"AAPL"};
    for (std::size_t i = 0; i < 10; ++i) {
        require_no_signal(strategy, make_bar(i, 100.0));
    }
    require_signal(strategy, make_bar(10, 105.0), SignalDirection::Long);
}

TEST_CASE("Momentum strategy period one first evaluates on second observation") {
    MomentumStrategy strategy{"AAPL", 1};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_signal(strategy, make_bar(1, 103.0), SignalDirection::Long);
}

TEST_CASE("Momentum strategy matches the approved absolute momentum oracle") {
    MomentumStrategy strategy{"AAPL", 2, MomentumMode::Absolute, 2.0};
    const std::array<double, 11> prices{
        100, 101, 102, 104, 99, 100, 101, 105, 103, 97, 101
    };
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 3 || i == 7) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4 || i == 9) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Momentum strategy matches the percentage momentum oracle") {
    MomentumStrategy strategy{"AAPL", 2, MomentumMode::Percentage, 2.0};
    const std::array<double, 11> prices{
        100, 101, 102, 104, 99, 100, 101, 105, 103, 97, 101
    };
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 3 || i == 6) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4 || i == 9) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Momentum strategy absolute and percentage modes can diverge at the same threshold") {
    MomentumStrategy absolute{"AAPL", 2, MomentumMode::Absolute, 2.0};
    MomentumStrategy percentage{"AAPL", 2, MomentumMode::Percentage, 2.0};
    const std::array<double, 7> prices{100, 101, 102, 104, 99, 100, 101};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 6) {
            require_no_signal(absolute, bar);
            require_signal(percentage, bar, SignalDirection::Long);
        } else {
            const auto a = deliver(absolute, bar);
            const auto b = deliver(percentage, bar);
            REQUIRE(a.size() == b.size());
            for (std::size_t j = 0; j < a.size(); ++j) {
                REQUIRE(a[j].direction() == b[j].direction());
            }
        }
    }
}

TEST_CASE("Momentum strategy first bearish evaluation preserves initial Flat") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Absolute, 1.0};
    require_no_signal(strategy, make_bar(0, 110.0));
    require_no_signal(strategy, make_bar(1, 100.0));
    require_signal(strategy, make_bar(2, 105.0), SignalDirection::Long);
}

TEST_CASE("Momentum strategy first neutral evaluation preserves initial Flat") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Absolute, 2.0};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_no_signal(strategy, make_bar(1, 101.0));
    require_signal(strategy, make_bar(2, 104.0), SignalDirection::Long);
}

TEST_CASE("Momentum strategy upper threshold equality is neutral") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Absolute, 2.0};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_no_signal(strategy, make_bar(1, 102.0));
    require_signal(strategy, make_bar(2, 105.0), SignalDirection::Long);
}

TEST_CASE("Momentum strategy lower threshold equality preserves Long") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Absolute, 2.0};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_signal(strategy, make_bar(1, 105.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(2, 103.0));
    require_signal(strategy, make_bar(3, 100.0), SignalDirection::Flat);
}

TEST_CASE("Momentum strategy zero threshold uses strict sign comparisons") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Absolute, 0.0};
    const std::array<double, 5> prices{100, 100, 101, 101, 100};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 2) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Momentum strategy neutral region retains the existing Long target") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Absolute, 3.0};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_signal(strategy, make_bar(1, 110.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(2, 110.0));
    require_no_signal(strategy, make_bar(3, 109.0));
    require_no_signal(strategy, make_bar(4, 107.0));
    require_signal(strategy, make_bar(5, 103.0), SignalDirection::Flat);
}

TEST_CASE("Momentum strategy suppresses repeated Long and Flat transitions") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Absolute, 2.0};
    const std::array<double, 7> prices{100, 110, 120, 130, 100, 90, 80};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 1) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Momentum strategy compares against K bars ago instead of first price") {
    MomentumStrategy strategy{"AAPL", 2, MomentumMode::Absolute, 5.0};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_no_signal(strategy, make_bar(1, 110.0));
    require_signal(strategy, make_bar(2, 120.0), SignalDirection::Long);
    require_signal(strategy, make_bar(3, 102.0), SignalDirection::Flat);
}

TEST_CASE("Momentum strategy absolute and percentage thresholds have different units") {
    MomentumStrategy absolute{"AAPL", 1, MomentumMode::Absolute, 2.0};
    MomentumStrategy percentage{"AAPL", 1, MomentumMode::Percentage, 2.0};
    const auto first = make_bar(0, 1000.0);
    const auto second = make_bar(1, 1005.0);
    require_no_signal(absolute, first);
    require_no_signal(percentage, first);
    require_signal(absolute, second, SignalDirection::Long);
    require_no_signal(percentage, second);
}

TEST_CASE("Momentum strategy supports all four OHLC price sources") {
    for (const auto source : {
        PriceSource::Open, PriceSource::High,
        PriceSource::Low, PriceSource::Close
    }) {
        MomentumStrategy strategy{"AAPL", 1, MomentumMode::Absolute, 2.0, source};
        const std::array<double, 4> prices{100, 106, 99, 110};
        for (std::size_t i = 0; i < prices.size(); ++i) {
            const auto bar = make_source_bar(i, prices[i], source);
            if (i == 1 || i == 3) {
                require_signal(strategy, bar, SignalDirection::Long);
            } else if (i == 2) {
                require_signal(strategy, bar, SignalDirection::Flat);
            } else {
                require_no_signal(strategy, bar);
            }
        }
    }
}

TEST_CASE("Momentum strategy defaults to Close rather than Open") {
    MomentumStrategy close_strategy{"AAPL", 1};
    MomentumStrategy open_strategy{"AAPL", 1, MomentumMode::Absolute, 0.0, PriceSource::Open};
    const auto prior = make_ohlc(0, 105, 106, 99, 100);
    const auto current = make_ohlc(1, 105, 112, 104, 110);
    require_no_signal(close_strategy, prior);
    require_no_signal(open_strategy, prior);
    require_signal(close_strategy, current, SignalDirection::Long);
    require_no_signal(open_strategy, current);
}

TEST_CASE("Momentum strategy changes to unselected OHLC do not alter Close signals") {
    MomentumStrategy first{"AAPL", 2, MomentumMode::Absolute, 2.0};
    MomentumStrategy second{"AAPL", 2, MomentumMode::Absolute, 2.0};
    for (std::size_t i = 0; i < 150; ++i) {
        const double close = 100.0 + static_cast<double>((i * 7) % 19);
        const auto a = make_ohlc(i, close, close + 10, close - 10, close);
        const auto b = make_ohlc(
            i, close + (i % 2 == 0 ? 5.0 : -5.0),
            close + 10, close - 10, close
        );
        const auto a_signals = deliver(first, a);
        const auto b_signals = deliver(second, b);
        REQUIRE(a_signals.size() == b_signals.size());
        for (std::size_t j = 0; j < a_signals.size(); ++j) {
            REQUIRE(a_signals[j].direction() == b_signals[j].direction());
        }
    }
}

TEST_CASE("Momentum strategy ignores foreign symbols without advancing warm-up") {
    MomentumStrategy strategy{"AAPL", 2, MomentumMode::Absolute, 0.0};
    require_no_signal(strategy, make_bar(0, 100.0, "MSFT"));
    require_no_signal(strategy, make_bar(1, 100.0, "AAPL"));
    require_no_signal(strategy, make_bar(2, 200.0, "MSFT"));
    require_no_signal(strategy, make_bar(3, 101.0, "AAPL"));
    require_signal(strategy, make_bar(4, 110.0, "AAPL"), SignalDirection::Long);
}

TEST_CASE("Momentum strategy foreign symbols cannot change a ready target") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Absolute, 0.0};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_signal(strategy, make_bar(1, 110.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(2, 10.0, "MSFT"));
    require_no_signal(strategy, make_bar(3, 112.0));
    require_signal(strategy, make_bar(4, 105.0), SignalDirection::Flat);
}

TEST_CASE("Momentum strategy warm-up counts observed bars rather than calendar days") {
    MomentumStrategy strategy{"AAPL", 2, MomentumMode::Absolute, 0.0};
    require_no_signal(strategy, make_bar(0, 100.0));
    require_no_signal(strategy, make_bar(10, 101.0));
    require_signal(strategy, make_bar(40, 103.0), SignalDirection::Long);
}

TEST_CASE("Momentum strategy instances with different symbols isolate their state") {
    MomentumStrategy a{"AAPL", 1};
    MomentumStrategy b{"MSFT", 1};
    require_no_signal(a, make_bar(0, 100.0, "AAPL"));
    require_no_signal(b, make_bar(0, 200.0, "MSFT"));
    require_signal(a, make_bar(1, 105.0, "AAPL"), SignalDirection::Long);
    require_no_signal(b, make_bar(1, 200.0, "MSFT"));
    require_signal(b, make_bar(2, 210.0, "MSFT"), SignalDirection::Long);
    require_signal(a, make_bar(2, 99.0, "AAPL"), SignalDirection::Flat);
}

TEST_CASE("Momentum strategy instances with the same symbol isolate their state") {
    MomentumStrategy a{"AAPL", 1, MomentumMode::Absolute, 0.0};
    MomentumStrategy b{"AAPL", 2, MomentumMode::Absolute, 0.0};
    require_no_signal(a, make_bar(0, 100.0));
    require_no_signal(b, make_bar(0, 100.0));
    require_signal(a, make_bar(1, 110.0), SignalDirection::Long);
    require_no_signal(b, make_bar(1, 110.0));
    require_signal(a, make_bar(2, 105.0), SignalDirection::Flat);
    require_signal(b, make_bar(2, 105.0), SignalDirection::Long);
}

TEST_CASE("Momentum strategy replays identical events deterministically") {
    const auto bars = make_bars(400);
    MomentumStrategy a{"AAPL", 5, MomentumMode::Percentage, 1.5};
    MomentumStrategy b{"AAPL", 5, MomentumMode::Percentage, 1.5};
    require_same_records(replay(a, bars), replay(b, bars));
}

TEST_CASE("Momentum strategy matches an independent historical-only oracle") {
    const auto bars = make_bars(600);
    for (const auto period : {1U, 2U, 5U, 10U, 30U}) {
        for (const auto mode : {MomentumMode::Absolute, MomentumMode::Percentage}) {
            for (const auto source : {
                PriceSource::Open, PriceSource::High,
                PriceSource::Low, PriceSource::Close
            }) {
                for (const double threshold : {0.0, 2.5}) {
                    MomentumStrategy strategy{"AAPL", period, mode, threshold, source};
                    require_same_records(
                        replay(strategy, bars),
                        reference_replay(bars, period, mode, threshold, source)
                    );
                }
            }
        }
    }
}

TEST_CASE("Momentum strategy decisions do not depend on future observations") {
    const auto bars = make_bars(250);
    const std::vector<Bar> prefix(bars.begin(), bars.begin() + 90);
    MomentumStrategy short_run{"AAPL", 7, MomentumMode::Absolute, 1.0};
    MomentumStrategy full_run{"AAPL", 7, MomentumMode::Absolute, 1.0};
    const auto before = replay(short_run, prefix);
    const auto after = replay(full_run, bars);
    REQUIRE(after.size() >= before.size());
    for (std::size_t i = 0; i < before.size(); ++i) {
        REQUIRE(before[i] == after[i]);
    }
}

TEST_CASE("Momentum strategy remains aligned over thousands of updates") {
    const auto bars = make_bars(5'000);
    MomentumStrategy strategy{"AAPL", 20, MomentumMode::Percentage, 1.0};
    require_same_records(
        replay(strategy, bars),
        reference_replay(bars, 20, MomentumMode::Percentage, 1.0, PriceSource::Close)
    );
}

TEST_CASE("Momentum strategy percentage mode preserves tiny positive changes") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Percentage, 0.0};
    const double start = 100.0;
    const double next = std::nextafter(start, std::numeric_limits<double>::infinity());
    require_no_signal(strategy, make_bar(0, start));
    require_signal(strategy, make_bar(1, next), SignalDirection::Long);
}

TEST_CASE("Momentum strategy handles large finite percentage changes") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Percentage, 0.5};
    require_no_signal(strategy, make_bar(0, 1e300));
    require_signal(strategy, make_bar(1, 1.01e300), SignalDirection::Long);
    require_signal(strategy, make_bar(2, 1e300), SignalDirection::Flat);
}

TEST_CASE("Momentum strategy propagates percentage overflow without changing state") {
    MomentumStrategy strategy{"AAPL", 1, MomentumMode::Percentage, 0.0};
    require_no_signal(strategy, make_bar(0, 1e-300));
    REQUIRE_THROWS_AS(deliver(strategy, make_bar(1, 1e300)), std::overflow_error);
    require_signal(strategy, make_bar(2, 2e-300), SignalDirection::Long);
}

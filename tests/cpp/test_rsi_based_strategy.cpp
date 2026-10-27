
#include "backtester/strategy/rsi_based_strategy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using backtester::events::MarketEvent;
using backtester::events::SignalDirection;
using backtester::events::SignalEvent;
using backtester::indicators::RSIMode;
using backtester::market::Bar;
using backtester::market::SessionDate;
using backtester::strategy::PriceSource;
using backtester::strategy::RSIBasedStrategy;
using backtester::strategy::RSISignalMode;

SessionDate session_date(std::size_t offset) {
    const std::chrono::sys_days first{
        std::chrono::year{2026} / std::chrono::October / 1
    };
    return SessionDate{first + std::chrono::days{static_cast<int>(offset)}};
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

Bar make_source_bar(std::size_t index, double price, PriceSource source) {
    switch (source) {
        case PriceSource::Open:
            return make_ohlc(index, price, std::max(price, 5.0) + 1.0, 4.0, 5.0);
        case PriceSource::High:
            return make_ohlc(index, 5.0, price, 4.0, 5.0);
        case PriceSource::Low:
            return make_ohlc(index, 200.0, 220.0, price, 200.0);
        case PriceSource::Close:
            return make_ohlc(index, 5.0, std::max(price, 5.0) + 1.0, 4.0, price);
    }
    throw std::invalid_argument("Invalid test price source");
}

std::vector<SignalEvent> deliver(RSIBasedStrategy& strategy, const Bar& bar) {
    return strategy.on_market(MarketEvent{bar});
}

void require_none(RSIBasedStrategy& strategy, const Bar& bar) {
    REQUIRE(deliver(strategy, bar).empty());
}

void require_signal(
    RSIBasedStrategy& strategy,
    const Bar& bar,
    SignalDirection direction
) {
    const auto signals = deliver(strategy, bar);
    REQUIRE(signals.size() == 1);
    REQUIRE(signals.front().symbol() == bar.symbol());
    REQUIRE(signals.front().signal_date() == bar.session_date());
    REQUIRE(signals.front().direction() == direction);
    REQUIRE(signals.front().direction() != SignalDirection::Short);
}

struct SignalRecord final {
    std::size_t index;
    SignalDirection direction;
    SessionDate date;

    bool operator==(const SignalRecord&) const = default;
};

std::vector<SignalRecord> replay(
    RSIBasedStrategy& strategy,
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

void require_same(
    const std::vector<SignalRecord>& actual,
    const std::vector<SignalRecord>& expected
) {
    REQUIRE(actual.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(actual[i] == expected[i]);
    }
}

std::vector<Bar> oracle_bars() {
    const std::array<double, 11> prices{
        100, 99, 98, 102, 105, 104, 100, 98, 103, 105, 101
    };
    std::vector<Bar> bars;
    bars.reserve(prices.size());
    for (std::size_t i = 0; i < prices.size(); ++i) {
        bars.push_back(make_bar(i, prices[i]));
    }
    return bars;
}

void require_oracle(
    RSIMode rsi_mode,
    RSISignalMode signal_mode,
    const std::vector<std::pair<std::size_t, SignalDirection>>& expected
) {
    RSIBasedStrategy strategy{
        "AAPL", 2, rsi_mode, 30.0, 70.0,
        PriceSource::Close, signal_mode, 0.5
    };
    const auto bars = oracle_bars();
    const auto records = replay(strategy, bars);
    REQUIRE(records.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(records[i].index == expected[i].first);
        REQUIRE(records[i].direction == expected[i].second);
        REQUIRE(records[i].date == bars[expected[i].first].session_date());
    }
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

double selected_price(const Bar& bar, PriceSource source) {
    switch (source) {
        case PriceSource::Open: return bar.open();
        case PriceSource::High: return bar.high();
        case PriceSource::Low: return bar.low();
        case PriceSource::Close: return bar.close();
    }
    throw std::invalid_argument("Invalid test price source");
}

// Independent O(T) numerical oracle. It derives each price change from
// the raw bars and computes RSI with long-double seed/recurrence arithmetic.
// It does not call RelativeStrengthIndex or reuse the strategy state.
std::vector<SignalRecord> reference_replay(
    const std::vector<Bar>& bars,
    std::size_t period,
    RSIMode mode,
    double oversold,
    double overbought,
    PriceSource source,
    RSISignalMode signal_mode,
    std::optional<double> beta_override = std::nullopt
) {
    const long double beta = static_cast<long double>(
        beta_override.value_or(
            mode == RSIMode::Directional
                ? 2.0 / (static_cast<double>(period) + 1.0)
                : 1.0 / static_cast<double>(period)
        )
    );
    long double average_up = 0.0L;
    long double average_down = 0.0L;
    std::optional<long double> previous_rsi;
    auto target = SignalDirection::Flat;
    std::vector<SignalRecord> records;

    for (std::size_t i = 1; i < bars.size(); ++i) {
        const long double current = selected_price(bars[i], source);
        const long double previous = selected_price(bars[i - 1], source);
        const long double change = current - previous;
        const long double up = mode == RSIMode::Directional
            ? (change > 0.0L ? 1.0L : 0.0L)
            : std::max(change, 0.0L);
        const long double down = mode == RSIMode::Directional
            ? (change < 0.0L ? 1.0L : 0.0L)
            : std::max(-change, 0.0L);

        if (i <= period) {
            average_up += up / static_cast<long double>(period);
            average_down += down / static_cast<long double>(period);
        } else {
            average_up += beta * (up - average_up);
            average_down += beta * (down - average_down);
        }
        if (i < period) {
            continue;
        }

        long double rsi = 50.0L;
        if (average_up == 0.0L && average_down != 0.0L) {
            rsi = 0.0L;
        } else if (average_down == 0.0L && average_up != 0.0L) {
            rsi = 100.0L;
        } else if (average_up != 0.0L && average_down != 0.0L) {
            rsi = 100.0L * average_up / (average_up + average_down);
        }

        auto desired = target;
        if (signal_mode == RSISignalMode::DirectThreshold) {
            if (rsi < static_cast<long double>(oversold)) {
                desired = SignalDirection::Long;
            } else if (rsi > static_cast<long double>(overbought)) {
                desired = SignalDirection::Flat;
            }
        } else if (previous_rsi.has_value()) {
            if (*previous_rsi < oversold && rsi >= oversold) {
                desired = SignalDirection::Long;
            } else if (*previous_rsi > overbought && rsi <= overbought) {
                desired = SignalDirection::Flat;
            }
        }
        previous_rsi = rsi;
        if (desired != target) {
            records.push_back({i, desired, bars[i].session_date()});
            target = desired;
        }
    }
    return records;
}

}  // namespace

TEST_CASE("RSI strategy rejects empty symbols") {
    REQUIRE_THROWS_AS((RSIBasedStrategy{""}), std::invalid_argument);
}

TEST_CASE("RSI strategy rejects zero RSI periods") {
    REQUIRE_THROWS_AS((RSIBasedStrategy{"AAPL", 0}), std::invalid_argument);
}

TEST_CASE("RSI strategy rejects unrecognized RSI indicator modes") {
    REQUIRE_THROWS_AS(
        (RSIBasedStrategy{"AAPL", 2, static_cast<RSIMode>(99)}),
        std::invalid_argument
    );
}

TEST_CASE("RSI strategy rejects invalid smoothing beta overrides") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        for (const double beta : {0.0, -0.1, 1.01, nan, inf, -inf}) {
            REQUIRE_THROWS_AS(
                (RSIBasedStrategy{
                    "AAPL", 2, mode, 30.0, 70.0,
                    PriceSource::Close, RSISignalMode::DirectThreshold, beta
                }),
                std::invalid_argument
            );
        }
    }
}

TEST_CASE("RSI strategy supports beta one and independent indicator modes") {
    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        RSIBasedStrategy strategy{
            "AAPL", 1, mode, 30.0, 70.0,
            PriceSource::Close, RSISignalMode::DirectThreshold, 1.0
        };
        require_none(strategy, make_bar(0, 100));
        require_signal(strategy, make_bar(1, 90), SignalDirection::Long);
        require_signal(strategy, make_bar(2, 110), SignalDirection::Flat);
    }
}

TEST_CASE("RSI strategy rejects oversold thresholds outside the open range") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : {0.0, -0.0, -0.01, 100.0, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            (RSIBasedStrategy{"AAPL", 2, RSIMode::Directional, invalid, 70.0}),
            std::invalid_argument
        );
    }
}

TEST_CASE("RSI strategy rejects overbought thresholds outside the open range") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : {0.0, -0.0, -0.01, 100.0, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            (RSIBasedStrategy{"AAPL", 2, RSIMode::Directional, 30.0, invalid}),
            std::invalid_argument
        );
    }
}

TEST_CASE("RSI strategy requires strictly ordered oversold and overbought thresholds") {
    for (const auto& thresholds : {
        std::pair{30.0, 30.0}, std::pair{70.0, 30.0},
        std::pair{99.0, 1.0}
    }) {
        REQUIRE_THROWS_AS(
            (RSIBasedStrategy{
                "AAPL", 2, RSIMode::Directional,
                thresholds.first, thresholds.second
            }),
            std::invalid_argument
        );
    }
}

TEST_CASE("RSI strategy accepts asymmetric and near-boundary valid thresholds") {
    RSIBasedStrategy asymmetric{"AAPL", 1, RSIMode::Directional, 15.0, 85.0};
    RSIBasedStrategy near_limits{
        "AAPL", 1, RSIMode::Directional,
        std::nextafter(0.0, 1.0), std::nextafter(100.0, 0.0)
    };
    require_none(asymmetric, make_bar(0, 100));
    require_none(near_limits, make_bar(0, 100));
}

TEST_CASE("RSI strategy rejects invalid reference OHLC sources") {
    for (const auto invalid : {
        static_cast<PriceSource>(-1), static_cast<PriceSource>(99)
    }) {
        REQUIRE_THROWS_AS(
            (RSIBasedStrategy{
                "AAPL", 2, RSIMode::Directional, 30, 70, invalid
            }),
            std::invalid_argument
        );
    }
}

TEST_CASE("RSI strategy rejects unrecognized signal policies") {
    for (const auto invalid : {
        static_cast<RSISignalMode>(-1), static_cast<RSISignalMode>(99)
    }) {
        REQUIRE_THROWS_AS(
            (RSIBasedStrategy{
                "AAPL", 2, RSIMode::Directional, 30, 70,
                PriceSource::Close, invalid
            }),
            std::invalid_argument
        );
    }
}

TEST_CASE("RSI strategy defaults to Directional DirectThreshold and period fourteen") {
    RSIBasedStrategy strategy{"AAPL"};
    for (std::size_t i = 0; i < 14; ++i) {
        require_none(strategy, make_bar(i, 100.0 - static_cast<double>(i)));
    }
    require_signal(strategy, make_bar(14, 86), SignalDirection::Long);
}

TEST_CASE("RSI strategy RecoveryConfirmation first evaluates on observation sixteen") {
    RSIBasedStrategy strategy{
        "AAPL", 14, RSIMode::Directional, 30, 70,
        PriceSource::Close, RSISignalMode::RecoveryConfirmation, 1.0
    };
    for (std::size_t i = 0; i < 15; ++i) {
        require_none(strategy, make_bar(i, 100.0 - static_cast<double>(i)));
    }
    require_signal(strategy, make_bar(15, 100), SignalDirection::Long);
}

TEST_CASE("RSI strategy DirectThreshold period one becomes ready on price two") {
    RSIBasedStrategy strategy{"AAPL", 1};
    require_none(strategy, make_bar(0, 100));
    require_signal(strategy, make_bar(1, 90), SignalDirection::Long);
}

TEST_CASE("RSI strategy RecoveryConfirmation period one needs a third price") {
    RSIBasedStrategy strategy{
        "AAPL", 1, RSIMode::Directional, 30, 70,
        PriceSource::Close, RSISignalMode::RecoveryConfirmation
    };
    require_none(strategy, make_bar(0, 100));
    require_none(strategy, make_bar(1, 90));
    require_signal(strategy, make_bar(2, 110), SignalDirection::Long);
}

TEST_CASE("RSI strategy approved Directional DirectThreshold oracle") {
    require_oracle(RSIMode::Directional, RSISignalMode::DirectThreshold, {
        {2, SignalDirection::Long}, {4, SignalDirection::Flat},
        {6, SignalDirection::Long}, {9, SignalDirection::Flat}
    });
}

TEST_CASE("RSI strategy approved Magnitude DirectThreshold oracle") {
    require_oracle(RSIMode::Magnitude, RSISignalMode::DirectThreshold, {
        {2, SignalDirection::Long}, {3, SignalDirection::Flat},
        {6, SignalDirection::Long}, {8, SignalDirection::Flat}
    });
}

TEST_CASE("RSI strategy approved Directional RecoveryConfirmation oracle") {
    require_oracle(RSIMode::Directional, RSISignalMode::RecoveryConfirmation, {
        {3, SignalDirection::Long}, {5, SignalDirection::Flat},
        {8, SignalDirection::Long}, {10, SignalDirection::Flat}
    });
}

TEST_CASE("RSI strategy approved Magnitude RecoveryConfirmation oracle") {
    require_oracle(RSIMode::Magnitude, RSISignalMode::RecoveryConfirmation, {
        {3, SignalDirection::Long}, {5, SignalDirection::Flat},
        {8, SignalDirection::Long}, {10, SignalDirection::Flat}
    });
}

TEST_CASE("RSI strategy DirectThreshold retains Long throughout the neutral zone") {
    RSIBasedStrategy strategy{
        "AAPL", 1, RSIMode::Magnitude, 50, 80,
        PriceSource::Close, RSISignalMode::DirectThreshold, 0.5
    };
    require_none(strategy, make_bar(0, 100));
    require_signal(strategy, make_bar(1, 90), SignalDirection::Long); // RSI = 0.
    require_none(strategy, make_bar(2, 100)); // RSI = 50, not Flat.
    require_signal(strategy, make_bar(3, 125), SignalDirection::Flat); // RSI > 80.
}

TEST_CASE("RSI strategy DirectThreshold treats exact oversold equality as neutral") {
    RSIBasedStrategy strategy{
        "AAPL", 1, RSIMode::Magnitude, 50, 80,
        PriceSource::Close, RSISignalMode::DirectThreshold, 0.5
    };
    require_none(strategy, make_bar(0, 100));
    require_signal(strategy, make_bar(1, 90), SignalDirection::Long);
    require_none(strategy, make_bar(2, 100)); // RSI exactly 50.
}

TEST_CASE("RSI strategy DirectThreshold treats exact overbought equality as neutral") {
    RSIBasedStrategy strategy{
        "AAPL", 1, RSIMode::Magnitude, 30, 80,
        PriceSource::Close, RSISignalMode::DirectThreshold, 0.5
    };
    require_none(strategy, make_bar(0, 100));
    require_signal(strategy, make_bar(1, 90), SignalDirection::Long);
    require_none(strategy, make_bar(2, 130)); // RSI exactly 80.
    require_signal(strategy, make_bar(3, 140), SignalDirection::Flat);
}

TEST_CASE("RSI strategy initial neutral and overbought observations preserve Flat") {
    RSIBasedStrategy neutral{"AAPL", 2};
    RSIBasedStrategy overbought{"AAPL", 2};
    for (std::size_t i = 0; i < 3; ++i) {
        require_none(neutral, make_bar(i, 100.0));
    }
    require_none(overbought, make_bar(0, 100));
    require_none(overbought, make_bar(1, 110));
    require_none(overbought, make_bar(2, 120));
    require_none(overbought, make_bar(3, 100));
    require_signal(overbought, make_bar(4, 90), SignalDirection::Long);
}

TEST_CASE("RSI strategy preserves Long when price changes are initially neutral") {
    RSIBasedStrategy strategy{"AAPL", 1};
    require_none(strategy, make_bar(0, 100));
    require_signal(strategy, make_bar(1, 90), SignalDirection::Long);
    require_none(strategy, make_bar(2, 90)); // beta = 1, zero change => RSI 50.
    require_signal(strategy, make_bar(3, 100), SignalDirection::Flat);
}

TEST_CASE("RSI strategy suppresses repeated Long and Flat signals") {
    RSIBasedStrategy strategy{"AAPL", 1};
    const std::array<double, 7> prices{100, 90, 80, 70, 100, 110, 120};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 1) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_none(strategy, bar);
        }
    }
}

TEST_CASE("RSI strategy RecoveryConfirmation enters on an inclusive oversold recovery") {
    RSIBasedStrategy strategy{
        "AAPL", 1, RSIMode::Magnitude, 50, 80,
        PriceSource::Close, RSISignalMode::RecoveryConfirmation, 0.5
    };
    require_none(strategy, make_bar(0, 100));
    require_none(strategy, make_bar(1, 90)); // RSI 0, first available.
    require_signal(strategy, make_bar(2, 100), SignalDirection::Long); // RSI 50.
}

TEST_CASE("RSI strategy RecoveryConfirmation exits on an inclusive overbought decline") {
    RSIBasedStrategy strategy{
        "AAPL", 1, RSIMode::Magnitude, 50, 80,
        PriceSource::Close, RSISignalMode::RecoveryConfirmation, 0.5
    };
    require_none(strategy, make_bar(0, 100));
    require_none(strategy, make_bar(1, 90)); // 0.
    require_signal(strategy, make_bar(2, 100), SignalDirection::Long); // 50.
    require_none(strategy, make_bar(3, 125)); // > 80.
    require_signal(strategy, make_bar(4, 123.75), SignalDirection::Flat); // exactly 80.
}

TEST_CASE("RSI strategy RecoveryConfirmation requires previous RSI strictly below oversold") {
    RSIBasedStrategy strategy{
        "AAPL", 2, RSIMode::Magnitude, 50, 80,
        PriceSource::Close, RSISignalMode::RecoveryConfirmation, 0.5
    };
    require_none(strategy, make_bar(0, 100));
    require_none(strategy, make_bar(1, 110));
    require_none(strategy, make_bar(2, 100)); // first RSI = 50.
    require_none(strategy, make_bar(3, 110)); // RSI > 50; no crossing.
    require_none(strategy, make_bar(4, 80)); // RSI < 50.
    require_signal(strategy, make_bar(5, 100), SignalDirection::Long);
}

TEST_CASE("RSI strategy RecoveryConfirmation requires previous RSI strictly above overbought") {
    RSIBasedStrategy strategy{
        "AAPL", 1, RSIMode::Magnitude, 50, 80,
        PriceSource::Close, RSISignalMode::RecoveryConfirmation, 0.5
    };
    require_none(strategy, make_bar(0, 100));
    require_none(strategy, make_bar(1, 90));
    require_signal(strategy, make_bar(2, 100), SignalDirection::Long);
    require_none(strategy, make_bar(3, 115)); // RSI 80, not above.
    require_none(strategy, make_bar(4, 110)); // Drop below 80 must not exit.
    require_none(strategy, make_bar(5, 160)); // Above 80.
    require_signal(strategy, make_bar(6, 150), SignalDirection::Flat);
}

TEST_CASE("RSI strategy RecoveryConfirmation handles a jump across both thresholds") {
    RSIBasedStrategy strategy{
        "AAPL", 1, RSIMode::Directional, 30, 70,
        PriceSource::Close, RSISignalMode::RecoveryConfirmation
    };
    require_none(strategy, make_bar(0, 100));
    require_none(strategy, make_bar(1, 90)); // RSI 0.
    require_signal(strategy, make_bar(2, 110), SignalDirection::Long); // RSI 100.
    require_signal(strategy, make_bar(3, 80), SignalDirection::Flat); // RSI 0.
}

TEST_CASE("RSI strategy RecoveryConfirmation does not exit without an overbought crossing") {
    RSIBasedStrategy strategy{
        "AAPL", 1, RSIMode::Magnitude, 50, 80,
        PriceSource::Close, RSISignalMode::RecoveryConfirmation, 0.5
    };
    require_none(strategy, make_bar(0, 100));
    require_none(strategy, make_bar(1, 90));
    require_signal(strategy, make_bar(2, 100), SignalDirection::Long);
    require_none(strategy, make_bar(3, 95)); // RSI below oversold/overbought.
    require_none(strategy, make_bar(4, 90));
}

TEST_CASE("RSI strategy different smoothing beta changes signals after seeding") {
    RSIBasedStrategy fast{
        "AAPL", 2, RSIMode::Magnitude, 30, 70,
        PriceSource::Close, RSISignalMode::DirectThreshold, 1.0
    };
    RSIBasedStrategy slow{
        "AAPL", 2, RSIMode::Magnitude, 30, 70,
        PriceSource::Close, RSISignalMode::DirectThreshold, 0.1
    };
    require_none(fast, make_bar(0, 100));
    require_none(slow, make_bar(0, 100));
    require_none(fast, make_bar(1, 90));
    require_none(slow, make_bar(1, 90));
    require_signal(fast, make_bar(2, 80), SignalDirection::Long);
    require_signal(slow, make_bar(2, 80), SignalDirection::Long);
    require_signal(fast, make_bar(3, 90), SignalDirection::Flat);
    require_none(slow, make_bar(3, 90));
}

TEST_CASE("RSI strategy supports all four reference OHLC streams") {
    const std::array<double, 5> prices{100, 90, 80, 100, 110};
    for (const auto source : {
        PriceSource::Open, PriceSource::High,
        PriceSource::Low, PriceSource::Close
    }) {
        RSIBasedStrategy strategy{
            "AAPL", 1, RSIMode::Directional, 30, 70, source
        };
        for (std::size_t i = 0; i < prices.size(); ++i) {
            const auto bar = make_source_bar(i, prices[i], source);
            if (i == 1) {
                require_signal(strategy, bar, SignalDirection::Long);
            } else if (i == 3) {
                require_signal(strategy, bar, SignalDirection::Flat);
            } else {
                require_none(strategy, bar);
            }
        }
    }
}

TEST_CASE("RSI strategy defaults to Close rather than Open") {
    RSIBasedStrategy close_strategy{"AAPL", 1};
    RSIBasedStrategy open_strategy{
        "AAPL", 1, RSIMode::Directional, 30, 70, PriceSource::Open
    };
    require_none(close_strategy, make_ohlc(0, 120, 121, 90, 100));
    require_none(open_strategy, make_ohlc(0, 120, 121, 90, 100));
    const auto second = make_ohlc(1, 120, 121, 80, 90);
    require_signal(close_strategy, second, SignalDirection::Long);
    require_none(open_strategy, second);
}

TEST_CASE("RSI strategy unselected OHLC changes do not alter Close signals") {
    RSIBasedStrategy a{"AAPL", 3};
    RSIBasedStrategy b{"AAPL", 3};
    const auto bars = make_bars(200);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        const auto& original = bars[i];
        const auto altered = make_ohlc(
            i,
            original.close() + (i % 2 == 0 ? 4.0 : -4.0),
            original.high() + 6.0,
            original.low() - 5.0,
            original.close()
        );
        const auto first = deliver(a, original);
        const auto second = deliver(b, altered);
        REQUIRE(first.size() == second.size());
        for (std::size_t j = 0; j < first.size(); ++j) {
            REQUIRE(first[j].direction() == second[j].direction());
        }
    }
}

TEST_CASE("RSI strategy ignores other symbols throughout warm-up") {
    RSIBasedStrategy strategy{"AAPL", 2};
    require_none(strategy, make_bar(0, 100, "MSFT"));
    require_none(strategy, make_bar(1, 100));
    require_none(strategy, make_bar(2, 70, "MSFT"));
    require_none(strategy, make_bar(3, 90));
    require_signal(strategy, make_bar(4, 80), SignalDirection::Long);
}

TEST_CASE("RSI strategy ignores foreign symbols without changing recovery history") {
    RSIBasedStrategy strategy{
        "AAPL", 1, RSIMode::Directional, 30, 70,
        PriceSource::Close, RSISignalMode::RecoveryConfirmation
    };
    require_none(strategy, make_bar(0, 100));
    require_none(strategy, make_bar(1, 90));
    require_none(strategy, make_bar(2, 500, "MSFT"));
    require_signal(strategy, make_bar(3, 105), SignalDirection::Long);
    require_none(strategy, make_bar(4, 10, "MSFT"));
    require_signal(strategy, make_bar(5, 90), SignalDirection::Flat);
}

TEST_CASE("RSI strategy readiness counts observed bars rather than dates") {
    RSIBasedStrategy strategy{"AAPL", 2};
    require_none(strategy, make_bar(0, 100));
    require_none(strategy, make_bar(14, 90));
    require_signal(strategy, make_bar(100, 80), SignalDirection::Long);
}

TEST_CASE("RSI strategy different-symbol instances isolate their state") {
    RSIBasedStrategy a{"AAPL", 1};
    RSIBasedStrategy b{"MSFT", 1};
    require_none(a, make_bar(0, 100, "AAPL"));
    require_none(b, make_bar(0, 200, "MSFT"));
    require_signal(a, make_bar(1, 90, "AAPL"), SignalDirection::Long);
    require_none(b, make_bar(1, 210, "MSFT"));
    require_signal(b, make_bar(2, 190, "MSFT"), SignalDirection::Long);
    require_signal(a, make_bar(2, 110, "AAPL"), SignalDirection::Flat);
}

TEST_CASE("RSI strategy same-symbol configurations isolate states and policies") {
    RSIBasedStrategy direct{"AAPL", 1};
    RSIBasedStrategy recovery{
        "AAPL", 1, RSIMode::Directional, 30, 70,
        PriceSource::Close, RSISignalMode::RecoveryConfirmation
    };
    require_none(direct, make_bar(0, 100));
    require_none(recovery, make_bar(0, 100));
    require_signal(direct, make_bar(1, 90), SignalDirection::Long);
    require_none(recovery, make_bar(1, 90));
    require_signal(direct, make_bar(2, 110), SignalDirection::Flat);
    require_signal(recovery, make_bar(2, 110), SignalDirection::Long);
}

TEST_CASE("RSI strategy replays identical events deterministically") {
    const auto bars = make_bars(300);
    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        for (const auto policy : {
            RSISignalMode::DirectThreshold, RSISignalMode::RecoveryConfirmation
        }) {
            RSIBasedStrategy a{"AAPL", 5, mode, 30, 70, PriceSource::Close, policy, 0.4};
            RSIBasedStrategy b{"AAPL", 5, mode, 30, 70, PriceSource::Close, policy, 0.4};
            require_same(replay(a, bars), replay(b, bars));
        }
    }
}

TEST_CASE("RSI strategy matches independent numeric oracle across configurations") {
    const auto bars = make_bars(500);
    for (const auto period : {1U, 2U, 5U, 14U}) {
        for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
            for (const auto policy : {
                RSISignalMode::DirectThreshold, RSISignalMode::RecoveryConfirmation
            }) {
                for (const auto source : {
                    PriceSource::Open, PriceSource::High,
                    PriceSource::Low, PriceSource::Close
                }) {
                    for (const auto beta : {
                        std::optional<double>{},
                        std::optional<double>{0.25},
                        std::optional<double>{0.75}
                    }) {
                        RSIBasedStrategy strategy{
                            "AAPL", period, mode, 23.25, 76.75,
                            source, policy, beta
                        };
                        require_same(
                            replay(strategy, bars),
                            reference_replay(
                                bars, period, mode, 23.25, 76.75,
                                source, policy, beta
                            )
                        );
                    }
                }
            }
        }
    }
}

TEST_CASE("RSI strategy early decisions are independent of future observations") {
    const auto bars = make_bars(250);
    const std::vector<Bar> prefix(bars.begin(), bars.begin() + 90);
    for (const auto policy : {
        RSISignalMode::DirectThreshold, RSISignalMode::RecoveryConfirmation
    }) {
        RSIBasedStrategy short_run{
            "AAPL", 7, RSIMode::Magnitude, 30, 70, PriceSource::Close, policy, 0.4
        };
        RSIBasedStrategy full_run{
            "AAPL", 7, RSIMode::Magnitude, 30, 70, PriceSource::Close, policy, 0.4
        };
        const auto before = replay(short_run, prefix);
        const auto after = replay(full_run, bars);
        REQUIRE(after.size() >= before.size());
        for (std::size_t i = 0; i < before.size(); ++i) {
            REQUIRE(before[i] == after[i]);
        }
    }
}

TEST_CASE("RSI strategy agrees with independent oracle over thousands of updates") {
    const auto bars = make_bars(5'000);
    for (const auto mode : {RSIMode::Directional, RSIMode::Magnitude}) {
        for (const auto policy : {
            RSISignalMode::DirectThreshold, RSISignalMode::RecoveryConfirmation
        }) {
            RSIBasedStrategy strategy{
                "AAPL", 14, mode, 30, 70, PriceSource::Close, policy
            };
            require_same(
                replay(strategy, bars),
                reference_replay(bars, 14, mode, 30, 70, PriceSource::Close, policy)
            );
        }
    }
}

TEST_CASE("RSI strategy Directional handles extreme finite positive prices") {
    const double max = std::numeric_limits<double>::max();
    RSIBasedStrategy strategy{"AAPL", 1, RSIMode::Directional};
    require_none(strategy, make_bar(0, max));
    require_signal(strategy, make_bar(1, max / 4.0), SignalDirection::Long);
    require_signal(strategy, make_bar(2, max), SignalDirection::Flat);
}

#include "backtester/strategy/sma_crossover_strategy.hpp"

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
#include <vector>

namespace {

using backtester::events::MarketEvent;
using backtester::events::SignalDirection;
using backtester::events::SignalEvent;
using backtester::market::Bar;
using backtester::market::SessionDate;
using backtester::strategy::PriceSource;
using backtester::strategy::SMACrossoverStrategy;

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

// The chosen field follows the oracle series. All other OHLC fields are
// deliberately different, while the Bar remains valid (positive and ordered).
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
                20.0, 21.0, observation, 20.0, 1'000
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

std::vector<SignalEvent> deliver(SMACrossoverStrategy& strategy, const Bar& bar) {
    return strategy.on_market(MarketEvent{bar});
}

void require_no_signal(SMACrossoverStrategy& strategy, const Bar& bar) {
    const auto signals = deliver(strategy, bar);
    REQUIRE(signals.empty());
}

void require_signal(
    SMACrossoverStrategy& strategy,
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
    std::size_t observation_index;
    SignalDirection direction;
    SessionDate date;

    bool operator==(const SignalRecord&) const = default;
};

std::vector<SignalRecord> replay(
    SMACrossoverStrategy& strategy,
    const std::vector<double>& prices,
    std::size_t first_date_offset = 0
) {
    std::vector<SignalRecord> result;
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(first_date_offset + i, prices[i]);
        for (const auto& signal : deliver(strategy, bar)) {
            REQUIRE(signal.symbol() == "AAPL");
            REQUIRE(signal.signal_date() == bar.session_date());
            REQUIRE(signal.direction() != SignalDirection::Short);
            result.push_back({i, signal.direction(), signal.signal_date()});
        }
    }
    return result;
}

void require_same_records(
    const std::vector<SignalRecord>& actual,
    const std::vector<SignalRecord>& expected
) {
    REQUIRE(actual.size() == expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        REQUIRE(actual[i] == expected[i]);
    }
}

}  // namespace

TEST_CASE("SMA crossover rejects an empty symbol") {
    REQUIRE_THROWS_AS(
        (SMACrossoverStrategy{"", 2, 3}),
        std::invalid_argument
    );
}

TEST_CASE("SMA crossover rejects invalid fast and slow periods") {
    REQUIRE_THROWS_AS(
        (SMACrossoverStrategy{"AAPL", 0, 3}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        (SMACrossoverStrategy{"AAPL", 2, 0}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        (SMACrossoverStrategy{"AAPL", 3, 3}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        (SMACrossoverStrategy{"AAPL", 4, 3}),
        std::invalid_argument
    );
}

TEST_CASE("SMA crossover rejects negative and nonfinite thresholds") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double threshold : {-0.1, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            (SMACrossoverStrategy{"AAPL", 2, 3, PriceSource::Close, threshold}),
            std::invalid_argument
        );
    }
}

TEST_CASE("SMA crossover rejects an invalid price source") {
    REQUIRE_THROWS_AS(
        (SMACrossoverStrategy{"AAPL", 2, 3, static_cast<PriceSource>(-1)}),
        std::invalid_argument
    );
}

TEST_CASE("SMA crossover permits fast period one and negative zero threshold") {
    SMACrossoverStrategy strategy{"AAPL", 1, 2, PriceSource::Close, -0.0};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_signal(strategy, make_bar(1, 12.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(2, 12.0));
    require_signal(strategy, make_bar(3, 8.0), SignalDirection::Flat);
}

TEST_CASE("SMA crossover waits for the slow moving average") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(1, 11.0));
    require_signal(strategy, make_bar(2, 12.0), SignalDirection::Long);
}

TEST_CASE("SMA crossover matches the approved seven-price oracle") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3};
    const std::array<double, 7> close{10.0, 11.0, 12.0, 10.0, 9.0, 13.0, 13.0};
    for (std::size_t i = 0; i < close.size(); ++i) {
        const auto bar = make_bar(i, close[i]);
        if (i == 2 || i == 5) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("SMA crossover first eligible bearish regime remains initially Flat") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3};
    for (std::size_t i = 0; i < 4; ++i) {
        require_no_signal(strategy, make_bar(i, 20.0 - static_cast<double>(i)));
    }
    require_signal(strategy, make_bar(4, 30.0), SignalDirection::Long);
}

TEST_CASE("SMA crossover first eligible equality does not request Flat") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(1, 10.0));
    require_no_signal(strategy, make_bar(2, 10.0));
    require_signal(strategy, make_bar(3, 16.0), SignalDirection::Long);
}

TEST_CASE("SMA crossover equality preserves an existing Long target") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(1, 11.0));
    require_signal(strategy, make_bar(2, 12.0), SignalDirection::Long);
    // SMA(2) = SMA(3) = 11: equality is neutral, not an exit.
    require_no_signal(strategy, make_bar(3, 10.0));
    require_signal(strategy, make_bar(4, 9.0), SignalDirection::Flat);
}

TEST_CASE("SMA crossover emits no repeated Long or Flat signals") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3};
    const std::array<double, 9> closes{
        10.0, 11.0, 12.0, 13.0, 14.0, 8.0, 7.0, 6.0, 5.0
    };
    for (std::size_t i = 0; i < closes.size(); ++i) {
        const auto bar = make_bar(i, closes[i]);
        if (i == 2) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 5) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("SMA crossover threshold filters small differences and allows transitions") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3, PriceSource::Close, 0.1};
    const std::array<double, 7> closes{10.0, 11.0, 12.0, 20.0, 18.0, 17.0, 1.0};
    for (std::size_t i = 0; i < closes.size(); ++i) {
        const auto bar = make_bar(i, closes[i]);
        if (i == 3) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 6) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("SMA crossover upper threshold boundary is strictly neutral") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3, PriceSource::Close, 0.1};
    require_no_signal(strategy, make_bar(0, 8.0));
    require_no_signal(strategy, make_bar(1, 11.0));
    // Fast = 11, slow = 10, D = exactly +0.1.
    require_no_signal(strategy, make_bar(2, 11.0));
    require_no_signal(strategy, make_bar(3, 11.0));
    require_no_signal(strategy, make_bar(4, 18.0));
    require_signal(strategy, make_bar(5, 35.0), SignalDirection::Long);
}

TEST_CASE("SMA crossover lower threshold boundary preserves Long") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3, PriceSource::Close, 0.1};
    require_no_signal(strategy, make_bar(0, 1.0));
    require_no_signal(strategy, make_bar(1, 12.0));
    require_signal(strategy, make_bar(2, 14.0), SignalDirection::Long);
    // Fast = 9, slow = 10, D = exactly -0.1.
    require_no_signal(strategy, make_bar(3, 4.0));
    require_signal(strategy, make_bar(4, 1.0), SignalDirection::Flat);
}

TEST_CASE("SMA crossover compares relative rather than absolute spreads") {
    SMACrossoverStrategy small{"AAPL", 2, 3, PriceSource::Close, 0.04};
    SMACrossoverStrategy large{"AAPL", 2, 3, PriceSource::Close, 0.04};
    for (std::size_t i = 0; i < 3; ++i) {
        const double close = 10.0 + static_cast<double>(i);
        const auto small_signals = deliver(small, make_bar(i, close));
        const auto large_signals = deliver(large, make_bar(i, 10.0 * close));
        REQUIRE(small_signals.size() == large_signals.size());
        if (i == 2) {
            REQUIRE(small_signals.size() == 1);
            REQUIRE(small_signals.front().direction() == SignalDirection::Long);
            REQUIRE(large_signals.front().direction() == SignalDirection::Long);
        } else {
            REQUIRE(small_signals.empty());
        }
    }
}

TEST_CASE("SMA crossover supports each configured OHLC price source") {
    constexpr std::array<double, 7> observations{
        10.0, 11.0, 12.0, 10.0, 9.0, 13.0, 13.0
    };
    for (const auto source : {
        PriceSource::Open,
        PriceSource::High,
        PriceSource::Low,
        PriceSource::Close
    }) {
        SMACrossoverStrategy strategy{"AAPL", 2, 3, source};
        for (std::size_t i = 0; i < observations.size(); ++i) {
            const auto bar = make_source_bar(i, observations[i], source);
            if (i == 2 || i == 5) {
                require_signal(strategy, bar, SignalDirection::Long);
            } else if (i == 4) {
                require_signal(strategy, bar, SignalDirection::Flat);
            } else {
                require_no_signal(strategy, bar);
            }
        }
    }
}

TEST_CASE("SMA crossover default source observes Close rather than Open") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3};
    // Opens decrease, but Closes increase: the default must enter Long.
    require_no_signal(strategy, Bar{session_date(0), "AAPL", 30, 31, 9, 10, 1'000});
    require_no_signal(strategy, Bar{session_date(1), "AAPL", 20, 21, 10, 11, 1'000});
    require_signal(
        strategy,
        Bar{session_date(2), "AAPL", 15, 16, 11, 12, 1'000},
        SignalDirection::Long
    );
}

TEST_CASE("SMA crossover ignores changes in unselected OHLC fields") {
    SMACrossoverStrategy simple{"AAPL", 2, 3};
    SMACrossoverStrategy varied{"AAPL", 2, 3};
    constexpr std::array<double, 7> prices{10, 11, 12, 10, 9, 13, 13};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const double price = prices[i];
        const auto expected = deliver(simple, make_bar(i, price));
        const double open = price + (i % 2 == 0 ? 2.0 : -2.0);
        const Bar variable{
            session_date(i), "AAPL", open,
            std::max(open, price) + 3.0,
            std::min(open, price) - 3.0,
            price, static_cast<std::uint64_t>(2'000 + i)
        };
        const auto actual = deliver(varied, variable);
        REQUIRE(actual.size() == expected.size());
        if (!expected.empty()) {
            REQUIRE(actual.front().direction() == expected.front().direction());
            REQUIRE(actual.front().signal_date() == expected.front().signal_date());
        }
    }
}

TEST_CASE("SMA crossover ignores foreign symbols without advancing warm-up") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(1, 11.0, "MSFT"));
    require_no_signal(strategy, make_bar(2, 11.0));
    require_no_signal(strategy, make_bar(3, 100.0, "MSFT"));
    require_signal(strategy, make_bar(4, 12.0), SignalDirection::Long);
}

TEST_CASE("SMA crossover ignores foreign symbols without changing a ready target") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(1, 11.0));
    require_signal(strategy, make_bar(2, 12.0), SignalDirection::Long);
    for (std::size_t i = 3; i < 8; ++i) {
        require_no_signal(strategy, make_bar(i, 1.0, "MSFT"));
    }
    require_no_signal(strategy, make_bar(8, 10.0));
    require_signal(strategy, make_bar(9, 9.0), SignalDirection::Flat);
}

TEST_CASE("SMA crossover warm-up counts observed bars rather than calendar days") {
    SMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(10, 11.0));
    require_signal(strategy, make_bar(30, 12.0), SignalDirection::Long);
}

TEST_CASE("SMA crossover defaults become ready on the fiftieth bar") {
    SMACrossoverStrategy strategy{"AAPL"};
    for (std::size_t i = 0; i < 49; ++i) {
        require_no_signal(strategy, make_bar(i, 100.0 + static_cast<double>(i)));
    }
    require_signal(strategy, make_bar(49, 149.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(50, 150.0));
}

TEST_CASE("SMA crossover instances for different symbols have isolated state") {
    SMACrossoverStrategy apple{"AAPL", 2, 3};
    SMACrossoverStrategy microsoft{"MSFT", 2, 3};
    for (std::size_t i = 0; i < 3; ++i) {
        const auto rising = make_bar(i, 10.0 + static_cast<double>(i), "AAPL");
        const auto falling = make_bar(i, 12.0 - static_cast<double>(i), "MSFT");
        if (i == 2) {
            require_signal(apple, rising, SignalDirection::Long);
        } else {
            require_no_signal(apple, rising);
        }
        require_no_signal(microsoft, falling);
    }
    require_signal(microsoft, make_bar(3, 20.0, "MSFT"), SignalDirection::Long);
    require_no_signal(apple, make_bar(3, 13.0, "AAPL"));
}

TEST_CASE("SMA crossover instances for the same symbol have isolated state") {
    SMACrossoverStrategy first{"AAPL", 2, 3};
    SMACrossoverStrategy second{"AAPL", 2, 3};
    require_no_signal(first, make_bar(0, 10.0));
    require_no_signal(first, make_bar(1, 11.0));
    require_signal(first, make_bar(2, 12.0), SignalDirection::Long);

    require_no_signal(second, make_bar(0, 12.0));
    require_no_signal(second, make_bar(1, 11.0));
    require_no_signal(second, make_bar(2, 10.0));
    require_signal(second, make_bar(3, 20.0), SignalDirection::Long);
    require_no_signal(first, make_bar(3, 13.0));
}

TEST_CASE("SMA crossover is deterministic across independent replays") {
    SMACrossoverStrategy first{"AAPL", 2, 3, PriceSource::Close, 0.03};
    SMACrossoverStrategy second{"AAPL", 2, 3, PriceSource::Close, 0.03};
    const std::vector<double> prices{10, 11, 12, 10, 9, 13, 13, 11, 8, 14, 15, 9, 7};
    const auto first_signals = replay(first, prices);
    const auto second_signals = replay(second, prices);
    require_same_records(first_signals, second_signals);
    REQUIRE_FALSE(first_signals.empty());
}

TEST_CASE("SMA crossover remains stable over hundreds of rolling updates") {
    SMACrossoverStrategy first{"AAPL", 2, 5};
    SMACrossoverStrategy second{"AAPL", 2, 5};
    const std::array<double, 10> pattern{10, 11, 14, 18, 15, 12, 9, 7, 8, 10};
    std::vector<double> prices;
    prices.reserve(500);
    for (std::size_t i = 0; i < 500; ++i) {
        prices.push_back(pattern[i % pattern.size()]);
    }
    const auto first_signals = replay(first, prices);
    const auto second_signals = replay(second, prices);
    require_same_records(first_signals, second_signals);
    REQUIRE(first_signals.size() > 5);
    for (std::size_t i = 1; i < first_signals.size(); ++i) {
        REQUIRE(first_signals[i].observation_index > first_signals[i - 1].observation_index);
        REQUIRE(first_signals[i].direction != first_signals[i - 1].direction);
    }
}

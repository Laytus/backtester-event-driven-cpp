#include "backtester/strategy/ema_crossover_strategy.hpp"

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
using backtester::strategy::EMACrossoverStrategy;
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

// Selected OHLC field equals observation. The remaining fields differ while
// preserving positive and internally consistent OHLC values.
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

std::vector<SignalEvent> deliver(EMACrossoverStrategy& strategy, const Bar& bar) {
    return strategy.on_market(MarketEvent{bar});
}

void require_no_signal(EMACrossoverStrategy& strategy, const Bar& bar) {
    REQUIRE(deliver(strategy, bar).empty());
}

void require_signal(
    EMACrossoverStrategy& strategy,
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
    EMACrossoverStrategy& strategy,
    const std::vector<double>& prices,
    std::size_t first_date_offset = 0
) {
    std::vector<SignalRecord> records;
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(first_date_offset + i, prices[i]);
        for (const auto& signal : deliver(strategy, bar)) {
            REQUIRE(signal.symbol() == "AAPL");
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
    for (std::size_t i = 0; i < actual.size(); ++i) {
        REQUIRE(actual[i] == expected[i]);
    }
}

// Independent reference implementation: long-double SMA seed followed by the
// standard EMA recurrence. It never invokes the production EMA indicators.
struct ReferenceEMA final {
    std::size_t period;
    long double alpha;
    std::size_t count = 0;
    long double seed_sum = 0.0L;
    std::optional<long double> current;

    void update(long double observation) {
        if (count < period) {
            seed_sum += observation;
            ++count;
            if (count == period) {
                current = seed_sum / static_cast<long double>(period);
            }
            return;
        }
        *current = *current + alpha * (observation - *current);
    }
};

std::vector<SignalRecord> reference_replay(
    const std::vector<double>& prices,
    std::size_t fast_period,
    std::size_t slow_period,
    long double fast_alpha,
    long double slow_alpha,
    long double threshold
) {
    ReferenceEMA fast{fast_period, fast_alpha, 0, 0.0L, std::nullopt};
    ReferenceEMA slow{slow_period, slow_alpha, 0, 0.0L, std::nullopt};
    auto target = SignalDirection::Flat;
    std::vector<SignalRecord> records;
    for (std::size_t i = 0; i < prices.size(); ++i) {
        fast.update(static_cast<long double>(prices[i]));
        slow.update(static_cast<long double>(prices[i]));
        if (!fast.current || !slow.current) {
            continue;
        }
        const long double spread = (*fast.current - *slow.current) / *slow.current;
        auto desired = target;
        if (spread > threshold) {
            desired = SignalDirection::Long;
        } else if (spread < -threshold) {
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

TEST_CASE("EMA crossover rejects an empty symbol") {
    REQUIRE_THROWS_AS((EMACrossoverStrategy{"", 2, 3}), std::invalid_argument);
}

TEST_CASE("EMA crossover rejects invalid fast and slow periods") {
    REQUIRE_THROWS_AS((EMACrossoverStrategy{"AAPL", 0, 3}), std::invalid_argument);
    REQUIRE_THROWS_AS((EMACrossoverStrategy{"AAPL", 2, 0}), std::invalid_argument);
    REQUIRE_THROWS_AS((EMACrossoverStrategy{"AAPL", 3, 3}), std::invalid_argument);
    REQUIRE_THROWS_AS((EMACrossoverStrategy{"AAPL", 4, 3}), std::invalid_argument);
}

TEST_CASE("EMA crossover rejects negative and nonfinite thresholds") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double threshold : {-0.1, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            (EMACrossoverStrategy{"AAPL", 2, 3, PriceSource::Close, threshold}),
            std::invalid_argument
        );
    }
}

TEST_CASE("EMA crossover rejects invalid price sources") {
    REQUIRE_THROWS_AS(
        (EMACrossoverStrategy{"AAPL", 2, 3, static_cast<PriceSource>(-1)}),
        std::invalid_argument
    );
}

TEST_CASE("EMA crossover rejects invalid fast alpha overrides") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double alpha : {0.0, -0.1, 1.0001, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            (EMACrossoverStrategy{"AAPL", 2, 3, PriceSource::Close, 0.0, alpha}),
            std::invalid_argument
        );
    }
}

TEST_CASE("EMA crossover rejects invalid slow alpha overrides") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double alpha : {0.0, -0.1, 1.0001, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            (EMACrossoverStrategy{
                "AAPL", 2, 3, PriceSource::Close, 0.0, std::nullopt, alpha
            }),
            std::invalid_argument
        );
    }
}

TEST_CASE("EMA crossover accepts alpha one and independent valid overrides") {
    EMACrossoverStrategy strategy{
        "AAPL", 2, 3, PriceSource::Close, 0.0, 0.1, 1.0
    };
    const auto records = replay(strategy, {10, 11, 12, 10, 9, 13});
    REQUIRE(records.size() >= 1);
    REQUIRE(records.front().direction == SignalDirection::Long);
}

TEST_CASE("EMA crossover accepts period one and negative zero threshold") {
    EMACrossoverStrategy strategy{"AAPL", 1, 2, PriceSource::Close, -0.0};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_signal(strategy, make_bar(1, 12.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(2, 12.0));
    require_signal(strategy, make_bar(3, 8.0), SignalDirection::Flat);
}

TEST_CASE("EMA crossover waits until the slow EMA has seeded") {
    EMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(1, 11.0));
    require_signal(strategy, make_bar(2, 12.0), SignalDirection::Long);
}

TEST_CASE("EMA crossover matches the approved seven-price oracle") {
    EMACrossoverStrategy strategy{"AAPL", 2, 3};
    const std::array<double, 7> prices{10, 11, 12, 10, 9, 13, 13};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 2 || i == 5) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("EMA crossover initially bearish stays Flat until bullish") {
    EMACrossoverStrategy strategy{"AAPL", 2, 3};
    for (std::size_t i = 0; i < 4; ++i) {
        require_no_signal(strategy, make_bar(i, 20.0 - static_cast<double>(i)));
    }
    require_signal(strategy, make_bar(4, 30.0), SignalDirection::Long);
}

TEST_CASE("EMA crossover first eligible equality does not emit Flat") {
    EMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(1, 10.0));
    require_no_signal(strategy, make_bar(2, 10.0));
    require_signal(strategy, make_bar(3, 16.0), SignalDirection::Long);
}

TEST_CASE("EMA crossover equality preserves an existing Long target") {
    EMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(1, 11.0));
    require_signal(strategy, make_bar(2, 12.0), SignalDirection::Long);
    // At t=4, fast and slow EMAs are both exactly 10.5.
    require_no_signal(strategy, make_bar(3, 10.0));
    require_signal(strategy, make_bar(4, 9.0), SignalDirection::Flat);
}

TEST_CASE("EMA crossover emits no repeated Long or Flat targets") {
    EMACrossoverStrategy strategy{"AAPL", 2, 3};
    const std::array<double, 9> prices{10, 11, 12, 13, 14, 8, 7, 6, 5};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 2) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 5) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("EMA crossover threshold filters small spreads and permits transitions") {
    EMACrossoverStrategy strategy{"AAPL", 2, 3, PriceSource::Close, 0.1};
    const std::array<double, 7> prices{10, 11, 12, 20, 18, 17, 1};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 3) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 6) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("EMA crossover upper threshold boundary is strictly neutral") {
    // EMA(1) is the current price, EMA(2) has SMA seed (6+10)/2 = 8.
    // Thus D = (10-8)/8 = +0.25, exactly representable in binary.
    EMACrossoverStrategy strategy{"AAPL", 1, 2, PriceSource::Close, 0.25};
    require_no_signal(strategy, make_bar(0, 6.0));
    require_no_signal(strategy, make_bar(1, 10.0));
    require_signal(strategy, make_bar(2, 24.0), SignalDirection::Long);
}

TEST_CASE("EMA crossover lower threshold boundary preserves Long") {
    // Initial D = (20-15)/15 > 0.25, followed by D = (9-12)/12 = -0.25.
    EMACrossoverStrategy strategy{
        "AAPL", 1, 2, PriceSource::Close, 0.25,
        std::nullopt, 0.5
    };
    require_no_signal(strategy, make_bar(0, 10.0));
    require_signal(strategy, make_bar(1, 20.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(2, 9.0));
    require_signal(strategy, make_bar(3, 5.0), SignalDirection::Flat);
}

TEST_CASE("EMA crossover uses relative rather than absolute spread") {
    EMACrossoverStrategy small{"AAPL", 2, 3, PriceSource::Close, 0.04};
    EMACrossoverStrategy large{"AAPL", 2, 3, PriceSource::Close, 0.04};
    for (std::size_t i = 0; i < 3; ++i) {
        const double price = 10.0 + static_cast<double>(i);
        const auto small_signals = deliver(small, make_bar(i, price));
        const auto large_signals = deliver(large, make_bar(i, price * 100.0));
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

TEST_CASE("EMA crossover supports all configured OHLC price sources") {
    constexpr std::array<double, 7> observations{10, 11, 12, 10, 9, 13, 13};
    for (const auto source : {
        PriceSource::Open, PriceSource::High,
        PriceSource::Low, PriceSource::Close
    }) {
        EMACrossoverStrategy strategy{"AAPL", 2, 3, source};
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

TEST_CASE("EMA crossover defaults to Close rather than Open") {
    EMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(
        strategy, Bar{session_date(0), "AAPL", 30, 31, 9, 10, 1'000}
    );
    require_no_signal(
        strategy, Bar{session_date(1), "AAPL", 20, 21, 10, 11, 1'000}
    );
    require_signal(
        strategy, Bar{session_date(2), "AAPL", 15, 16, 11, 12, 1'000},
        SignalDirection::Long
    );
}

TEST_CASE("EMA crossover ignores unselected OHLC changes") {
    EMACrossoverStrategy baseline{"AAPL", 2, 3};
    EMACrossoverStrategy modified{"AAPL", 2, 3};
    const std::array<double, 7> prices{10, 11, 12, 10, 9, 13, 13};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const double price = prices[i];
        const auto expected = deliver(baseline, make_bar(i, price));
        const double open = price + (i % 2 == 0 ? 2.0 : -2.0);
        const Bar varied{
            session_date(i), "AAPL", open,
            std::max(open, price) + 3.0,
            std::min(open, price) - 3.0,
            price, static_cast<std::uint64_t>(2'000 + i)
        };
        const auto actual = deliver(modified, varied);
        REQUIRE(actual.size() == expected.size());
        if (!expected.empty()) {
            REQUIRE(actual.front().direction() == expected.front().direction());
            REQUIRE(actual.front().signal_date() == expected.front().signal_date());
        }
    }
}

TEST_CASE("EMA crossover ignores foreign symbols without advancing warm-up") {
    EMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(1, 11.0, "MSFT"));
    require_no_signal(strategy, make_bar(2, 11.0));
    require_no_signal(strategy, make_bar(3, 100.0, "MSFT"));
    require_signal(strategy, make_bar(4, 12.0), SignalDirection::Long);
}

TEST_CASE("EMA crossover ignores foreign symbols without changing a ready target") {
    EMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(1, 11.0));
    require_signal(strategy, make_bar(2, 12.0), SignalDirection::Long);
    for (std::size_t i = 3; i < 8; ++i) {
        require_no_signal(strategy, make_bar(i, 1.0, "MSFT"));
    }
    require_no_signal(strategy, make_bar(8, 10.0));
    require_signal(strategy, make_bar(9, 9.0), SignalDirection::Flat);
}

TEST_CASE("EMA crossover warm-up counts bars rather than calendar days") {
    EMACrossoverStrategy strategy{"AAPL", 2, 3};
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(10, 11.0));
    require_signal(strategy, make_bar(30, 12.0), SignalDirection::Long);
}

TEST_CASE("EMA crossover default periods are ready on bar fifty") {
    EMACrossoverStrategy strategy{"AAPL"};
    for (std::size_t i = 0; i < 49; ++i) {
        require_no_signal(strategy, make_bar(i, 100.0 + static_cast<double>(i)));
    }
    require_signal(strategy, make_bar(49, 149.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(50, 150.0));
}

TEST_CASE("EMA crossover fast alpha override changes the recurrence") {
    EMACrossoverStrategy standard{"AAPL", 2, 3};
    EMACrossoverStrategy slow_fast_ema{
        "AAPL", 2, 3, PriceSource::Close, 0.0, 0.1
    };
    const std::array<double, 5> prices{10, 11, 12, 10, 9};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 2) {
            require_signal(standard, bar, SignalDirection::Long);
            require_no_signal(slow_fast_ema, bar);
        } else if (i == 3) {
            require_no_signal(standard, bar);
            require_signal(slow_fast_ema, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(standard, bar, SignalDirection::Flat);
            require_no_signal(slow_fast_ema, bar);
        } else {
            require_no_signal(standard, bar);
            require_no_signal(slow_fast_ema, bar);
        }
    }
}

TEST_CASE("EMA crossover slow alpha override changes the recurrence") {
    EMACrossoverStrategy standard{"AAPL", 2, 3};
    EMACrossoverStrategy instant_slow{
        "AAPL", 2, 3, PriceSource::Close, 0.0, std::nullopt, 1.0
    };
    const std::array<double, 5> prices{10, 11, 12, 10, 9};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 2) {
            require_signal(standard, bar, SignalDirection::Long);
            require_signal(instant_slow, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(standard, bar, SignalDirection::Flat);
            require_no_signal(instant_slow, bar);
        } else {
            require_no_signal(standard, bar);
            require_no_signal(instant_slow, bar);
        }
    }
}

TEST_CASE("EMA crossover accepts unordered independent alpha overrides") {
    EMACrossoverStrategy strategy{
        "AAPL", 2, 3, PriceSource::Close, 0.0, 0.25, 0.75
    };
    const std::array<double, 7> prices{10, 11, 12, 10, 9, 13, 13};
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

TEST_CASE("EMA crossover slow alpha overrides preserve the slow SMA seed") {
    EMACrossoverStrategy standard{"AAPL", 2, 3};
    EMACrossoverStrategy custom{
        "AAPL", 2, 3, PriceSource::Close, 0.0, std::nullopt, 0.01
    };
    const std::array<double, 3> prices{10, 11, 12};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i]);
        if (i == 2) {
            // At readiness, the slow EMA is SMA(3) = 11 regardless of alpha.
            require_signal(standard, bar, SignalDirection::Long);
            require_signal(custom, bar, SignalDirection::Long);
        } else {
            require_no_signal(standard, bar);
            require_no_signal(custom, bar);
        }
    }
}

TEST_CASE("EMA crossover period one honors a custom alpha after its seed") {
    EMACrossoverStrategy strategy{
        "AAPL", 1, 2, PriceSource::Close, 0.0, 0.5
    };
    require_no_signal(strategy, make_bar(0, 10.0));
    require_no_signal(strategy, make_bar(1, 12.0));
    require_no_signal(strategy, make_bar(2, 14.0));
    require_signal(strategy, make_bar(3, 8.0), SignalDirection::Long);
}

TEST_CASE("EMA crossover instances for distinct symbols isolate state") {
    EMACrossoverStrategy apple{"AAPL", 2, 3};
    EMACrossoverStrategy microsoft{"MSFT", 2, 3};
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

TEST_CASE("EMA crossover instances for the same symbol isolate state") {
    EMACrossoverStrategy first{"AAPL", 2, 3};
    EMACrossoverStrategy second{"AAPL", 2, 3};
    require_no_signal(first, make_bar(0, 10.0));
    require_no_signal(first, make_bar(1, 11.0));
    require_signal(first, make_bar(2, 12.0), SignalDirection::Long);

    require_no_signal(second, make_bar(0, 12.0));
    require_no_signal(second, make_bar(1, 11.0));
    require_no_signal(second, make_bar(2, 10.0));
    require_signal(second, make_bar(3, 20.0), SignalDirection::Long);
    require_no_signal(first, make_bar(3, 13.0));
}

TEST_CASE("EMA crossover replays identical market events deterministically") {
    EMACrossoverStrategy first{
        "AAPL", 2, 3, PriceSource::Close, 0.03, 0.4, 0.2
    };
    EMACrossoverStrategy second{
        "AAPL", 2, 3, PriceSource::Close, 0.03, 0.4, 0.2
    };
    const std::vector<double> prices{10, 11, 12, 10, 9, 13, 13, 11, 8, 14, 15, 9, 7};
    const auto first_records = replay(first, prices);
    const auto second_records = replay(second, prices);
    require_same_records(first_records, second_records);
    REQUIRE_FALSE(first_records.empty());
}

TEST_CASE("EMA crossover agrees with an independent long-double signal oracle") {
    const std::size_t fast_period = 3;
    const std::size_t slow_period = 7;
    const double fast_alpha = 0.45;
    const double slow_alpha = 0.18;
    const double threshold = 0.01;
    EMACrossoverStrategy strategy{
        "AAPL", fast_period, slow_period, PriceSource::Close,
        threshold, fast_alpha, slow_alpha
    };
    std::vector<double> prices;
    prices.reserve(240);
    for (std::size_t i = 0; i < 240; ++i) {
        // Positive data with trends and reversals, away from exact boundaries.
        const double cycle = static_cast<double>(i % 20);
        prices.push_back(100.0 + (cycle < 10.0 ? cycle * 2.0 : (20.0 - cycle) * 2.0)
            + static_cast<double>(i % 3) * 0.05);
    }
    const auto expected = reference_replay(
        prices, fast_period, slow_period,
        fast_alpha, slow_alpha, threshold
    );
    const auto actual = replay(strategy, prices);
    require_same_records(actual, expected);
    REQUIRE(actual.size() >= 8);
}

TEST_CASE("EMA crossover remains stable over hundreds of rolling updates") {
    EMACrossoverStrategy first{"AAPL", 2, 5};
    EMACrossoverStrategy second{"AAPL", 2, 5};
    const std::array<double, 10> pattern{10, 11, 14, 18, 15, 12, 9, 7, 8, 10};
    std::vector<double> prices;
    prices.reserve(500);
    for (std::size_t i = 0; i < 500; ++i) {
        prices.push_back(pattern[i % pattern.size()]);
    }
    const auto first_records = replay(first, prices);
    const auto second_records = replay(second, prices);
    require_same_records(first_records, second_records);
    REQUIRE(first_records.size() > 5);
    for (std::size_t i = 1; i < first_records.size(); ++i) {
        REQUIRE(first_records[i].observation_index > first_records[i - 1].observation_index);
        REQUIRE(first_records[i].direction != first_records[i - 1].direction);
    }
}

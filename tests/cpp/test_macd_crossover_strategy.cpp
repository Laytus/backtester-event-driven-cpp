
#include "backtester/strategy/macd_crossover_strategy.hpp"

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
using backtester::strategy::MACDCrossoverStrategy;
using backtester::strategy::PriceSource;

SessionDate session_date(std::size_t offset) {
    const std::chrono::sys_days start{
        std::chrono::year{2026} / std::chrono::October / 1
    };
    return SessionDate{start + std::chrono::days{static_cast<int>(offset)}};
}

Bar make_bar(
    std::size_t i,
    double price,
    const std::string& symbol = "AAPL"
) {
    return Bar{
        session_date(i), symbol, price, price, price, price,
        static_cast<std::uint64_t>(1'000 + i)
    };
}

Bar make_ohlc(
    std::size_t i,
    double open,
    double high,
    double low,
    double close,
    const std::string& symbol = "AAPL"
) {
    return Bar{
        session_date(i), symbol, open, high, low, close,
        static_cast<std::uint64_t>(1'000 + i)
    };
}

// The selected field follows price; all OHLC values remain valid.
Bar make_source_bar(
    std::size_t i,
    double price,
    PriceSource source
) {
    switch (source) {
        case PriceSource::Open:
            return make_ohlc(
                i, price, std::max(price, 5.0) + 1, 3, 5
            );
        case PriceSource::High:
            return make_ohlc(i, 4, price, 3, 4);
        case PriceSource::Low:
            return make_ohlc(i, 20, 21, price, 20);
        case PriceSource::Close:
            return make_ohlc(
                i, 5, std::max(price, 5.0) + 1, 3, price
            );
    }
    throw std::invalid_argument("Invalid test price source");
}

std::vector<SignalEvent> deliver(
    MACDCrossoverStrategy& strategy,
    const Bar& bar
) {
    return strategy.on_market(MarketEvent{bar});
}

void require_none(
    MACDCrossoverStrategy& strategy,
    const Bar& bar
) {
    REQUIRE(deliver(strategy, bar).empty());
}

void require_signal(
    MACDCrossoverStrategy& strategy,
    const Bar& bar,
    SignalDirection direction
) {
    const auto signals = deliver(strategy, bar);
    REQUIRE(signals.size() == 1);
    REQUIRE(signals[0].symbol() == bar.symbol());
    REQUIRE(signals[0].signal_date() == bar.session_date());
    REQUIRE(signals[0].direction() == direction);
    REQUIRE(signals[0].direction() != SignalDirection::Short);
}

struct SignalRecord final {
    std::size_t index;
    SignalDirection direction;
    SessionDate date;

    bool operator==(const SignalRecord&) const = default;
};

std::vector<SignalRecord> replay(
    MACDCrossoverStrategy& strategy,
    const std::vector<Bar>& bars
) {
    std::vector<SignalRecord> result;

    for (std::size_t i = 0; i < bars.size(); ++i) {
        const auto signals = deliver(strategy, bars[i]);
        REQUIRE(signals.size() <= 1);

        for (const auto& signal : signals) {
            REQUIRE(signal.symbol() == bars[i].symbol());
            REQUIRE(signal.signal_date() == bars[i].session_date());
            REQUIRE(signal.direction() != SignalDirection::Short);

            result.push_back({
                i, signal.direction(), signal.signal_date()
            });
        }
    }

    return result;
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

std::vector<Bar> make_bars(std::size_t count) {
    std::vector<Bar> bars;
    bars.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        const double close =
            100.0 + static_cast<double>((i * 17) % 29)
            - 14.0 + 0.025 * static_cast<double>(i);

        const double open =
            close + (i % 2 == 0 ? 1.25 : -1.75);

        bars.push_back(make_ohlc(
            i,
            open,
            std::max(open, close) + 2.0,
            std::min(open, close) - 3.0,
            close
        ));
    }

    return bars;
}

std::vector<Bar> oracle_bars() {
    const std::array<double, 10> prices{
        10, 12, 11, 15, 13, 16, 16, 11, 11, 17
    };

    std::vector<Bar> bars;

    for (std::size_t i = 0; i < prices.size(); ++i) {
        bars.push_back(make_bar(i, prices[i]));
    }

    return bars;
}

double selected_price(
    const Bar& bar,
    PriceSource source
) {
    switch (source) {
        case PriceSource::Open: return bar.open();
        case PriceSource::High: return bar.high();
        case PriceSource::Low: return bar.low();
        case PriceSource::Close: return bar.close();
    }

    throw std::invalid_argument("Invalid test price source");
}

// Independent reference implementation with long-double arithmetic.
// Seeds each EMA with the arithmetic mean of its first N inputs.
struct ReferenceEMA final {
    std::size_t period;
    long double alpha;
    std::size_t count{0};
    long double sum{0};
    long double current{0};

    void update(long double price) {
        if (count < period) {
            sum += price;
            ++count;

            if (count == period) {
                current = sum / static_cast<long double>(period);
            }
        } else {
            current += alpha * (price - current);
        }
    }

    [[nodiscard]]
    bool ready() const {
        return count == period;
    }
};

std::vector<SignalRecord> reference_replay(
    const std::vector<Bar>& bars,
    std::size_t fast_period,
    std::size_t slow_period,
    std::size_t signal_period,
    double threshold,
    PriceSource source,
    std::optional<double> fast_alpha = std::nullopt,
    std::optional<double> slow_alpha = std::nullopt,
    std::optional<double> signal_alpha = std::nullopt
) {
    const auto alpha = [](
        std::size_t n,
        std::optional<double> override_value
    ) {
        return static_cast<long double>(
            override_value.value_or(
                2.0 / (static_cast<double>(n) + 1.0)
            )
        );
    };

    ReferenceEMA fast{
        fast_period, alpha(fast_period, fast_alpha)
    };
    ReferenceEMA slow{
        slow_period, alpha(slow_period, slow_alpha)
    };
    ReferenceEMA signal{
        signal_period, alpha(signal_period, signal_alpha)
    };

    std::vector<SignalRecord> records;
    auto target = SignalDirection::Flat;

    for (std::size_t i = 0; i < bars.size(); ++i) {
        const auto observation = static_cast<long double>(
            selected_price(bars[i], source)
        );

        fast.update(observation);
        slow.update(observation);

        if (!fast.ready() || !slow.ready()) {
            continue;
        }

        const long double macd_line =
            fast.current - slow.current;

        signal.update(macd_line);

        if (!signal.ready()) {
            continue;
        }

        const long double histogram =
            macd_line - signal.current;

        auto desired = target;

        if (histogram > static_cast<long double>(threshold)) {
            desired = SignalDirection::Long;
        } else if (
            histogram < -static_cast<long double>(threshold)
        ) {
            desired = SignalDirection::Flat;
        }

        if (desired != target) {
            records.push_back({
                i, desired, bars[i].session_date()
            });
            target = desired;
        }
    }

    return records;
}

}  // namespace

TEST_CASE("MACD crossover rejects an empty symbol") {
    REQUIRE_THROWS_AS(
        (MACDCrossoverStrategy{""}),
        std::invalid_argument
    );
}

TEST_CASE("MACD crossover rejects zero and unordered periods") {
    REQUIRE_THROWS_AS(
        (MACDCrossoverStrategy{"AAPL", 0, 3, 2}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        (MACDCrossoverStrategy{"AAPL", 1, 0, 2}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        (MACDCrossoverStrategy{"AAPL", 1, 2, 0}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        (MACDCrossoverStrategy{"AAPL", 3, 3, 2}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        (MACDCrossoverStrategy{"AAPL", 4, 3, 2}),
        std::invalid_argument
    );
}

TEST_CASE("MACD crossover rejects negative and nonfinite thresholds") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    for (const double invalid : {-0.01, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            (MACDCrossoverStrategy{
                "AAPL", 2, 3, 2, PriceSource::Close, invalid
            }),
            std::invalid_argument
        );
    }
}

TEST_CASE("MACD crossover accepts zero negative zero and large finite thresholds") {
    MACDCrossoverStrategy zero{
        "AAPL", 2, 3, 2, PriceSource::Close, 0.0
    };
    MACDCrossoverStrategy negative_zero{
        "AAPL", 2, 3, 2, PriceSource::Close, -0.0
    };
    MACDCrossoverStrategy large{
        "AAPL", 2, 3, 2, PriceSource::Close, 1e200
    };

    const auto bars = oracle_bars();

    require_same(
        replay(zero, bars),
        replay(negative_zero, bars)
    );
    REQUIRE(replay(large, bars).empty());
}

TEST_CASE("MACD crossover rejects invalid price sources") {
    for (const auto source : {
        static_cast<PriceSource>(-1),
        static_cast<PriceSource>(99)
    }) {
        REQUIRE_THROWS_AS(
            (MACDCrossoverStrategy{"AAPL", 2, 3, 2, source}),
            std::invalid_argument
        );
    }
}

TEST_CASE("MACD crossover rejects invalid fast alpha overrides") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    for (const double invalid : {
        0.0, -0.1, 1.01, nan, inf, -inf
    }) {
        REQUIRE_THROWS_AS(
            (MACDCrossoverStrategy{
                "AAPL", 2, 3, 2, PriceSource::Close,
                0.0, invalid
            }),
            std::invalid_argument
        );
    }
}

TEST_CASE("MACD crossover rejects invalid slow alpha overrides") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    for (const double invalid : {
        0.0, -0.1, 1.01, nan, inf, -inf
    }) {
        REQUIRE_THROWS_AS(
            (MACDCrossoverStrategy{
                "AAPL", 2, 3, 2, PriceSource::Close,
                0.0, std::nullopt, invalid
            }),
            std::invalid_argument
        );
    }
}

TEST_CASE("MACD crossover rejects invalid signal alpha overrides") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    for (const double invalid : {
        0.0, -0.1, 1.01, nan, inf, -inf
    }) {
        REQUIRE_THROWS_AS(
            (MACDCrossoverStrategy{
                "AAPL", 2, 3, 2, PriceSource::Close,
                0.0, std::nullopt, std::nullopt, invalid
            }),
            std::invalid_argument
        );
    }
}

TEST_CASE("MACD crossover permits independent alpha overrides without ordering") {
    MACDCrossoverStrategy strategy{
        "AAPL", 2, 3, 2, PriceSource::Close,
        0.0, 0.1, 0.9, 1.0
    };

    REQUIRE(
        replay(strategy, oracle_bars()).size() <=
        oracle_bars().size()
    );
}

TEST_CASE("MACD crossover waits until observation thirty-four by default") {
    MACDCrossoverStrategy strategy{"AAPL"};

    for (std::size_t i = 0; i < 33; ++i) {
        require_none(strategy, make_bar(i, 100.0));
    }

    require_signal(
        strategy,
        make_bar(33, 140.0),
        SignalDirection::Long
    );
}

TEST_CASE("MACD crossover requires the signal line after the MACD line is ready") {
    MACDCrossoverStrategy strategy{"AAPL", 2, 3, 2};

    require_none(strategy, make_bar(0, 10));
    require_none(strategy, make_bar(1, 12));
    require_none(strategy, make_bar(2, 11));

    require_signal(
        strategy,
        make_bar(3, 15),
        SignalDirection::Long
    );
}

TEST_CASE("MACD crossover signal period one always has a zero histogram") {
    MACDCrossoverStrategy strategy{"AAPL", 2, 3, 1};

    for (std::size_t i = 0; i < 10; ++i) {
        require_none(
            strategy,
            make_bar(i, i % 2 == 0 ? 10.0 : 15.0)
        );
    }
}

TEST_CASE("MACD crossover supports fast period one and first eligible signal") {
    MACDCrossoverStrategy strategy{
        "AAPL", 1, 2, 2, PriceSource::Close,
        0.0, std::nullopt, 0.5
    };

    require_none(strategy, make_bar(0, 8));
    require_none(strategy, make_bar(1, 8));

    require_signal(
        strategy,
        make_bar(2, 12),
        SignalDirection::Long
    );
}

TEST_CASE("MACD crossover matches the approved zero-threshold oracle") {
    MACDCrossoverStrategy strategy{"AAPL", 2, 3, 2};
    const auto bars = oracle_bars();

    for (std::size_t i = 0; i < bars.size(); ++i) {
        if (i == 3 || i == 5 || i == 9) {
            require_signal(
                strategy, bars[i], SignalDirection::Long
            );
        } else if (i == 4 || i == 6) {
            require_signal(
                strategy, bars[i], SignalDirection::Flat
            );
        } else {
            require_none(strategy, bars[i]);
        }
    }
}

TEST_CASE("MACD crossover matches the approved threshold one-tenth oracle") {
    MACDCrossoverStrategy strategy{
        "AAPL", 2, 3, 2, PriceSource::Close, 0.1
    };
    const auto bars = oracle_bars();

    for (std::size_t i = 0; i < bars.size(); ++i) {
        if (i == 3 || i == 9) {
            require_signal(
                strategy, bars[i], SignalDirection::Long
            );
        } else if (i == 7) {
            require_signal(
                strategy, bars[i], SignalDirection::Flat
            );
        } else {
            require_none(strategy, bars[i]);
        }
    }
}

TEST_CASE("MACD crossover initially bearish histogram preserves Flat") {
    MACDCrossoverStrategy strategy{"AAPL", 2, 3, 2};

    for (std::size_t i = 0; i < 4; ++i) {
        require_none(
            strategy,
            make_bar(i, 16.0 - 2.0 * static_cast<double>(i))
        );
    }

    require_signal(
        strategy,
        make_bar(4, 25.0),
        SignalDirection::Long
    );
}

TEST_CASE("MACD crossover first eligible zero histogram preserves Flat") {
    MACDCrossoverStrategy strategy{"AAPL", 2, 3, 2};

    for (std::size_t i = 0; i < 4; ++i) {
        require_none(strategy, make_bar(i, 100.0));
    }

    require_signal(
        strategy,
        make_bar(4, 120.0),
        SignalDirection::Long
    );
}

TEST_CASE("MACD crossover exact positive threshold equality is neutral") {
    MACDCrossoverStrategy strategy{
        "AAPL", 1, 2, 2, PriceSource::Close,
        1.0, std::nullopt, 0.5
    };

    require_none(strategy, make_bar(0, 8));
    require_none(strategy, make_bar(1, 8));

    // Histogram exactly +1.
    require_none(strategy, make_bar(2, 12));

    require_signal(
        strategy,
        make_bar(3, 32),
        SignalDirection::Long
    );
}

TEST_CASE("MACD crossover exact negative threshold equality preserves Long") {
    MACDCrossoverStrategy strategy{
        "AAPL", 1, 2, 2, PriceSource::Close,
        1.0, std::nullopt, 0.5
    };

    require_none(strategy, make_bar(0, 8));
    require_none(strategy, make_bar(1, 8));

    require_signal(
        strategy,
        make_bar(2, 16),
        SignalDirection::Long
    );

    // Histogram exactly -1.
    require_none(strategy, make_bar(3, 10));

    require_signal(
        strategy,
        make_bar(4, 2),
        SignalDirection::Flat
    );
}

TEST_CASE("MACD crossover neutral histogram retains an existing Long target") {
    MACDCrossoverStrategy strategy{
        "AAPL", 2, 3, 2, PriceSource::Close, 0.1
    };
    const auto bars = oracle_bars();

    require_none(strategy, bars[0]);
    require_none(strategy, bars[1]);
    require_none(strategy, bars[2]);

    require_signal(
        strategy, bars[3], SignalDirection::Long
    );

    require_none(strategy, bars[4]);
    require_none(strategy, bars[5]);
    require_none(strategy, bars[6]);

    require_signal(
        strategy, bars[7], SignalDirection::Flat
    );
}

TEST_CASE("MACD crossover does not generate duplicate target signals") {
    MACDCrossoverStrategy strategy{
        "AAPL", 2, 3, 2, PriceSource::Close, 0.1
    };
    const auto bars = oracle_bars();
    const auto records = replay(strategy, bars);

    REQUIRE(records.size() == 3);
    REQUIRE(records[0].direction == SignalDirection::Long);
    REQUIRE(records[1].direction == SignalDirection::Flat);
    REQUIRE(records[2].direction == SignalDirection::Long);
}

TEST_CASE("MACD crossover uses histogram sign rather than MACD line sign") {
    MACDCrossoverStrategy strategy{"AAPL", 2, 3, 2};
    const auto bars = oracle_bars();

    for (std::size_t i = 0; i < 4; ++i) {
        if (i == 3) {
            require_signal(
                strategy, bars[i], SignalDirection::Long
            );
        } else {
            require_none(strategy, bars[i]);
        }
    }

    // MACD line remains positive, but histogram turns negative.
    require_signal(
        strategy, bars[4], SignalDirection::Flat
    );
}

TEST_CASE("MACD crossover threshold is absolute rather than relative") {
    MACDCrossoverStrategy regular{
        "AAPL", 2, 3, 2, PriceSource::Close, 0.1
    };
    MACDCrossoverStrategy scaled{
        "AAPL", 2, 3, 2, PriceSource::Close, 0.1
    };
    const auto bars = oracle_bars();

    for (std::size_t i = 0; i < 5; ++i) {
        const auto ordinary = deliver(regular, bars[i]);
        const auto magnified = deliver(
            scaled,
            make_bar(i, bars[i].close() * 100)
        );

        if (i == 4) {
            REQUIRE(ordinary.empty());
            REQUIRE(magnified.size() == 1);
            REQUIRE(
                magnified[0].direction() ==
                SignalDirection::Flat
            );
        } else {
            REQUIRE(ordinary.size() == magnified.size());
        }
    }
}

TEST_CASE("MACD crossover custom fast alpha changes the initial ready signal") {
    MACDCrossoverStrategy baseline{"AAPL", 2, 3, 2};
    MACDCrossoverStrategy custom{
        "AAPL", 2, 3, 2, PriceSource::Close, 0.0, 0.1
    };
    const auto bars = oracle_bars();

    for (std::size_t i = 0; i < 3; ++i) {
        require_none(baseline, bars[i]);
        require_none(custom, bars[i]);
    }

    require_signal(
        baseline, bars[3], SignalDirection::Long
    );
    require_none(custom, bars[3]);
}

TEST_CASE("MACD crossover custom slow alpha changes the initial ready signal") {
    MACDCrossoverStrategy baseline{"AAPL", 2, 3, 2};
    MACDCrossoverStrategy custom{
        "AAPL", 2, 3, 2, PriceSource::Close,
        0.0, std::nullopt, 1.0
    };
    const auto bars = oracle_bars();

    for (std::size_t i = 0; i < 3; ++i) {
        require_none(baseline, bars[i]);
        require_none(custom, bars[i]);
    }

    require_signal(
        baseline, bars[3], SignalDirection::Long
    );
    require_none(custom, bars[3]);
}

TEST_CASE("MACD crossover signal alpha override affects only post-seed recurrence") {
    MACDCrossoverStrategy baseline{"AAPL", 2, 3, 2};
    MACDCrossoverStrategy custom{
        "AAPL", 2, 3, 2, PriceSource::Close, 0.0,
        std::nullopt, std::nullopt, 1.0
    };
    const auto bars = oracle_bars();

    for (std::size_t i = 0; i < 3; ++i) {
        require_none(baseline, bars[i]);
        require_none(custom, bars[i]);
    }

    require_signal(
        baseline, bars[3], SignalDirection::Long
    );
    require_signal(
        custom, bars[3], SignalDirection::Long
    );

    require_signal(
        baseline, bars[4], SignalDirection::Flat
    );
    require_none(custom, bars[4]);
}

TEST_CASE("MACD crossover all custom alpha overrides follow their SMA seeds") {
    MACDCrossoverStrategy strategy{
        "AAPL", 2, 3, 2, PriceSource::Close,
        0.4, 0.5, 0.25, 0.5
    };
    const auto bars = oracle_bars();

    for (std::size_t i = 0; i < 3; ++i) {
        require_none(strategy, bars[i]);
    }

    require_signal(
        strategy, bars[3], SignalDirection::Long
    );
}

TEST_CASE("MACD crossover supports Open High Low and Close input streams") {
    const std::array<double, 5> prices{
        10, 12, 11, 15, 13
    };

    for (const auto source : {
        PriceSource::Open,
        PriceSource::High,
        PriceSource::Low,
        PriceSource::Close
    }) {
        MACDCrossoverStrategy strategy{
            "AAPL", 2, 3, 2, source
        };

        for (std::size_t i = 0; i < prices.size(); ++i) {
            const auto bar = make_source_bar(
                i, prices[i], source
            );

            if (i == 3) {
                require_signal(
                    strategy, bar, SignalDirection::Long
                );
            } else if (i == 4) {
                require_signal(
                    strategy, bar, SignalDirection::Flat
                );
            } else {
                require_none(strategy, bar);
            }
        }
    }
}

TEST_CASE("MACD crossover defaults to Close rather than Open") {
    MACDCrossoverStrategy close_strategy{
        "AAPL", 2, 3, 2
    };
    MACDCrossoverStrategy open_strategy{
        "AAPL", 2, 3, 2, PriceSource::Open
    };
    const std::array<double, 4> prices{
        10, 12, 11, 15
    };

    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_ohlc(
            i, 20, 21, 3, prices[i]
        );

        if (i == 3) {
            require_signal(
                close_strategy, bar, SignalDirection::Long
            );
        } else {
            require_none(close_strategy, bar);
        }

        require_none(open_strategy, bar);
    }
}

TEST_CASE("MACD crossover ignores modifications to unselected OHLC fields") {
    MACDCrossoverStrategy first{
        "AAPL", 2, 3, 2
    };
    MACDCrossoverStrategy second{
        "AAPL", 2, 3, 2
    };
    const auto bars = make_bars(120);

    for (std::size_t i = 0; i < bars.size(); ++i) {
        const auto& a = bars[i];

        const auto b = make_ohlc(
            i,
            a.close() + (i % 2 == 0 ? 4.0 : -4.0),
            a.high() + 6.0,
            a.low() - 5.0,
            a.close()
        );

        const auto first_signals = deliver(first, a);
        const auto second_signals = deliver(second, b);

        REQUIRE(
            first_signals.size() == second_signals.size()
        );

        for (
            std::size_t j = 0;
            j < first_signals.size();
            ++j
        ) {
            REQUIRE(
                first_signals[j].direction() ==
                second_signals[j].direction()
            );
        }
    }
}

TEST_CASE("MACD crossover ignores foreign symbols during warm-up") {
    MACDCrossoverStrategy strategy{
        "AAPL", 2, 3, 2
    };

    require_none(strategy, make_bar(0, 999, "MSFT"));
    require_none(strategy, make_bar(1, 10));
    require_none(strategy, make_bar(2, 999, "MSFT"));
    require_none(strategy, make_bar(3, 12));
    require_none(strategy, make_bar(4, 11));

    require_signal(
        strategy,
        make_bar(5, 15),
        SignalDirection::Long
    );
}

TEST_CASE("MACD crossover ignores foreign symbols after a Long target") {
    MACDCrossoverStrategy strategy{
        "AAPL", 2, 3, 2
    };
    const auto bars = oracle_bars();

    for (std::size_t i = 0; i < 4; ++i) {
        if (i == 3) {
            require_signal(
                strategy, bars[i], SignalDirection::Long
            );
        } else {
            require_none(strategy, bars[i]);
        }
    }

    require_none(
        strategy,
        make_bar(20, 1, "MSFT")
    );

    require_signal(
        strategy, bars[4], SignalDirection::Flat
    );
}

TEST_CASE("MACD crossover readiness counts observed bars rather than calendar days") {
    MACDCrossoverStrategy strategy{
        "AAPL", 2, 3, 2
    };

    require_none(strategy, make_bar(0, 10));
    require_none(strategy, make_bar(12, 12));
    require_none(strategy, make_bar(35, 11));

    require_signal(
        strategy,
        make_bar(70, 15),
        SignalDirection::Long
    );
}

TEST_CASE("MACD crossover different symbols use independent strategy state") {
    MACDCrossoverStrategy first{
        "AAPL", 2, 3, 2
    };
    MACDCrossoverStrategy second{
        "MSFT", 2, 3, 2
    };
    const auto bars = oracle_bars();

    for (std::size_t i = 0; i < 5; ++i) {
        const auto a = deliver(first, bars[i]);
        const auto b = deliver(
            second,
            make_bar(i, bars[i].close(), "MSFT")
        );

        REQUIRE(a.size() == b.size());

        for (std::size_t j = 0; j < a.size(); ++j) {
            REQUIRE(
                a[j].direction() == b[j].direction()
            );
        }
    }
}

TEST_CASE("MACD crossover same-symbol strategies isolate differing thresholds") {
    MACDCrossoverStrategy zero{
        "AAPL", 2, 3, 2
    };
    MACDCrossoverStrategy filtered{
        "AAPL", 2, 3, 2, PriceSource::Close, 0.1
    };
    const auto bars = oracle_bars();

    const auto a = replay(zero, bars);
    const auto b = replay(filtered, bars);

    REQUIRE(a.size() == 5);
    REQUIRE(b.size() == 3);
}

TEST_CASE("MACD crossover replays identical events deterministically") {
    const auto bars = make_bars(400);

    MACDCrossoverStrategy first{
        "AAPL", 5, 10, 4, PriceSource::Close, 0.1
    };
    MACDCrossoverStrategy second{
        "AAPL", 5, 10, 4, PriceSource::Close, 0.1
    };

    require_same(
        replay(first, bars),
        replay(second, bars)
    );
}

TEST_CASE("MACD crossover agrees with an independent long-double oracle") {
    const auto bars = make_bars(500);

    for (const auto fast : {1U, 2U, 5U}) {
        for (const auto slow : {6U, 10U, 20U}) {
            for (const auto signal : {1U, 2U, 5U}) {
                for (const double threshold : {
                    0.0, 0.1, 0.5
                }) {
                    MACDCrossoverStrategy strategy{
                        "AAPL",
                        fast,
                        slow,
                        signal,
                        PriceSource::Close,
                        threshold
                    };

                    require_same(
                        replay(strategy, bars),
                        reference_replay(
                            bars,
                            fast,
                            slow,
                            signal,
                            threshold,
                            PriceSource::Close
                        )
                    );
                }
            }
        }
    }
}

TEST_CASE("MACD crossover custom alpha signals match an independent oracle") {
    const auto bars = make_bars(400);

    for (const auto source : {
        PriceSource::Open,
        PriceSource::Close
    }) {
        MACDCrossoverStrategy strategy{
            "AAPL", 3, 8, 4, source,
            0.2, 0.35, 0.15, 0.5
        };

        require_same(
            replay(strategy, bars),
            reference_replay(
                bars, 3, 8, 4, 0.2, source,
                0.35, 0.15, 0.5
            )
        );
    }
}

TEST_CASE("MACD crossover decisions do not depend on future observations") {
    const auto bars = make_bars(250);

    const std::vector<Bar> prefix(
        bars.begin(),
        bars.begin() + 90
    );

    MACDCrossoverStrategy short_run{
        "AAPL", 5, 12, 4
    };
    MACDCrossoverStrategy complete_run{
        "AAPL", 5, 12, 4
    };

    const auto before = replay(short_run, prefix);
    const auto after = replay(complete_run, bars);

    REQUIRE(after.size() >= before.size());

    for (std::size_t i = 0; i < before.size(); ++i) {
        REQUIRE(after[i] == before[i]);
    }
}

TEST_CASE("MACD crossover remains stable over thousands of updates") {
    const auto bars = make_bars(5'000);

    MACDCrossoverStrategy strategy{
        "AAPL", 12, 26, 9,
        PriceSource::Close, 0.2
    };

    require_same(
        replay(strategy, bars),
        reference_replay(
            bars, 12, 26, 9, 0.2,
            PriceSource::Close
        )
    );
}

TEST_CASE("MACD crossover handles large finite positive prices") {
    MACDCrossoverStrategy strategy{
        "AAPL", 2, 3, 2
    };
    const auto bars = oracle_bars();

    for (std::size_t i = 0; i < bars.size(); ++i) {
        const auto signals = deliver(
            strategy,
            make_bar(i, bars[i].close() * 1e200)
        );

        REQUIRE(signals.size() <= 1);

        for (const auto& signal : signals) {
            REQUIRE(
                signal.direction() != SignalDirection::Short
            );
        }
    }
}

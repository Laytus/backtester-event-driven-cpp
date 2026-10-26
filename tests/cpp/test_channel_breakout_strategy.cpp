
#include "backtester/strategy/channel_breakout_strategy.hpp"

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
using backtester::strategy::ChannelBreakoutStrategy;
using backtester::strategy::PriceSource;

SessionDate session_date(std::size_t offset) {
    const std::chrono::sys_days first{
        std::chrono::year{2026} / std::chrono::October / 1
    };
    return SessionDate{first + std::chrono::days{static_cast<int>(offset)}};
}

Bar make_bar(
    std::size_t index,
    double high,
    double low,
    double close,
    const std::string& symbol = "AAPL"
) {
    return Bar{
        session_date(index), symbol, close, high, low, close,
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

std::vector<SignalEvent> deliver(ChannelBreakoutStrategy& strategy, const Bar& bar) {
    return strategy.on_market(MarketEvent{bar});
}

void require_no_signal(ChannelBreakoutStrategy& strategy, const Bar& bar) {
    REQUIRE(deliver(strategy, bar).empty());
}

void require_signal(
    ChannelBreakoutStrategy& strategy,
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
    ChannelBreakoutStrategy& strategy,
    const std::vector<Bar>& bars
) {
    std::vector<SignalRecord> results;
    for (std::size_t i = 0; i < bars.size(); ++i) {
        for (const auto& signal : deliver(strategy, bars[i])) {
            REQUIRE(signal.symbol() == bars[i].symbol());
            REQUIRE(signal.signal_date() == bars[i].session_date());
            REQUIRE(signal.direction() != SignalDirection::Short);
            results.push_back({i, signal.direction(), signal.signal_date()});
        }
    }
    return results;
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

double reference_price(const Bar& bar, PriceSource source) {
    switch (source) {
        case PriceSource::Open: return bar.open();
        case PriceSource::High: return bar.high();
        case PriceSource::Low: return bar.low();
        case PriceSource::Close: return bar.close();
    }
    throw std::invalid_argument("Invalid reference price source");
}

// Independent O(T * (entry_period + exit_period)) oracle.
// Scans raw historical OHLC values; does not use RollingMinMax.
std::vector<SignalRecord> reference_replay(
    const std::vector<Bar>& bars,
    std::size_t entry_period,
    std::size_t exit_period,
    double buffer,
    PriceSource source
) {
    std::vector<SignalRecord> results;
    auto target = SignalDirection::Flat;
    const std::size_t readiness = std::max(entry_period, exit_period);

    for (std::size_t i = readiness; i < bars.size(); ++i) {
        double upper = bars[i - entry_period].high();
        for (std::size_t j = i - entry_period + 1; j < i; ++j) {
            upper = std::max(upper, bars[j].high());
        }

        double lower = bars[i - exit_period].low();
        for (std::size_t j = i - exit_period + 1; j < i; ++j) {
            lower = std::min(lower, bars[j].low());
        }

        const double upper_threshold = std::fma(buffer, upper, upper);
        const double lower_threshold = std::fma(-buffer, lower, lower);
        const double price = reference_price(bars[i], source);

        auto desired = target;
        if (price > upper_threshold) {
            desired = SignalDirection::Long;
        } else if (price < lower_threshold) {
            desired = SignalDirection::Flat;
        }

        if (desired != target) {
            results.push_back({i, desired, bars[i].session_date()});
            target = desired;
        }
    }
    return results;
}

std::vector<Bar> make_bars(std::size_t count, const std::string& symbol = "AAPL") {
    std::vector<Bar> bars;
    bars.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double close = 100.0
            + static_cast<double>((i * 17) % 29) - 14.0
            + 0.025 * static_cast<double>(i);
        const double open = close + (i % 2 == 0 ? 1.25 : -1.75);
        const double high = std::max(open, close)
            + 2.0 + static_cast<double>(i % 4);
        const double low = std::min(open, close)
            - 2.0 - static_cast<double>((i + 1) % 3);
        bars.push_back(make_ohlc(i, open, high, low, close, symbol));
    }
    return bars;
}

}  // namespace

TEST_CASE("Channel breakout rejects an empty symbol") {
    REQUIRE_THROWS_AS((ChannelBreakoutStrategy{""}), std::invalid_argument);
}

TEST_CASE("Channel breakout rejects zero entry and exit periods") {
    REQUIRE_THROWS_AS((ChannelBreakoutStrategy{"AAPL", 0, 2}), std::invalid_argument);
    REQUIRE_THROWS_AS((ChannelBreakoutStrategy{"AAPL", 2, 0}), std::invalid_argument);
    REQUIRE_THROWS_AS((ChannelBreakoutStrategy{"AAPL", 0, 0}), std::invalid_argument);
}

TEST_CASE("Channel breakout rejects negative and nonfinite buffers") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double buffer : {-0.01, nan, inf, -inf}) {
        REQUIRE_THROWS_AS(
            (ChannelBreakoutStrategy{"AAPL", 2, 2, buffer}),
            std::invalid_argument
        );
    }
}

TEST_CASE("Channel breakout rejects buffers at or above one") {
    for (const double buffer : {1.0, 1.001, 2.0}) {
        REQUIRE_THROWS_AS(
            (ChannelBreakoutStrategy{"AAPL", 2, 2, buffer}),
            std::invalid_argument
        );
    }
}

TEST_CASE("Channel breakout permits zero negative zero and near-one buffers") {
    ChannelBreakoutStrategy zero{"AAPL", 1, 1, 0.0};
    ChannelBreakoutStrategy negative_zero{"AAPL", 1, 1, -0.0};
    ChannelBreakoutStrategy almost_one{
        "AAPL", 1, 1, std::nextafter(1.0, 0.0)
    };
    const auto first = make_bar(0, 101.0, 99.0, 100.0);
    require_no_signal(zero, first);
    require_no_signal(negative_zero, first);
    require_no_signal(almost_one, first);
    const auto second = make_bar(1, 106.0, 103.0, 105.0);
    require_signal(zero, second, SignalDirection::Long);
    require_signal(negative_zero, second, SignalDirection::Long);
    require_no_signal(almost_one, second);
}

TEST_CASE("Channel breakout rejects invalid reference price sources") {
    for (const auto invalid : {static_cast<PriceSource>(-1), static_cast<PriceSource>(99)}) {
        REQUIRE_THROWS_AS(
            (ChannelBreakoutStrategy{"AAPL", 2, 2, 0.0, invalid}),
            std::invalid_argument
        );
    }
}

TEST_CASE("Channel breakout default configuration first evaluates on bar twenty-one") {
    ChannelBreakoutStrategy strategy{"AAPL"};
    for (std::size_t i = 0; i < 20; ++i) {
        require_no_signal(strategy, make_bar(i, 101.0, 99.0, 100.0));
    }
    require_signal(
        strategy, make_bar(20, 112.0, 109.0, 111.0),
        SignalDirection::Long
    );
}

TEST_CASE("Channel breakout warm-up advances both channels before they are ready") {
    ChannelBreakoutStrategy strategy{"AAPL", 2, 2, 0.0};
    require_no_signal(strategy, make_bar(0, 101.0, 98.0, 100.0));
    require_no_signal(strategy, make_bar(1, 104.0, 99.0, 103.0));
    require_signal(strategy, make_bar(2, 108.0, 101.0, 107.0), SignalDirection::Long);
}

TEST_CASE("Channel breakout waits for longer entry period") {
    ChannelBreakoutStrategy strategy{"AAPL", 3, 1, 0.0};
    require_no_signal(strategy, make_bar(0, 101.0, 99.0, 100.0));
    require_no_signal(strategy, make_bar(1, 103.0, 99.0, 102.0));
    require_no_signal(strategy, make_bar(2, 107.0, 103.0, 106.0));
    require_signal(strategy, make_bar(3, 112.0, 108.0, 111.0), SignalDirection::Long);
}

TEST_CASE("Channel breakout waits for longer exit period") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 3, 0.0};
    require_no_signal(strategy, make_bar(0, 101.0, 99.0, 100.0));
    require_no_signal(strategy, make_bar(1, 103.0, 99.0, 102.0));
    require_no_signal(strategy, make_bar(2, 107.0, 103.0, 106.0));
    require_signal(strategy, make_bar(3, 112.0, 108.0, 111.0), SignalDirection::Long);
}

TEST_CASE("Channel breakout matches the approved nine-bar OHLC oracle") {
    ChannelBreakoutStrategy strategy{"AAPL", 2, 2, 0.0};
    const std::array<std::array<double, 3>, 9> values{{
        {101, 98, 100}, {104, 99, 103}, {108, 101, 107},
        {109, 104, 107}, {107, 97, 98}, {103, 96, 101},
        {112, 100, 111}, {114, 106, 112}, {113, 93, 95}
    }};
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto bar = make_bar(i, values[i][0], values[i][1], values[i][2]);
        if (i == 2 || i == 6) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4 || i == 8) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Channel breakout first bearish observation preserves initial Flat") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.0};
    require_no_signal(strategy, make_bar(0, 110.0, 90.0, 100.0));
    require_no_signal(strategy, make_bar(1, 85.0, 80.0, 82.0));
    require_signal(strategy, make_bar(2, 95.0, 88.0, 92.0), SignalDirection::Long);
}

TEST_CASE("Channel breakout first neutral evaluation preserves initial Flat") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.0};
    require_no_signal(strategy, make_bar(0, 110.0, 90.0, 100.0));
    require_no_signal(strategy, make_bar(1, 109.0, 91.0, 100.0));
    require_signal(strategy, make_bar(2, 112.0, 109.0, 111.0), SignalDirection::Long);
}

TEST_CASE("Channel breakout neutral prices preserve an existing Long target") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.0};
    require_no_signal(strategy, make_bar(0, 101.0, 98.0, 100.0));
    require_signal(strategy, make_bar(1, 111.0, 105.0, 110.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(2, 112.0, 106.0, 109.0));
    require_no_signal(strategy, make_bar(3, 110.0, 105.0, 107.0));
    require_signal(strategy, make_bar(4, 100.0, 96.0, 98.0), SignalDirection::Flat);
}

TEST_CASE("Channel breakout emits no repeated Long or Flat signals") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.0};
    const std::array<double, 7> prices{100, 110, 120, 130, 90, 80, 70};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const auto bar = make_bar(i, prices[i] + 1, prices[i] - 1, prices[i]);
        if (i == 1) {
            require_signal(strategy, bar, SignalDirection::Long);
        } else if (i == 4) {
            require_signal(strategy, bar, SignalDirection::Flat);
        } else {
            require_no_signal(strategy, bar);
        }
    }
}

TEST_CASE("Channel breakout equality with prior upper boundary is neutral") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.0};
    require_no_signal(strategy, make_bar(0, 105.0, 90.0, 100.0));
    require_no_signal(strategy, make_bar(1, 108.0, 98.0, 105.0));
    require_signal(strategy, make_bar(2, 112.0, 108.0, 110.0), SignalDirection::Long);
}

TEST_CASE("Channel breakout equality with prior lower boundary preserves Long") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.0};
    require_no_signal(strategy, make_bar(0, 101.0, 90.0, 100.0));
    require_signal(strategy, make_bar(1, 120.0, 110.0, 115.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(2, 115.0, 109.0, 110.0));
    require_signal(strategy, make_bar(3, 100.0, 79.0, 80.0), SignalDirection::Flat);
}

TEST_CASE("Channel breakout zero buffer requires strict channel break") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.0};
    require_no_signal(strategy, make_bar(0, 100.0, 90.0, 95.0));
    require_no_signal(strategy, make_bar(1, 103.0, 92.0, 100.0));
    require_signal(strategy, make_bar(2, 105.0, 101.0, 104.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(3, 107.0, 102.0, 103.0));
    require_signal(strategy, make_bar(4, 101.0, 90.0, 91.0), SignalDirection::Flat);
}

TEST_CASE("Channel breakout buffer filters small breakouts in both directions") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.05};
    require_no_signal(strategy, make_bar(0, 100.0, 90.0, 95.0));
    require_no_signal(strategy, make_bar(1, 101.0, 94.0, 100.0));
    require_signal(strategy, make_bar(2, 108.0, 95.0, 107.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(3, 100.0, 90.0, 91.0));
    require_signal(strategy, make_bar(4, 89.0, 79.0, 80.0), SignalDirection::Flat);
}

TEST_CASE("Channel breakout uses previous High rather than previous Close for entry") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.0};
    require_no_signal(strategy, make_bar(0, 125.0, 95.0, 100.0));
    require_no_signal(strategy, make_bar(1, 120.0, 101.0, 110.0));
    require_signal(strategy, make_bar(2, 130.0, 121.0, 126.0), SignalDirection::Long);
}

TEST_CASE("Channel breakout uses previous Low rather than previous High minimum for exit") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.0};
    require_no_signal(strategy, make_bar(0, 110.0, 90.0, 100.0));
    require_signal(strategy, make_bar(1, 125.0, 100.0, 115.0), SignalDirection::Long);
    // 95 is below the prior Low (100), but not below the prior High (125).
    require_signal(strategy, make_bar(2, 104.0, 93.0, 95.0), SignalDirection::Flat);
}

TEST_CASE("Channel breakout uses historical Low even when prior High minimum differs") {
    ChannelBreakoutStrategy strategy{"AAPL", 2, 2, 0.0};
    require_no_signal(strategy, make_bar(0, 150.0, 90.0, 100.0));
    require_no_signal(strategy, make_bar(1, 120.0, 80.0, 110.0));
    require_signal(strategy, make_bar(2, 160.0, 125.0, 155.0), SignalDirection::Long);
    // Prior Low minimum is 80, not minimum(High) = 120.
    require_no_signal(strategy, make_bar(3, 130.0, 83.0, 100.0));
    require_signal(strategy, make_bar(4, 105.0, 74.0, 75.0), SignalDirection::Flat);
}

TEST_CASE("Channel breakout expires older High observations at the correct boundary") {
    ChannelBreakoutStrategy strategy{"AAPL", 2, 2, 0.0};
    require_no_signal(strategy, make_bar(0, 150.0, 98.0, 100.0));
    require_no_signal(strategy, make_bar(1, 120.0, 98.0, 100.0));
    require_no_signal(strategy, make_bar(2, 110.0, 98.0, 100.0));
    require_signal(strategy, make_bar(3, 132.0, 125.0, 130.0), SignalDirection::Long);
}

TEST_CASE("Channel breakout expires older Low observations at the correct boundary") {
    ChannelBreakoutStrategy strategy{"AAPL", 2, 2, 0.0};
    require_no_signal(strategy, make_bar(0, 101.0, 80.0, 100.0));
    require_no_signal(strategy, make_bar(1, 102.0, 90.0, 100.0));
    require_signal(strategy, make_bar(2, 110.0, 95.0, 109.0), SignalDirection::Long);
    require_no_signal(strategy, make_bar(3, 112.0, 92.0, 103.0));
    require_signal(strategy, make_bar(4, 103.0, 90.0, 91.0), SignalDirection::Flat);
}

TEST_CASE("Channel breakout supports distinct entry and exit lookbacks") {
    ChannelBreakoutStrategy short_entry{"AAPL", 1, 3, 0.0};
    ChannelBreakoutStrategy long_entry{"AAPL", 3, 1, 0.0};
    const std::array<Bar, 5> bars{{
        make_bar(0, 150, 90, 100),
        make_bar(1, 120, 91, 100),
        make_bar(2, 110, 92, 100),
        make_bar(3, 125, 110, 124),
        make_bar(4, 160, 151, 159)
    }};
    for (std::size_t i = 0; i < bars.size(); ++i) {
        if (i == 3) {
            require_signal(short_entry, bars[i], SignalDirection::Long);
            require_no_signal(long_entry, bars[i]);
        } else if (i == 4) {
            require_no_signal(short_entry, bars[i]);
            require_signal(long_entry, bars[i], SignalDirection::Long);
        } else {
            require_no_signal(short_entry, bars[i]);
            require_no_signal(long_entry, bars[i]);
        }
    }
}

TEST_CASE("Channel breakout supports Open High Low and Close reference prices") {
    const auto prior = make_ohlc(0, 95, 100, 90, 95);
    const auto rise = make_ohlc(1, 115, 121, 111, 116);
    const auto fall = make_ohlc(2, 35, 40, 30, 35);
    for (const PriceSource source : {
        PriceSource::Open, PriceSource::High,
        PriceSource::Low, PriceSource::Close
    }) {
        ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.0, source};
        require_no_signal(strategy, prior);
        require_signal(strategy, rise, SignalDirection::Long);
        require_signal(strategy, fall, SignalDirection::Flat);
    }
}

TEST_CASE("Channel breakout defaults to Close rather than Open reference") {
    ChannelBreakoutStrategy close_strategy{"AAPL", 1, 1, 0.0};
    ChannelBreakoutStrategy open_strategy{"AAPL", 1, 1, 0.0, PriceSource::Open};
    const auto prior = make_ohlc(0, 95, 105, 90, 100);
    const auto current = make_ohlc(1, 95, 112, 93, 110);
    require_no_signal(close_strategy, prior);
    require_no_signal(open_strategy, prior);
    require_signal(close_strategy, current, SignalDirection::Long);
    require_no_signal(open_strategy, current);
}

TEST_CASE("Channel breakout Open changes do not affect Close based channels") {
    ChannelBreakoutStrategy a{"AAPL", 2, 2, 0.0};
    ChannelBreakoutStrategy b{"AAPL", 2, 2, 0.0};
    for (std::size_t i = 0; i < 80; ++i) {
        const double close = 100.0 + static_cast<double>((i * 7) % 17);
        const double high = close + 10.0;
        const double low = close - 10.0;
        const auto first = make_ohlc(i, close, high, low, close);
        const auto second = make_ohlc(i, close + (i % 2 == 0 ? 4.0 : -4.0), high, low, close);
        const auto first_signals = deliver(a, first);
        const auto second_signals = deliver(b, second);
        REQUIRE(first_signals.size() == second_signals.size());
        for (std::size_t j = 0; j < first_signals.size(); ++j) {
            REQUIRE(first_signals[j].direction() == second_signals[j].direction());
        }
    }
}

TEST_CASE("Channel breakout ignores foreign symbols during warm-up") {
    ChannelBreakoutStrategy strategy{"AAPL", 2, 2, 0.0};
    require_no_signal(strategy, make_bar(0, 101, 90, 100, "MSFT"));
    require_no_signal(strategy, make_bar(1, 101, 90, 100, "MSFT"));
    require_no_signal(strategy, make_bar(2, 101, 90, 100, "AAPL"));
    require_no_signal(strategy, make_bar(3, 102, 90, 101, "MSFT"));
    require_no_signal(strategy, make_bar(4, 101, 90, 100, "AAPL"));
    require_signal(strategy, make_bar(5, 112, 110, 111, "AAPL"), SignalDirection::Long);
}

TEST_CASE("Channel breakout foreign symbols cannot change a ready target") {
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.0};
    require_no_signal(strategy, make_bar(0, 101, 99, 100));
    require_signal(strategy, make_bar(1, 111, 109, 110), SignalDirection::Long);
    require_no_signal(strategy, make_bar(2, 51, 49, 50, "MSFT"));
    require_no_signal(strategy, make_bar(3, 112, 108, 109));
    require_signal(strategy, make_bar(4, 91, 89, 90), SignalDirection::Flat);
}

TEST_CASE("Channel breakout warm-up counts bars rather than consecutive dates") {
    ChannelBreakoutStrategy strategy{"AAPL", 2, 2, 0.0};
    require_no_signal(strategy, make_bar(0, 101, 99, 100));
    require_no_signal(strategy, make_bar(12, 104, 99, 103));
    require_signal(strategy, make_bar(36, 112, 109, 111), SignalDirection::Long);
}

TEST_CASE("Channel breakout different-symbol strategies have isolated states") {
    ChannelBreakoutStrategy a{"AAPL", 1, 1, 0.0};
    ChannelBreakoutStrategy b{"MSFT", 1, 1, 0.0};
    const auto a0 = make_bar(0, 101, 99, 100, "AAPL");
    const auto b0 = make_bar(0, 201, 199, 200, "MSFT");
    require_no_signal(a, a0);
    require_no_signal(b, b0);
    require_signal(a, make_bar(1, 112, 109, 111, "AAPL"), SignalDirection::Long);
    require_no_signal(b, make_bar(1, 202, 198, 200, "MSFT"));
    require_signal(b, make_bar(2, 215, 211, 214, "MSFT"), SignalDirection::Long);
    require_signal(a, make_bar(2, 91, 88, 90, "AAPL"), SignalDirection::Flat);
}

TEST_CASE("Channel breakout same-symbol strategy instances have isolated states") {
    ChannelBreakoutStrategy a{"AAPL", 1, 1, 0.0};
    ChannelBreakoutStrategy b{"AAPL", 2, 2, 0.0};
    require_no_signal(a, make_bar(0, 101, 99, 100));
    require_no_signal(b, make_bar(0, 101, 99, 100));
    require_signal(a, make_bar(1, 110, 108, 109), SignalDirection::Long);
    require_no_signal(b, make_bar(1, 110, 108, 109));
    require_no_signal(a, make_bar(2, 110, 108, 109));
    require_no_signal(b, make_bar(2, 110, 108, 109));
    require_signal(a, make_bar(3, 100, 95, 96), SignalDirection::Flat);
    require_signal(b, make_bar(3, 115, 112, 114), SignalDirection::Long);
}

TEST_CASE("Channel breakout replays identical events deterministically") {
    const auto bars = make_bars(300);
    ChannelBreakoutStrategy a{"AAPL", 5, 3, 0.0125};
    ChannelBreakoutStrategy b{"AAPL", 5, 3, 0.0125};
    require_same_records(replay(a, bars), replay(b, bars));
}

TEST_CASE("Channel breakout agrees with an independent historical only oracle") {
    const auto bars = make_bars(600);
    for (const auto entry : {1U, 2U, 5U, 20U}) {
        for (const auto exit : {1U, 3U, 10U, 30U}) {
            for (const double buffer : {0.0, 0.01, 0.07}) {
                for (const PriceSource source : {
                    PriceSource::Open, PriceSource::High,
                    PriceSource::Low, PriceSource::Close
                }) {
                    ChannelBreakoutStrategy strategy{
                        "AAPL", entry, exit, buffer, source
                    };
                    require_same_records(
                        replay(strategy, bars),
                        reference_replay(bars, entry, exit, buffer, source)
                    );
                }
            }
        }
    }
}

TEST_CASE("Channel breakout past decisions are independent of future bars") {
    const auto bars = make_bars(250);
    const auto end = bars.begin() + 80;
    const std::vector<Bar> prefix(bars.begin(), end);
    ChannelBreakoutStrategy short_run{"AAPL", 10, 5, 0.01};
    ChannelBreakoutStrategy full_run{"AAPL", 10, 5, 0.01};
    const auto before = replay(short_run, prefix);
    const auto after = replay(full_run, bars);
    REQUIRE(after.size() >= before.size());
    for (std::size_t i = 0; i < before.size(); ++i) {
        REQUIRE(after[i] == before[i]);
    }
}

TEST_CASE("Channel breakout remains aligned over thousands of rolling updates") {
    const auto bars = make_bars(5'000);
    ChannelBreakoutStrategy strategy{"AAPL", 20, 10, 0.005};
    require_same_records(
        replay(strategy, bars),
        reference_replay(bars, 20, 10, 0.005, PriceSource::Close)
    );
}

TEST_CASE("Channel breakout upper threshold overflow cannot create a bullish signal") {
    const double max = std::numeric_limits<double>::max();
    ChannelBreakoutStrategy strategy{"AAPL", 1, 1, 0.5};
    require_no_signal(strategy, make_bar(0, max, max, max));
    require_no_signal(strategy, make_bar(1, max, max, max));
}

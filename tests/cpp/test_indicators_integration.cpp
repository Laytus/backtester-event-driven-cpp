#include "backtester/events/event.hpp"
#include "backtester/indicators/bollinger_bands.hpp"
#include "backtester/indicators/exponential_moving_average.hpp"
#include "backtester/indicators/macd.hpp"
#include "backtester/indicators/momentum.hpp"
#include "backtester/indicators/relative_strength_index.hpp"
#include "backtester/indicators/rolling_min_max.hpp"
#include "backtester/indicators/simple_moving_average.hpp"
#include "backtester/market/bar.hpp"
#include "backtester/strategy/strategy.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using backtester::events::MarketEvent;
using backtester::indicators::BollingerBands;
using backtester::indicators::BollingerBandsValue;
using backtester::indicators::ExponentialMovingAverage;
using backtester::indicators::MACD;
using backtester::indicators::MACDValue;
using backtester::indicators::Momentum;
using backtester::indicators::RelativeStrengthIndex;
using backtester::indicators::RollingMinMax;
using backtester::indicators::RollingMinMaxValue;
using backtester::indicators::SimpleMovingAverage;
using backtester::market::Bar;
using backtester::market::SessionDate;

SessionDate session_date(std::size_t index) {
    const std::chrono::sys_days first{
        std::chrono::year{2026} / std::chrono::October / 1
    };
    return SessionDate{first + std::chrono::days{static_cast<int>(index)}};
}

Bar make_bar(
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

std::vector<Bar> make_bars(
    std::size_t count = 50,
    const std::string& symbol = "AAPL",
    double baseline = 100.0
) {
    std::vector<Bar> bars;
    bars.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        const double close = baseline
            + 0.35 * static_cast<double>(i)
            + static_cast<double>((i * 7) % 9) - 4.0;
        const double open = close + (i % 2 == 0 ? 1.5 : -1.25);
        const double high = std::max(open, close)
            + 2.0 + static_cast<double>(i % 4);
        const double low = std::min(open, close)
            - 3.0 - static_cast<double>((i + 1) % 3);

        bars.push_back(make_bar(i, open, high, low, close, symbol));
    }
    return bars;
}

struct IndicatorSnapshot final {
    SessionDate session_date;
    std::string symbol;
    double close;

    std::optional<double> sma;
    std::optional<double> ema;
    std::optional<double> momentum;
    MACDValue macd;
    std::optional<double> rsi;
    std::optional<BollingerBandsValue> bollinger;

    std::optional<RollingMinMaxValue> previous_high_channel;
    std::optional<RollingMinMaxValue> previous_low_channel;
    std::optional<RollingMinMaxValue> current_high_channel;
    std::optional<RollingMinMaxValue> current_low_channel;

    bool upward_breakout;
    bool downward_breakout;
};

// Test-only integration adapter. Production indicators remain independent
// of Bar, MarketEvent and BacktestEngine.
class RecordingIndicatorStrategy final : public backtester::strategy::Strategy {
public:
    explicit RecordingIndicatorStrategy(
        std::size_t channel_period = 20,
        double bollinger_multiplier = 2.0
    )
        : high_channel_(channel_period),
          low_channel_(channel_period),
          bollinger_(20, bollinger_multiplier) {}

    std::vector<backtester::events::SignalEvent> on_market(
        const MarketEvent& event
    ) override {
        const Bar& bar = event.bar();

        // A prior-bar breakout must be evaluated before including bar(t).
        const auto previous_high = high_channel_.value();
        const auto previous_low = low_channel_.value();
        const bool upward = previous_high.has_value()
            && bar.close() > previous_high->maximum;
        const bool downward = previous_low.has_value()
            && bar.close() < previous_low->minimum;

        // These indicators observe the completed Close(t).
        sma_.update(bar.close());
        ema_.update(bar.close());
        momentum_.update(bar.close());
        macd_.update(bar.close());
        rsi_.update(bar.close());
        bollinger_.update(bar.close());

        // Channels receive High(t) and Low(t), after the prior-bar query.
        high_channel_.update(bar.high());
        low_channel_.update(bar.low());

        snapshots_.push_back(IndicatorSnapshot{
            bar.session_date(),
            bar.symbol(),
            bar.close(),
            sma_.value(),
            ema_.value(),
            momentum_.value(),
            macd_.value(),
            rsi_.value(),
            bollinger_.value(),
            previous_high,
            previous_low,
            high_channel_.value(),
            low_channel_.value(),
            upward,
            downward
        });

        // Task 4.1.8 validates indicator plumbing, not trading signals.
        return {};
    }

    [[nodiscard]] const std::vector<IndicatorSnapshot>& snapshots() const noexcept {
        return snapshots_;
    }

private:
    SimpleMovingAverage sma_;
    ExponentialMovingAverage ema_;
    Momentum momentum_;
    MACD macd_;
    RelativeStrengthIndex rsi_;
    RollingMinMax high_channel_;
    RollingMinMax low_channel_;
    BollingerBands bollinger_;
    std::vector<IndicatorSnapshot> snapshots_;
};

void deliver(RecordingIndicatorStrategy& strategy, const Bar& bar) {
    const auto signals = strategy.on_market(MarketEvent{bar});
    REQUIRE(signals.empty());
}

void deliver_all(RecordingIndicatorStrategy& strategy, const std::vector<Bar>& bars) {
    for (const auto& bar : bars) {
        deliver(strategy, bar);
    }
}

void require_optional_equal(
    const std::optional<double>& actual,
    const std::optional<double>& expected
) {
    REQUIRE(actual.has_value() == expected.has_value());
    if (expected) {
        REQUIRE(*actual == *expected);
    }
}

void require_extrema_equal(
    const std::optional<RollingMinMaxValue>& actual,
    const std::optional<RollingMinMaxValue>& expected
) {
    REQUIRE(actual.has_value() == expected.has_value());
    if (expected) {
        REQUIRE(actual->minimum == expected->minimum);
        REQUIRE(actual->maximum == expected->maximum);
    }
}

void require_bollinger_equal(
    const std::optional<BollingerBandsValue>& actual,
    const std::optional<BollingerBandsValue>& expected
) {
    REQUIRE(actual.has_value() == expected.has_value());
    if (expected) {
        REQUIRE(actual->middle_band == expected->middle_band);
        REQUIRE(actual->upper_band == expected->upper_band);
        REQUIRE(actual->lower_band == expected->lower_band);
    }
}

void require_same_snapshot(
    const IndicatorSnapshot& actual,
    const IndicatorSnapshot& expected
) {
    REQUIRE(actual.session_date == expected.session_date);
    REQUIRE(actual.symbol == expected.symbol);
    REQUIRE(actual.close == expected.close);
    require_optional_equal(actual.sma, expected.sma);
    require_optional_equal(actual.ema, expected.ema);
    require_optional_equal(actual.momentum, expected.momentum);
    require_optional_equal(actual.rsi, expected.rsi);
    require_optional_equal(actual.macd.macd_line, expected.macd.macd_line);
    require_optional_equal(actual.macd.signal_line, expected.macd.signal_line);
    require_optional_equal(actual.macd.histogram, expected.macd.histogram);
    require_bollinger_equal(actual.bollinger, expected.bollinger);
    require_extrema_equal(actual.previous_high_channel, expected.previous_high_channel);
    require_extrema_equal(actual.previous_low_channel, expected.previous_low_channel);
    require_extrema_equal(actual.current_high_channel, expected.current_high_channel);
    require_extrema_equal(actual.current_low_channel, expected.current_low_channel);
    REQUIRE(actual.upward_breakout == expected.upward_breakout);
    REQUIRE(actual.downward_breakout == expected.downward_breakout);
}

}  // namespace

TEST_CASE("Indicator integration reads the real MarketEvent Bar payload") {
    RecordingIndicatorStrategy strategy;
    const Bar bar = make_bar(0, 98.0, 107.0, 95.0, 103.0);

    deliver(strategy, bar);
    REQUIRE(strategy.snapshots().size() == 1);
    const auto& state = strategy.snapshots().front();
    REQUIRE(state.session_date == bar.session_date());
    REQUIRE(state.symbol == "AAPL");
    REQUIRE(state.close == 103.0);
    REQUIRE_FALSE(state.sma.has_value());
    REQUIRE_FALSE(state.current_high_channel.has_value());
}

TEST_CASE("Indicator integration aligns every default warm-up with its MarketEvent") {
    RecordingIndicatorStrategy strategy;
    const auto bars = make_bars(50);

    for (std::size_t i = 0; i < bars.size(); ++i) {
        deliver(strategy, bars[i]);
        const auto& state = strategy.snapshots().back();
        const std::size_t n = i + 1;

        REQUIRE(strategy.snapshots().size() == n);
        REQUIRE(state.session_date == bars[i].session_date());
        REQUIRE(state.sma.has_value() == (n >= 20));
        REQUIRE(state.ema.has_value() == (n >= 20));
        REQUIRE(state.momentum.has_value() == (n >= 11));
        REQUIRE(state.rsi.has_value() == (n >= 15));
        REQUIRE(state.macd.macd_line.has_value() == (n >= 26));
        REQUIRE(state.macd.signal_line.has_value() == (n >= 34));
        REQUIRE(state.macd.histogram.has_value() == (n >= 34));
        REQUIRE(state.bollinger.has_value() == (n >= 20));
        REQUIRE(state.current_high_channel.has_value() == (n >= 20));
        REQUIRE(state.current_low_channel.has_value() == (n >= 20));
        REQUIRE(state.previous_high_channel.has_value() == (n >= 21));
        REQUIRE(state.previous_low_channel.has_value() == (n >= 21));
    }
}

TEST_CASE("Indicator integration Close indicators match independently driven implementations") {
    RecordingIndicatorStrategy strategy;
    SimpleMovingAverage sma;
    ExponentialMovingAverage ema;
    Momentum momentum;
    MACD macd;
    RelativeStrengthIndex rsi;
    BollingerBands bands;

    for (const auto& bar : make_bars(55)) {
        deliver(strategy, bar);
        const auto& state = strategy.snapshots().back();

        sma.update(bar.close());
        ema.update(bar.close());
        momentum.update(bar.close());
        macd.update(bar.close());
        rsi.update(bar.close());
        bands.update(bar.close());

        require_optional_equal(state.sma, sma.value());
        require_optional_equal(state.ema, ema.value());
        require_optional_equal(state.momentum, momentum.value());
        require_optional_equal(state.rsi, rsi.value());
        require_optional_equal(state.macd.macd_line, macd.value().macd_line);
        require_optional_equal(state.macd.signal_line, macd.value().signal_line);
        require_optional_equal(state.macd.histogram, macd.value().histogram);
        require_bollinger_equal(state.bollinger, bands.value());

        if (state.sma && state.bollinger) {
            REQUIRE(
                state.bollinger->middle_band ==
                Catch::Approx(*state.sma).epsilon(1e-12)
            );
        }
    }
}

TEST_CASE("Indicator integration routes High and Low to independent rolling channels") {
    RecordingIndicatorStrategy strategy;
    const auto bars = make_bars(65);
    deliver_all(strategy, bars);

    for (std::size_t i = 19; i < bars.size(); ++i) {
        double expected_high = bars[i - 19].high();
        double expected_low = bars[i - 19].low();
        for (std::size_t j = i - 19; j <= i; ++j) {
            expected_high = std::max(expected_high, bars[j].high());
            expected_low = std::min(expected_low, bars[j].low());
        }

        const auto& state = strategy.snapshots()[i];
        REQUIRE(state.current_high_channel.has_value());
        REQUIRE(state.current_low_channel.has_value());
        REQUIRE(state.current_high_channel->maximum == expected_high);
        REQUIRE(state.current_low_channel->minimum == expected_low);
    }
}

TEST_CASE("Indicator integration queries prior channels before adding the current Bar") {
    RecordingIndicatorStrategy strategy{2};

    deliver(strategy, make_bar(0, 10.0, 11.0, 9.0, 10.0));
    deliver(strategy, make_bar(1, 12.0, 13.0, 11.0, 12.0));

    const auto& second = strategy.snapshots().back();
    REQUIRE_FALSE(second.previous_high_channel.has_value());
    REQUIRE_FALSE(second.upward_breakout);

    deliver(strategy, make_bar(2, 15.0, 16.0, 14.0, 15.0));
    const auto& third = strategy.snapshots().back();
    REQUIRE(third.previous_high_channel.has_value());
    REQUIRE(third.previous_high_channel->maximum == 13.0);
    REQUIRE(third.current_high_channel->maximum == 16.0);
    REQUIRE(third.upward_breakout);  // 15 > previous 13, but not current 16.
    REQUIRE_FALSE(third.downward_breakout);

    deliver(strategy, make_bar(3, 7.0, 8.0, 6.0, 7.0));
    const auto& fourth = strategy.snapshots().back();
    REQUIRE(fourth.previous_low_channel.has_value());
    REQUIRE(fourth.previous_low_channel->minimum == 11.0);
    REQUIRE(fourth.current_low_channel->minimum == 6.0);
    REQUIRE(fourth.downward_breakout);  // 7 < previous 11, but not current 6.
}

TEST_CASE("Indicator integration prior-bar channels match a historical-only oracle") {
    RecordingIndicatorStrategy strategy;
    const auto bars = make_bars(85);
    deliver_all(strategy, bars);

    for (std::size_t i = 20; i < bars.size(); ++i) {
        double previous_max = bars[i - 20].high();
        double previous_min = bars[i - 20].low();
        for (std::size_t j = i - 20; j < i; ++j) {
            previous_max = std::max(previous_max, bars[j].high());
            previous_min = std::min(previous_min, bars[j].low());
        }

        const auto& state = strategy.snapshots()[i];
        REQUIRE(state.previous_high_channel.has_value());
        REQUIRE(state.previous_low_channel.has_value());
        REQUIRE(state.previous_high_channel->maximum == previous_max);
        REQUIRE(state.previous_low_channel->minimum == previous_min);
        REQUIRE(state.upward_breakout == (bars[i].close() > previous_max));
        REQUIRE(state.downward_breakout == (bars[i].close() < previous_min));
    }
}

TEST_CASE("Indicator integration early results cannot depend on later MarketEvents") {
    const auto bars = make_bars(45);
    RecordingIndicatorStrategy full;
    RecordingIndicatorStrategy prefix;

    deliver_all(full, bars);
    for (std::size_t i = 0; i < 35; ++i) {
        deliver(prefix, bars[i]);
    }

    REQUIRE(prefix.snapshots().size() == 35);
    for (std::size_t i = 0; i < prefix.snapshots().size(); ++i) {
        require_same_snapshot(prefix.snapshots()[i], full.snapshots()[i]);
    }
}

TEST_CASE("Indicator integration separate Strategy instances isolate instrument state") {
    RecordingIndicatorStrategy aapl;
    RecordingIndicatorStrategy msft;
    RecordingIndicatorStrategy msft_replay;

    const auto aapl_bars = make_bars(50, "AAPL", 100.0);
    const auto msft_bars = make_bars(50, "MSFT", 250.0);

    for (std::size_t i = 0; i < 50; ++i) {
        deliver(aapl, aapl_bars[i]);
        deliver(msft, msft_bars[i]);
    }
    deliver_all(msft_replay, msft_bars);

    REQUIRE(aapl.snapshots().size() == 50);
    REQUIRE(msft.snapshots().size() == 50);
    for (std::size_t i = 0; i < 50; ++i) {
        REQUIRE(aapl.snapshots()[i].symbol == "AAPL");
        REQUIRE(msft.snapshots()[i].symbol == "MSFT");
        require_same_snapshot(msft.snapshots()[i], msft_replay.snapshots()[i]);
    }
    REQUIRE(aapl.snapshots().back().sma.has_value());
    REQUIRE(msft.snapshots().back().sma.has_value());
    REQUIRE(*aapl.snapshots().back().sma != *msft.snapshots().back().sma);
}

TEST_CASE("Indicator integration replays the same MarketEvents deterministically") {
    const auto bars = make_bars(80);
    RecordingIndicatorStrategy first;
    RecordingIndicatorStrategy second;

    deliver_all(first, bars);
    deliver_all(second, bars);
    REQUIRE(first.snapshots().size() == bars.size());
    REQUIRE(second.snapshots().size() == bars.size());

    for (std::size_t i = 0; i < bars.size(); ++i) {
        require_same_snapshot(first.snapshots()[i], second.snapshots()[i]);
    }
}

TEST_CASE("Indicator integration remains aligned over many complete windows") {
    const auto bars = make_bars(260);
    RecordingIndicatorStrategy strategy;
    deliver_all(strategy, bars);
    REQUIRE(strategy.snapshots().size() == bars.size());

    for (std::size_t i = 33; i < bars.size(); ++i) {
        const auto& state = strategy.snapshots()[i];
        REQUIRE(state.sma.has_value());
        REQUIRE(state.ema.has_value());
        REQUIRE(state.momentum.has_value());
        REQUIRE(state.macd.macd_line.has_value());
        REQUIRE(state.macd.signal_line.has_value());
        REQUIRE(state.macd.histogram.has_value());
        REQUIRE(state.rsi.has_value());
        REQUIRE(state.bollinger.has_value());
        REQUIRE(state.previous_high_channel.has_value());
        REQUIRE(state.previous_low_channel.has_value());
    }
}

TEST_CASE("Indicator integration propagates numerical failure instead of committing a snapshot") {
    // A very large but finite multiplier makes the first nonconstant
    // 20-price Bollinger window unrepresentable. The adapter intentionally
    // does not attempt cross-indicator rollback: processing stops on error.
    RecordingIndicatorStrategy strategy{20, 1e308};

    for (std::size_t i = 0; i < 19; ++i) {
        deliver(strategy, make_bar(i, 100.0, 102.0, 98.0, 100.0));
    }
    REQUIRE(strategy.snapshots().size() == 19);

    REQUIRE_THROWS_AS(
        deliver(strategy, make_bar(19, 110.0, 112.0, 108.0, 110.0)),
        std::overflow_error
    );
    REQUIRE(strategy.snapshots().size() == 19);
}

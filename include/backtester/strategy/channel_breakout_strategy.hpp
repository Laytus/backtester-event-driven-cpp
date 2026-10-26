#pragma once

#include "backtester/indicators/rolling_min_max.hpp"
#include "backtester/strategy/price_source.hpp"
#include "backtester/strategy/strategy.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace backtester::strategy {

class ChannelBreakoutStrategy final : public Strategy {
public:
    explicit ChannelBreakoutStrategy(
        std::string symbol,
        std::size_t entry_period = 20,
        std::size_t exit_period = 20,
        double breakout_buffer = 0.0,
        PriceSource reference_price = PriceSource::Close
    );

    [[nodiscard]]
    std::vector<events::SignalEvent>
    on_market(const events::MarketEvent& event) override;

private:
    std::string symbol_;
    double breakout_buffer_;
    PriceSource reference_price_;

    indicators::RollingMinMax upper_channel_;
    indicators::RollingMinMax lower_channel_;

    events::SignalDirection target_{
        events::SignalDirection::Flat
    };
};

}  // namespace backtester::strategy
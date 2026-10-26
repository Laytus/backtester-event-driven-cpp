#pragma once

#include "backtester/indicators/rolling_min_max.hpp"
#include "backtester/strategy/price_source.hpp"
#include "backtester/strategy/strategy.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace backtester::strategy {

class PriceFilterStrategy final : public Strategy {
public:
    explicit PriceFilterStrategy(
        std::string symbol,
        std::size_t lookback_period = 1,
        double filter_threshold = 0.01,
        PriceSource price_source = PriceSource::Close
    );

    [[nodiscard]]
    std::vector<events::SignalEvent>
    on_market(const events::MarketEvent& event) override;

private:
    std::string symbol_;
    double filter_threshold_;
    PriceSource price_source_;

    indicators::RollingMinMax price_window_;

    events::SignalDirection target_{
        events::SignalDirection::Flat
    };
};

}  // namespace backtester::strategy
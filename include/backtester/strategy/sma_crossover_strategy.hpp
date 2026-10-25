#pragma once

#include "backtester/strategy/strategy.hpp"
#include "backtester/strategy/price_source.hpp"
#include "backtester/indicators/simple_moving_average.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace backtester::strategy {

class SMACrossoverStrategy final : public Strategy {
public:
    explicit SMACrossoverStrategy(
        std::string symbol,
        std::size_t fast_period = 20,
        std::size_t slow_period = 50,
        PriceSource price_source = PriceSource::Close,
        double crossover_threshold = 0.0
    );

    [[nodiscard]]
    std::vector<events::SignalEvent>
    on_market(const events::MarketEvent& event) override;

private:
    std::string symbol_;
    PriceSource price_source_;
    double crossover_threshold_;

    indicators::SimpleMovingAverage fast_sma_;
    indicators::SimpleMovingAverage slow_sma_;

    events::SignalDirection target_{
        events::SignalDirection::Flat
    };
};

}  // namespace backtester::strategy
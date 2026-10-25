#pragma once

#include "backtester/strategy/strategy.hpp"
#include "backtester/strategy/price_source.hpp"
#include "backtester/indicators/exponential_moving_average.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace backtester::strategy {

class EMACrossoverStrategy final : public Strategy {
public:
    explicit EMACrossoverStrategy(
        std::string symbol,
        std::size_t fast_period = 20,
        std::size_t slow_period = 50,
        PriceSource price_source = PriceSource::Close,
        double crossover_threshold = 0.0,
        std::optional<double> fast_alpha = std::nullopt,
        std::optional<double> slow_alpha = std::nullopt
    );

    [[nodiscard]]
    std::vector<events::SignalEvent>
    on_market(const events::MarketEvent& event) override;

private:
    std::string symbol_;
    PriceSource price_source_;
    double crossover_threshold_;

    indicators::ExponentialMovingAverage fast_ema_;
    indicators::ExponentialMovingAverage slow_ema_;

    events::SignalDirection target_{
        events::SignalDirection::Flat
    };
};

}  // namespace backtester::strategy
#pragma once

#include "backtester/indicators/momentum.hpp"
#include "backtester/strategy/price_source.hpp"
#include "backtester/strategy/strategy.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace backtester::strategy {

class MomentumStrategy final : public Strategy {
public:
    explicit MomentumStrategy(
        std::string symbol,
        std::size_t momentum_period = 10,
        indicators::MomentumMode momentum_mode =
            indicators::MomentumMode::Absolute,
        double momentum_threshold = 0.0,
        PriceSource price_source = PriceSource::Close
    );

    [[nodiscard]]
    std::vector<events::SignalEvent>
    on_market(const events::MarketEvent& event) override;

private:
    std::string symbol_;
    double momentum_threshold_;
    PriceSource price_source_;

    indicators::Momentum momentum_;

    events::SignalDirection target_{
        events::SignalDirection::Flat
    };
};

}  // namespace backtester::strategy
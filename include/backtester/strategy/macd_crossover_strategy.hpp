#pragma once

#include "backtester/indicators/macd.hpp"
#include "backtester/strategy/price_source.hpp"
#include "backtester/strategy/strategy.hpp"

#include <cstddef>
#include <string>
#include <optional>

namespace backtester::strategy {

class MACDCrossoverStrategy final : public Strategy {
public:
    explicit MACDCrossoverStrategy(
        std::string symbol,
        std::size_t fast_period = 12,
        std::size_t slow_period = 26,
        std::size_t signal_period = 9,
        PriceSource price_source = PriceSource::Close,
        double crossover_threshold = 0.0,
        std::optional<double> fast_alpha = std::nullopt,
        std::optional<double> slow_alpha = std::nullopt,
        std::optional<double> signal_alpha = std::nullopt
    );

    [[nodiscard]]
    std::vector<events::SignalEvent>
    on_market(const events::MarketEvent& event) override;

private:
    std::string symbol_;
    PriceSource price_source_;
    double crossover_threshold_;

    indicators::MACD macd_;

    events::SignalDirection target_{
        events::SignalDirection::Flat
    };
};

}  // namespace backtester::strategy
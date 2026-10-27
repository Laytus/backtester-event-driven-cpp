#pragma once

#include "backtester/indicators/relative_strength_index.hpp"
#include "backtester/strategy/price_source.hpp"
#include "backtester/strategy/strategy.hpp"

#include <cstddef>
#include <string>
#include <optional>
#include <vector>

namespace backtester::strategy {

enum class RSISignalMode {
    DirectThreshold,
    RecoveryConfirmation
};

class RSIBasedStrategy final : public Strategy {
public:
    explicit RSIBasedStrategy(
        std::string symbol,
        std::size_t rsi_period = 14,
        indicators::RSIMode rsi_mode =
            indicators::RSIMode::Directional,
        double oversold_threshold = 30.0,
        double overbought_threshold = 70.0,
        PriceSource price_source = PriceSource::Close,
        RSISignalMode signal_mode =
            RSISignalMode::DirectThreshold,
        std::optional<double> rsi_beta = std::nullopt
    );

    [[nodiscard]]
    std::vector<events::SignalEvent>
    on_market(const events::MarketEvent& event) override;

private:
    std::string symbol_;

    double oversold_threshold_;
    double overbought_threshold_;

    PriceSource price_source_;
    RSISignalMode signal_mode_;

    indicators::RelativeStrengthIndex rsi_;

    std::optional<double> previous_rsi_{std::nullopt};

    events::SignalDirection target_{
        events::SignalDirection::Flat
    };
};

}  // namespace backtester::strategy
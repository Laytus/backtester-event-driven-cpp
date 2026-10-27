#include "backtester/strategy/rsi_based_strategy.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace backtester::strategy {

RSIBasedStrategy::RSIBasedStrategy(
    std::string symbol,
    std::size_t rsi_period,
    indicators::RSIMode rsi_mode,
    double oversold_threshold,
    double overbought_threshold,
    PriceSource price_source,
    RSISignalMode signal_mode,
    std::optional<double> rsi_beta
)
    : symbol_(std::move(symbol)),
    oversold_threshold_(oversold_threshold),
    overbought_threshold_(overbought_threshold),
    price_source_(price_source),
    signal_mode_(signal_mode),
    rsi_(rsi_period, rsi_mode, rsi_beta) {
        if (symbol_.empty()) {
            throw std::invalid_argument(
                "RSI strategy requires a non-empty symbol"
            );
        }

        if (
            !std::isfinite(oversold_threshold_) ||
            oversold_threshold_ <= 0.0 ||
            oversold_threshold_ >= 100.0
        ) {
            throw std::invalid_argument(
                "RSI oversold threshold must be finite and in (0, 100)"
            );
        }

        if (
            !std::isfinite(overbought_threshold_) ||
            overbought_threshold_ <= 0.0 ||
            overbought_threshold_ >= 100.0
        ) {
            throw std::invalid_argument(
                "RSI overbought threshold must be finite and in (0, 100)"
            );
        }

        if (oversold_threshold_ >= overbought_threshold_) {
            throw std::invalid_argument(
                "RSI oversold threshold must be less than overbought threshold"
            );
        }

        if (!is_valid_price_source(price_source_)) {
            throw std::invalid_argument(
                "Invalid RSI strategy price source"
            );
        }

        if (
            signal_mode_ != RSISignalMode::DirectThreshold &&
            signal_mode_ != RSISignalMode::RecoveryConfirmation
        ) {
            throw std::invalid_argument(
                "Invalid RSI strategy signal mode"
            );
        }
    }

std::vector<events::SignalEvent>
RSIBasedStrategy::on_market(const events::MarketEvent& event) {
    const auto& bar = event.bar();

    if (bar.symbol() != symbol_) {
        return {};
    }

    const double observation = select_price(bar, price_source_);
    rsi_.update(observation);

    const auto current_rsi = rsi_.value();

    if (!current_rsi.has_value()) {
        return {};
    }

    auto desired_target = target_;

    if (signal_mode_ == RSISignalMode::DirectThreshold) {
        if (*current_rsi < oversold_threshold_) {
            desired_target = events::SignalDirection::Long;
        } else if (*current_rsi > overbought_threshold_) {
            desired_target = events::SignalDirection::Flat;
        }
    } else {
        if (previous_rsi_.has_value()) {
            if (
                *previous_rsi_ < oversold_threshold_ &&
                *current_rsi >= oversold_threshold_
            ) {
                desired_target = events::SignalDirection::Long;
            } else if (
                *previous_rsi_ > overbought_threshold_ &&
                *current_rsi <= overbought_threshold_
            ) {
                desired_target = events::SignalDirection::Flat;
            }
        }
    }

    previous_rsi_ = *current_rsi;

    if (desired_target == target_) {
        return {};
    }

    std::vector<events::SignalEvent> signals;
    signals.emplace_back(
        symbol_,
        desired_target,
        bar.session_date()
    );

    target_ = desired_target;

    return signals;
}

}  // namespace backtester::strategy
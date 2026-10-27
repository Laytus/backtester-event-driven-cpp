#include "backtester/strategy/macd_crossover_strategy.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace backtester::strategy {

MACDCrossoverStrategy::MACDCrossoverStrategy(
    std::string symbol,
    std::size_t fast_period,
    std::size_t slow_period,
    std::size_t signal_period,
    PriceSource price_source,
    double crossover_threshold,
    std::optional<double> fast_alpha,
    std::optional<double> slow_alpha,
    std::optional<double> signal_alpha
)
    : symbol_(std::move(symbol)),
    price_source_(price_source),
    crossover_threshold_(crossover_threshold),
    macd_(
        fast_period,
        slow_period,
        signal_period,
        fast_alpha,
        slow_alpha,
        signal_alpha
    ) {
        if (symbol_.empty()) {
            throw std::invalid_argument(
                "MACD crossover requires a non-empty symbol"
            );
        }

        if (
            !std::isfinite(crossover_threshold_) ||
            crossover_threshold_ < 0.0
        ) {
            throw std::invalid_argument(
                "MACD crossover threshold must be finite and non-negative"
            );
        }

        if (!is_valid_price_source(price_source_)) {
            throw std::invalid_argument(
                "Invalid MACD crossover price source"
            );
        }
    }

std::vector<events::SignalEvent>
MACDCrossoverStrategy::on_market(const events::MarketEvent& event) {
    const auto& bar = event.bar();

    if (bar.symbol() != symbol_) {
        return {};
    }

    const double observation = select_price(bar, price_source_);
    macd_.update(observation);

    const auto result = macd_.value();

    if (!result.histogram.has_value()) {
        return {};
    }

    const double histogram = *result.histogram;

    auto desired_target = target_;

    if (histogram > crossover_threshold_) {
        desired_target = events::SignalDirection::Long;
    } else if (histogram < -crossover_threshold_) {
        desired_target = events::SignalDirection::Flat;
    }

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
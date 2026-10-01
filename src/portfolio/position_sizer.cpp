#include "backtester/portfolio/position_sizer.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace backtester::portfolio {

PositionSizer::PositionSizer(double target_allocation)
    : target_allocation_(target_allocation) {
        if (!std::isfinite(target_allocation_)) {
            throw std::invalid_argument(
                "PositionSizer target allocation must be finite"
            );
        }

        if (
            target_allocation_ <= 0.0 ||
            target_allocation_ > 1.0
        ) {
            throw std::invalid_argument(
                "PositionSizer target allocation must be greater than 0 and smaller or equal than 1"
            );
        }
    }

PositionQuantity PositionSizer::target_position(
    const events::SignalEvent& signal,
    const Portfolio& portfolio,
    double reference_price
) const {
    if (signal.symbol() != portfolio.symbol()) {
        throw std::invalid_argument(
            "SignalEvent symbol does not match Portfolio symbol"
        );
    }

    if (!std::isfinite(reference_price)) {
        throw std::invalid_argument(
            "PositionSizer reference price must be finite"
        );
    }

    if (reference_price <= 0.0) {
        throw std::invalid_argument(
            "PositionSizer reference price must be strictly positive"
        );
    }

    if (signal.direction() == events::SignalDirection::Flat) {
        return 0;
    }

    // MVP
    if (signal.direction() == events::SignalDirection::Short) {
        throw std::invalid_argument(
            "Short target generation is not supported by the MVP"
        );
    }

    const double reference_market_value =
        static_cast<double>(portfolio.position_quantity())
        * reference_price;

    if (!std::isfinite(reference_market_value)) {
        throw std::overflow_error(
            "PositionSizer reference market value must be finite"
        );
    }

    const double reference_equity =
    portfolio.cash() + reference_market_value;

    if (!std::isfinite(reference_equity)) {
        throw std::overflow_error(
            "PositionSizer reference equity must be finite"
        );
    }

    if (reference_equity <= 0.0) {
        throw std::invalid_argument(
            "PositionSizer reference equity must be strictly positive"
        );
    }

    const double target_value =
        target_allocation_ * reference_equity;

    if (!std::isfinite(target_value)) {
        throw std::overflow_error(
            "PositionSizer target value must be finite"
        );
    }

    const double raw_quantity = target_value / reference_price;

    if (!std::isfinite(raw_quantity)) {
        throw std::overflow_error(
            "PositionSizer raw quantity must be finite"
        );
    }

    const double floored_quantity = std::floor(raw_quantity);

    const double position_limit = static_cast<double>(
        std::numeric_limits<PositionQuantity>::max()
    );

    if (floored_quantity >= position_limit) {
        throw std::overflow_error(
            "Target position exceeds PositionQuantity range"
        );
    }

    return static_cast<PositionQuantity>(floored_quantity);
}

}  // namespace backtester::portfolio
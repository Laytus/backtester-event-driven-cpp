#include "backtester/portfolio/order_generator.hpp"

#include <cstdint>
#include <limits>
#include<stdexcept>

namespace backtester::portfolio {

std::optional<events::OrderEvent> OrderGenerator::generate(
    const events::SignalEvent& signal,
    const Portfolio& portfolio,
    PositionQuantity target_position
) const {
    if (signal.symbol() != portfolio.symbol()) {
        throw std::invalid_argument(
            "SignalEvent symbol does not match Portfolio symbol"
        );
    }

    // MVP
    if (target_position < 0) {
        throw std::invalid_argument(
            "Negative target positions are not supported by the MVP"
        );
    }

    const PositionQuantity current_position =
        portfolio.position_quantity();

    if (target_position == current_position) {
        return std::nullopt;
    }

    if (target_position > current_position) {
        if (
            current_position < 0 &&
            target_position
                > std::numeric_limits<PositionQuantity>::max()
                + current_position
        ) {
            throw std::overflow_error(
                "Order quantity exceeds PositionQuantity range"
            );
        }

        const PositionQuantity delta = target_position - current_position;

        const auto quantity = static_cast<std::uint64_t>(delta);

        return events::OrderEvent{
            signal.symbol(),
            events::OrderSide::Buy,
            quantity,
            events::OrderType::Market,
            signal.signal_date()
        };
    }

    if (target_position < current_position) {
        const PositionQuantity delta = current_position - target_position;

        const auto quantity = static_cast<std::uint64_t>(delta);

        return events::OrderEvent{
            signal.symbol(),
            events::OrderSide::Sell,
            quantity,
            events::OrderType::Market,
            signal.signal_date()
        };
    }

    return std::nullopt;
}

}  // namespace backtester::portfolio
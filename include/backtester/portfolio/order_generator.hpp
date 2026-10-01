#pragma once

#include "backtester/events/event.hpp"
#include "backtester/portfolio/portfolio.hpp"

#include <optional>

namespace backtester::portfolio {

class OrderGenerator final {
public:
    [[nodiscard]]
    std::optional<events::OrderEvent> generate(
        const events::SignalEvent& signal,
        const Portfolio& portfolio,
        PositionQuantity target_position
    ) const;
};

}  // namespace backtester::portfolio
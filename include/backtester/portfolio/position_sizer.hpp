#pragma once

#include "backtester/events/event.hpp"
#include "backtester/portfolio/portfolio.hpp"

namespace backtester::portfolio {

class PositionSizer final {
public:
    explicit PositionSizer(double target_allocation);

    [[nodiscard]]
    PositionQuantity target_position(
        const events::SignalEvent& signal,
        const Portfolio& portfolio,
        double reference_price
    ) const;

private:
    double target_allocation_;
};

}  // namespace backtester::portfolio
#include "backtester/execution/execution_handler.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace backtester::execution {

bool ExecutionHandler::has_pending_order() const noexcept {
    return pending_order_.has_value();
}

void ExecutionHandler::submit(events::OrderEvent order) {
    if (has_pending_order()) {
        throw std::logic_error(
            "ExecutionHandler already has a pending order"
        );
    }

    if (order.order_type() != events::OrderType::Market) {
        throw std::invalid_argument(
            "ExecutionHandler order must be a market order"
        );
    }

    pending_order_ = std::move(order);
}

std::optional<events::FillEvent> ExecutionHandler::execute_on_open(
    market::SessionDate session_date,
    double open_price,
    const portfolio::Portfolio& portfolio
) {
    if (!has_pending_order()) {
        return std::nullopt;
    }

    if (!session_date.ok()) {
        throw std::invalid_argument(
            "ExecutionHandler session date is invalid"
        );
    }

    if (!std::isfinite(open_price)) {
        throw std::invalid_argument(
            "ExecutionHandler open price must be finite"
        );
    }

    if (open_price <= 0) {
        throw std::invalid_argument(
            "ExecutionHandler open price must be strictly positive"
        );
    }

    const auto& order = *pending_order_;

    if (order.symbol() != portfolio.symbol()) {
        throw std::invalid_argument(
            "Pending order symbol does not match Portfolio symbol"
        );
    }

    if (session_date < order.submission_date()) {
        throw std::logic_error(
            "Execution date precedes order submission date"
        );
    }

    if (session_date == order.submission_date()) {
        return std::nullopt;
    }

    const std::uint64_t requested_quantity = order.quantity();

    std::uint64_t approved_quantity = requested_quantity;

    if (order.side() == events::OrderSide::Buy) {
        const double available_cash = std::max(portfolio.cash(), 0.0);

        if (available_cash <= 0.0) {
            approved_quantity = 0;
        } else {
            const double affordable_quantity =
                std::floor(available_cash / open_price);

            const double uint64_limit_exclusive =
                std::ldexp(
                    1.0,
                    std::numeric_limits<std::uint64_t>::digits
                );

            if (
                !std::isfinite(affordable_quantity) ||
                affordable_quantity >= uint64_limit_exclusive
            ) {
                approved_quantity = requested_quantity;
            } else {
                const auto max_affordable =
                    static_cast<std::uint64_t>(affordable_quantity);

                approved_quantity =
                    std::min(requested_quantity, max_affordable);
            }
        }
    }

    if (approved_quantity == 0) {
        pending_order_.reset();
        return std::nullopt;
    }

    events::FillEvent fill{
        order.symbol(),
        order.side(),
        approved_quantity,
        open_price,
        session_date,
        0.0
    };

    pending_order_.reset();

    return fill;
}

void ExecutionHandler::discard_pending() noexcept {
    pending_order_.reset();
}

}  // namespace backtester::execution
#pragma once

#include "backtester/events/event.hpp"
#include "backtester/market/bar.hpp"
#include "backtester/portfolio/portfolio.hpp"

#include <optional>

namespace backtester::execution {

class ExecutionHandler final {
public:
    ExecutionHandler() = default;

    ExecutionHandler(const ExecutionHandler&) = delete;
    ExecutionHandler& operator=(const ExecutionHandler&) = delete;

    ExecutionHandler(ExecutionHandler&&) = default;
    ExecutionHandler& operator=(ExecutionHandler&&) = default;

    [[nodiscard]]
    bool has_pending_order() const noexcept;

    void submit(events::OrderEvent order);

    [[nodiscard]]
    std::optional<events::FillEvent> execute_on_open(
        market::SessionDate session_date,
        double open_price,
        const portfolio::Portfolio& portfolio
    );

    void discard_pending() noexcept;

private:
    std::optional<events::OrderEvent> pending_order_;
};

}  // namespace backtester::execution
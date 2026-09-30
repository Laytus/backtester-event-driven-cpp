#pragma once

#include "backtester/events/event.hpp"

#include <vector>

namespace backtester::strategy {

class Strategy {
public:
    Strategy() = default;

    virtual ~Strategy() = default;

    Strategy(const Strategy&) = delete;
    Strategy& operator=(const Strategy&) = delete;

    Strategy(Strategy&&) = default;
    Strategy& operator=(Strategy&&) = default;

    [[nodiscard]]
    virtual std::vector<events::SignalEvent>
    on_market(const events::MarketEvent& event) = 0;
};

}  // namespace backtester::strategy
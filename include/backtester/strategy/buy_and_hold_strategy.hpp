#pragma once

#include "backtester/strategy/strategy.hpp"

#include <string>

namespace backtester::strategy {

class BuyAndHoldStrategy final : public Strategy {
public:
    explicit BuyAndHoldStrategy(std::string symbol);

    [[nodiscard]]
    std::vector<events::SignalEvent>
    on_market(const events::MarketEvent& event) override;

private:
    std::string symbol_;
    bool has_signaled_entry_{false};
};

}  // namespace backtester::strategy
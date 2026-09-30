#include "backtester/strategy/buy_and_hold_strategy.hpp"

#include <stdexcept>
#include <utility>

namespace backtester::strategy {

BuyAndHoldStrategy::BuyAndHoldStrategy(std::string symbol)
    : symbol_(std::move(symbol)) {
        if (symbol_.empty()) {
            throw std::invalid_argument(
                "BuyAndHoldStrategy symbol cannot be empty"
            );
        }
    }

std::vector<events::SignalEvent>
BuyAndHoldStrategy::on_market(const events::MarketEvent& event) {
    if (event.bar().symbol() != symbol_) {
        return {};
    }

    if (has_signaled_entry_ == true) {
        return {};
    }

    events::SignalEvent signal{
        event.bar().symbol(),
        events::SignalDirection::Long,
        event.bar().session_date()
    };

    has_signaled_entry_ = true;

    return {signal};
}

}  // namespace backtester::strategy
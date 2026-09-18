#include "backtester/events/event.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace backtester::events {

MarketEvent::MarketEvent(market::Bar bar)
    : bar_(std::move(bar)) {}

const market::Bar& MarketEvent::bar() const noexcept {
    return bar_;
}

SignalEvent::SignalEvent(
    std::string symbol,
    SignalDirection direction,
    market::SessionDate signal_date
)
    : symbol_(std::move(symbol)),
    direction_(direction),
    signal_date_(signal_date) {
        if (symbol_.empty()) {
            throw std::invalid_argument("Signal symbol cannot be empty");
        }

        if (!signal_date_.ok()) {
            throw std::invalid_argument("Signal date is invalid");
        }
    }

const std::string& SignalEvent::symbol() const noexcept {
    return symbol_;
}

SignalDirection SignalEvent::direction() const noexcept {
    return direction_;
}

market::SessionDate SignalEvent::signal_date() const noexcept {
    return signal_date_;
}

OrderEvent::OrderEvent(
    std::string symbol,
    OrderSide side,
    std::uint64_t quantity,
    OrderType order_type,
    market::SessionDate submission_date
)
    : symbol_(std::move(symbol)),
    side_(side),
    quantity_(quantity),
    order_type_(order_type),
    submission_date_(submission_date) {
        if (symbol_.empty()) {
            throw std::invalid_argument("Order symbol cannot be empty");
        }

        if (quantity_ == 0) {
            throw std::invalid_argument("Order quantity must be strictly positive");
        }

        if (!submission_date_.ok()) {
            throw std::invalid_argument("Order date is invalid");
        }
    }

const std::string& OrderEvent::symbol() const noexcept {
    return symbol_;
}

OrderSide OrderEvent::side() const noexcept {
    return side_;
}

std::uint64_t OrderEvent::quantity() const noexcept {
    return quantity_;
}

OrderType OrderEvent::order_type() const noexcept {
    return order_type_;
}

market::SessionDate OrderEvent::submission_date() const noexcept {
    return submission_date_;
}

FillEvent::FillEvent(
    std::string symbol,
    OrderSide side,
    std::uint64_t quantity,
    double fill_price,
    market::SessionDate fill_date,
    double fees
)
    : symbol_(std::move(symbol)),
    side_(side),
    quantity_(quantity),
    fill_price_(fill_price),
    fill_date_(fill_date),
    fees_(fees) {
        if (symbol_.empty()) {
            throw std::invalid_argument("Fill symbol cannot be empty");
        }

        if (quantity_ == 0) {
            throw std::invalid_argument("Fill quantity must be strictly positive");
        }

        if (
            !std::isfinite(fill_price_) ||
            fill_price_ <= 0
        ) {
            throw std::invalid_argument("Fill price must be finite and strictly positive");
        }

        if (!fill_date_.ok()) {
            throw std::invalid_argument("Fill date is invalid");
        }

        if (
            !std::isfinite(fees_) ||
            fees_ < 0
        ) {
            throw std::invalid_argument("Fill fees must be finite and non-negative");
        }
    }

const std::string& FillEvent::symbol() const noexcept {
    return symbol_;
}

OrderSide FillEvent::side() const noexcept {
    return side_;
}

std::uint64_t FillEvent::quantity() const noexcept {
    return quantity_;
}

double FillEvent::fill_price() const noexcept {
    return fill_price_;
}

market::SessionDate FillEvent::fill_date() const noexcept {
    return fill_date_;
}

double FillEvent::fees() const noexcept {
    return fees_;
}

}  // namespace backtester::events
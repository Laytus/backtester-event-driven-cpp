#include "backtester/market/bar.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace backtester::market {

Bar::Bar(
    SessionDate session_date,
    std::string symbol,
    double open,
    double high,
    double low,
    double close,
    std::uint64_t volume
)
    : session_date_(session_date),
    symbol_(std::move(symbol)),
    open_(open),
    high_(high),
    low_(low),
    close_(close),
    volume_(volume)
{
    if (!session_date_.ok()) {
        throw std::invalid_argument("Bar session date is invalid");
    }

    if (symbol_.empty()) {
        throw std::invalid_argument("Bar symbol cannot be empty");
    }

    if (
        !std::isfinite(open_) ||
        !std::isfinite(high_) ||
        !std::isfinite(low_) ||
        !std::isfinite(close_)
    ) {
        throw std::invalid_argument("Bar prices must be finite");
    }

    if (
        open_ <= 0.0 ||
        high_ <= 0.0 ||
        low_ <= 0.0 ||
        close_ <= 0.0
    ) {
        throw std::invalid_argument("Bar prices must be positive");
    }

    if (high_ < low_) {
        throw std::invalid_argument("Bar high cannot be lower than low");
    }

    if (high_ < open_ || high_ < close_) {
        throw std::invalid_argument("Bar high cannot be lower than open or close");
    }

    if (low_ > open_ || low_ > close_) {
        throw std::invalid_argument("Bar low cannot be higher than open or close");
    }
}

Bar::SessionDate Bar::session_date() const noexcept {
    return session_date_;
}

const std::string& Bar::symbol() const noexcept {
    return symbol_;
}

double Bar::open() const noexcept  {
    return open_;
}

double Bar::high() const noexcept  {
    return high_;
}

double Bar::low() const noexcept  {
    return low_;
}

double Bar::close() const noexcept  {
    return close_;
}

std::uint64_t Bar::volume() const noexcept  {
    return volume_;
}

}  // namespace backtester::market
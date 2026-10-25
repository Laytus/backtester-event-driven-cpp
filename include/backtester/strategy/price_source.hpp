#pragma once

#include "backtester/market/bar.hpp"

#include <stdexcept>

namespace backtester::strategy {

enum class PriceSource {
    Open,
    High,
    Low,
    Close
};

[[nodiscard]]
constexpr bool is_valid_price_source(PriceSource source) noexcept {
    switch (source) {
        case PriceSource::Open:
        case PriceSource::High:
        case PriceSource::Low:
        case PriceSource::Close:
            return true;
    }

    return false;
}

[[nodiscard]]
inline double select_price(
    const market::Bar& bar,
    PriceSource source
) {
    switch (source) {
        case PriceSource::Open:
            return bar.open();
        case PriceSource::High:
            return bar.high();
        case PriceSource::Low:
            return bar.low();
        case PriceSource::Close:
            return bar.close();
    }

    throw std::invalid_argument(
        "Invalid price source"
    );
}

}  // namespace backtester::strategy
#pragma once

#include "backtester/market/bar.hpp"

#include <cstdint>
#include <string>
#include <variant>

namespace backtester::events {

enum class SignalDirection {
    Long,
    Short,
    Flat
};

enum class OrderSide {
    Buy,
    Sell
};

enum class OrderType {
    Market
};

class MarketEvent {
public:
    explicit MarketEvent(market::Bar bar);

    [[nodiscard]]
    const market::Bar& bar() const noexcept;

private:
    market::Bar bar_;
};

class SignalEvent {
public:
    SignalEvent(
        std::string symbol,
        SignalDirection direction,
        market::SessionDate signal_date
    );

    [[nodiscard]]
    const std::string& symbol() const noexcept;

    [[nodiscard]]
    SignalDirection direction() const noexcept;

    [[nodiscard]]
    market::SessionDate signal_date() const noexcept;

private:
    std::string symbol_;
    SignalDirection direction_;
    market::SessionDate signal_date_;
};

class OrderEvent {
public:
    OrderEvent(
        std::string symbol,
        OrderSide side,
        std::uint64_t quantity,
        OrderType order_type,
        market::SessionDate submission_date
    );

    [[nodiscard]]
    const std::string& symbol() const noexcept;

    [[nodiscard]]
    OrderSide side() const noexcept;

    [[nodiscard]]
    std::uint64_t quantity() const noexcept;

    [[nodiscard]]
    OrderType order_type() const noexcept;

    [[nodiscard]]
    market::SessionDate submission_date() const noexcept;

private:
    std::string symbol_;
    OrderSide side_;
    std::uint64_t quantity_;
    OrderType order_type_;
    market::SessionDate submission_date_;
};

class FillEvent {
public:
    FillEvent(
        std::string symbol,
        OrderSide side,
        std::uint64_t quantity,
        double fill_price,
        market::SessionDate fill_date,
        double fees
    );

    [[nodiscard]]
    const std::string& symbol() const noexcept;

    [[nodiscard]]
    OrderSide side() const noexcept;

    [[nodiscard]]
    std::uint64_t quantity() const noexcept;

    [[nodiscard]]
    double fill_price() const noexcept;

    [[nodiscard]]
    market::SessionDate fill_date() const noexcept;

    [[nodiscard]]
    double fees() const noexcept;

private:
    std::string symbol_;
    OrderSide side_;
    std::uint64_t quantity_;
    double fill_price_;
    market::SessionDate fill_date_;
    double fees_;
};

using Event = std::variant<
    MarketEvent,
    SignalEvent,
    OrderEvent,
    FillEvent
>;

}  // namespace backtester::events
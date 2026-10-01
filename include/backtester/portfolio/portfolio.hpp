#pragma once

#include "backtester/events/event.hpp"
#include "backtester/market/bar.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace backtester::portfolio {

using PositionQuantity = std::int64_t;

class PortfolioSnapshot {
public:
    PortfolioSnapshot(
        market::SessionDate session_date,
        double cash,
        PositionQuantity position_quantity,
        double market_value,
        double equity
    );

    [[nodiscard]]
    market::SessionDate session_date() const noexcept;

    [[nodiscard]]
    double cash() const noexcept;

    [[nodiscard]]
    PositionQuantity position_quantity() const noexcept;

    [[nodiscard]]
    double market_value() const noexcept;

    [[nodiscard]]
    double equity() const noexcept;

private:
    market::SessionDate session_date_;
    double cash_;
    PositionQuantity position_quantity_;
    double market_value_;
    double equity_;
};

class Portfolio {
public:
    Portfolio(
        std::string symbol,
        double initial_capital
    );

    Portfolio(const Portfolio&) = delete;
    Portfolio& operator=(const Portfolio&) = delete;

    Portfolio(Portfolio&&) = default;
    Portfolio& operator=(Portfolio&&) = default;

    [[nodiscard]]
    const std::string& symbol() const noexcept;

    [[nodiscard]]
    double initial_capital() const noexcept;

    [[nodiscard]]
    double cash() const noexcept;

    [[nodiscard]]
    PositionQuantity position_quantity() const noexcept;

    [[nodiscard]]
    std::optional<double> latest_mark_price() const noexcept;

    [[nodiscard]]
    double latest_market_value() const noexcept;

    [[nodiscard]]
    double latest_equity() const noexcept;

    [[nodiscard]]
    const std::vector<PortfolioSnapshot>& snapshots() const noexcept;

    void apply_fill(
        const events::FillEvent& fill
    );

    void mark_to_market(
        market::SessionDate session_date,
        double mark_price
    );

private:
    std::string symbol_;
    double initial_capital_;
    double cash_;
    PositionQuantity position_quantity_{0};

    std::optional<double> latest_mark_price_;
    double latest_market_value_{0.0};
    double latest_equity_;

    std::vector<PortfolioSnapshot> snapshots_;
};

}  // namespace backtester::portfolio
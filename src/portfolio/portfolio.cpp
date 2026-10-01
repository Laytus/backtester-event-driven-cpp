#include "backtester/portfolio/portfolio.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace backtester::portfolio {

PortfolioSnapshot::PortfolioSnapshot(
    market::SessionDate session_date,
    double cash,
    PositionQuantity position_quantity,
    double market_value,
    double equity
)
    : session_date_(session_date),
    cash_(cash),
    position_quantity_(position_quantity),
    market_value_(market_value),
    equity_(equity) {
        if (!session_date_.ok()) {
            throw std::invalid_argument("PortfolioSnapshot session date is invalid");
        }

        if (!std::isfinite(cash_)) {
            throw std::invalid_argument("PortfolioSnapshot cash must be finite");
        }

        if (!std::isfinite(market_value_)) {
            throw std::invalid_argument("PortfolioSnapshot market value must be finite");
        }

        if (!std::isfinite(equity_)) {
            throw std::invalid_argument("PortfolioSnapshot equity value must be finite");
        }
    }

market::SessionDate PortfolioSnapshot::session_date() const noexcept {
    return session_date_;
}

double PortfolioSnapshot::cash() const noexcept {
    return cash_;
}

PositionQuantity PortfolioSnapshot::position_quantity() const noexcept {
    return position_quantity_;
}

double PortfolioSnapshot::market_value() const noexcept {
    return market_value_;
}

double PortfolioSnapshot::equity() const noexcept {
    return equity_;
}

Portfolio::Portfolio(
    std::string symbol,
    double initial_capital
)
    : symbol_(std::move(symbol)),
    initial_capital_(initial_capital),
    cash_(initial_capital),
    position_quantity_(0),
    latest_mark_price_(std::nullopt),
    latest_market_value_(0.0),
    latest_equity_(initial_capital) {
        if (symbol_.empty()) {
            throw std::invalid_argument(
                "Portfolio symbol cannot be empty"
            );
        }

        if (!std::isfinite(initial_capital_)) {
            throw std::invalid_argument(
                "Portfolio initial capital value must be finite"
            );
        }

        if (initial_capital_ <= 0) {
            throw std::invalid_argument(
                "Portfolio initial capital value must be strictly positive"
            );
        }
    }

const std::string& Portfolio::symbol() const noexcept {
    return symbol_;
}

double Portfolio::initial_capital() const noexcept {
    return initial_capital_;
}

double Portfolio::cash() const noexcept {
    return cash_;
}

PositionQuantity Portfolio::position_quantity() const noexcept {
    return position_quantity_;
}

std::optional<double> Portfolio::latest_mark_price() const noexcept {
    return latest_mark_price_;
}

double Portfolio::latest_market_value() const noexcept {
    return latest_market_value_;
}

double Portfolio::latest_equity() const noexcept {
    return latest_equity_;
}

const std::vector<PortfolioSnapshot>& Portfolio::snapshots() const noexcept {
    return snapshots_;
}

void Portfolio::apply_fill(
    const events::FillEvent& fill
) {
    if (fill.symbol() != symbol_) {
        throw std::invalid_argument(
            "FillEvent symbol does not match Portfolio symbol"
        );
    }

    if (
        fill.quantity()
        > static_cast<std::uint64_t>(
            std::numeric_limits<PositionQuantity>::max()
        )
    ) {
        throw std::overflow_error(
            "Fill quantity exceeds PositionQuantity range"
        );
    }

    const auto quantity = static_cast<PositionQuantity>(fill.quantity());

    PositionQuantity new_position = position_quantity_;

    if (fill.side() == events::OrderSide::Buy) {
        if (
            position_quantity_
            > std::numeric_limits<PositionQuantity>::max() - quantity
        ) {
            throw std::overflow_error(
                "Portfolio position quantity overflow"
            );
        }

        new_position += quantity;
    } else if (fill.side() == events::OrderSide::Sell) {
        if (
            position_quantity_
            < std::numeric_limits<PositionQuantity>::min() + quantity
        ) {
            throw std::overflow_error(
                "Portfolio position quantity underflow"
            );
        }

        new_position -= quantity;
    }

    const double notional =
        static_cast<double>(fill.quantity()) * fill.fill_price();

    if (!std::isfinite(notional)) {
        throw std::overflow_error(
            "Portfolio fill notional is not finite"
        );
    }

    double new_cash = cash_;

    if (fill.side() == events::OrderSide::Buy) {
        new_cash = cash_ - notional - fill.fees();
    } else if (fill.side() == events::OrderSide::Sell) {
        new_cash = cash_ + notional - fill.fees();
    }

    if (!std::isfinite(new_cash)) {
        throw std::overflow_error(
            "Portfolio cash calculation is not finite"
        );
    }

    position_quantity_ = new_position;
    cash_ = new_cash;
}

void Portfolio::mark_to_market(
    market::SessionDate session_date,
    double mark_price
) {
    if (!session_date.ok()) {
        throw std::invalid_argument(
            "Portfolio session date is invalid"
        );
    }

    if (!std::isfinite(mark_price)) {
        throw std::invalid_argument(
            "Portfolio mark price must be finite"
        );
    }

    if (mark_price <= 0) {
        throw std::invalid_argument(
            "Portfolio mark price must be strictly positive"
        );
    }

    if (
        !snapshots_.empty() &&
        session_date <= snapshots_.back().session_date()
    ) {
        throw std::invalid_argument(
            "Portfolio snapshot date must be strictly increasing"
        );
    }

    const double market_value =
        static_cast<double>(position_quantity_) * mark_price;

    if (!std::isfinite(market_value)) {
        throw std::overflow_error(
            "Portfolio market value is not finite"
        );
    }

    const double equity = cash_ + market_value;

    if (!std::isfinite(equity)) {
        throw std::overflow_error(
            "Portfolio equity is not finite"
        );
    }

    PortfolioSnapshot snapshot{
        session_date,
        cash_,
        position_quantity_,
        market_value,
        equity
    };

    snapshots_.push_back(std::move(snapshot));

    latest_mark_price_ = mark_price;
    latest_market_value_ = market_value;
    latest_equity_ = equity;
}

}  // namespace backtester::portfolio
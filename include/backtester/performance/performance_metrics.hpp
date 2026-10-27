#pragma once

#include "backtester/performance/return_series.hpp"
#include "backtester/portfolio/portfolio.hpp"

#include <optional>
#include <span>

namespace backtester::performance {

struct PerformanceMetrics final {
    std::optional<double> total_return;
    std::optional<double> cagr;
    std::optional<double> annualized_volatility;
    std::optional<double> sharpe_ratio;
    std::optional<double> maximum_drawdown;
};

[[nodiscard]]
std::optional<double> calculate_total_return(
    double initial_equity,
    std::span<const portfolio::PortfolioSnapshot> snapshots
);

[[nodiscard]]
std::optional<double> calculate_cagr(
    double initial_equity,
    std::span<const portfolio::PortfolioSnapshot> snapshots,
    double annualization_factor = 252.0
);

[[nodiscard]]
std::optional<double> calculate_annualized_volatility(
    std::span<const ReturnObservation> returns,
    double annualization_factor = 252.0
);

[[nodiscard]]
std::optional<double> calculate_sharpe_ratio(
    std::span<const ReturnObservation> returns,
    double annualization_factor = 252.0,
    double risk_free_rate_annual = 0.0
);

[[nodiscard]]
std::optional<double> calculate_maximum_drawdown(
    double initial_equity,
    std::span<const portfolio::PortfolioSnapshot> snapshots
);

[[nodiscard]]
PerformanceMetrics calculate_performance_metrics(
    double initial_equity,
    std::span<const portfolio::PortfolioSnapshot> snapshots,
    std::span<const ReturnObservation> returns,
    double annualization_factor = 252.0,
    double risk_free_rate_annual = 0.0
);

}  // namespace backtester::performance
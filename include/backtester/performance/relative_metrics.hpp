#pragma once

#include "backtester/performance/active_return_series.hpp"
#include "backtester/performance/performance_metrics.hpp"

#include <optional>
#include <span>

namespace backtester::performance {

struct RelativePerformanceMetrics final {
    std::optional<double> excess_total_return;
    std::optional<double> annualized_return_difference;
    std::optional<double> tracking_error;
    std::optional<double> information_ratio;
    std::optional<double> correlation;
    std::optional<double> beta;
    std::optional<double> jensen_alpha;
};

// Difference between strategy and benchmark total returns.
[[nodiscard]]
std::optional<double> calculate_excess_total_return(
    std::optional<double> strategy_total_return,
    std::optional<double> benchmark_total_return
);

// Difference between separately calculated strategy and benchmark CAGRs.
[[nodiscard]]
std::optional<double> calculate_annualized_return_difference(
    std::optional<double> strategy_cagr,
    std::optional<double> benchmark_cagr
);

// Annualized sample standard deviation of active simple returns.
[[nodiscard]]
std::optional<double> calculate_tracking_error(
    std::span<const ActiveReturnObservation> active_returns,
    double annualized_factor = 252.0
);

// Annualized mean active return divided by its sample deviation.
[[nodiscard]]
std::optional<double> calculate_information_ratio(
    std::span<const ActiveReturnObservation> active_returns,
    double annualization_factor = 252.0
);

// Pearson correlation of paired strategy and benchmark simple returns.
[[nodiscard]]
std::optional<double> calculate_correlation(
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns
);

// Sensitivity of strategy returns to benchmark returns.
[[nodiscard]]
std::optional<double> calculate_beta(
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns
);

// Annualized arithmetic Jensen's alpha relative to the benchmark.
[[nodiscard]]
std::optional<double> calculate_jensen_alpha(
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns,
    double annualization_factor = 252.0,
    double risk_free_rate_annual = 0.0
);

// Compute all relative metrics from independetly calculated absolute metrics
// and aligned dated simple-return histories.
[[nodiscard]]
RelativePerformanceMetrics calculate_relative_performance_metrics(
    const PerformanceMetrics& strategy_metrics,
    const PerformanceMetrics& benchmark_metrics,
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns,
    double annualization_factor = 252.0,
    double risk_free_rate_annual = 0.0
);

}  // namespace backtester::performance
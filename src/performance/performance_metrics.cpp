#include "backtester/performance/performance_metrics.hpp"

#include "backtester/statistics/statistics.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

namespace backtester::performance {

namespace {

// -----------------------------------------------------------
// Shared validation
// -----------------------------------------------------------

void validate_initial_equity(double initial_equity) {
    if (!std::isfinite(initial_equity) || initial_equity <= 0.0) {
        throw std::invalid_argument(
            "Performance requires finite and positive initial equity"
        );
    }
}

void validate_annualization(double annualization_factor) {
    if (
        !std::isfinite(annualization_factor) ||
        annualization_factor <= 0.0
    ) {
        throw std::invalid_argument(
            "Annualization factor must be finite and positive"
        );
    }
}

void validate_risk_free_rate(double risk_free_rate_annual) {
    if (
        !std::isfinite(risk_free_rate_annual) ||
        risk_free_rate_annual <= -1.0
    ) {
        throw std::invalid_argument(
            "Annual risk-free rate must be finite and greater than -1"
        );
    }
}

double require_finite(double value) {
    if (!std::isfinite(value)) {
        throw std::overflow_error(
            "Performance calculation produced a nonfinite result"
        );
    }

    return value;
}

// -----------------------------------------------------------
// Equity history analysis
// -----------------------------------------------------------

struct EquitySummary final {
    bool has_observations{false};
    double final_equity{0.0};
    bool growth_path_valid{true};
    double maximum_drawdown{0.0};
};

EquitySummary summarize_equity(
    double initial_equity,
    std::span<const portfolio::PortfolioSnapshot> snapshots,
    bool track_growth,
    bool track_drawdown
) {
    validate_initial_equity(initial_equity);

    EquitySummary summary;
    double peak = initial_equity;

    std::optional<market::SessionDate> previous_date;

    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        const auto& snapshot = snapshots[i];
        const auto date = snapshot.session_date();
        const double equity = snapshot.equity();

        if (!date.ok()) {
            throw std::invalid_argument(
                "Performance requires valid session dates"
            );
        }

        if (previous_date && date <= *previous_date) {
            throw std::invalid_argument(
                "Performance requires strictly increasing session dates"
            );
        }

        if (!std::isfinite(equity)) {
            throw std::invalid_argument(
                "Performance requires finite snapshot equity"
            );
        }

        if (
            track_growth &&
            i + 1 < snapshots.size() &&
            equity <= 0.0
        ) {
            summary.growth_path_valid = false;
        }

        if (track_drawdown) {
            peak = std::max(peak, equity);

            const double drawdown = equity >= 0.0
                ? (peak - equity) / peak
                : 1.0 - equity / peak;

            summary.maximum_drawdown = std::max(
                summary.maximum_drawdown,
                require_finite(drawdown)
            );
        }

        summary.final_equity = equity;
        summary.has_observations = true;
        previous_date = date;
    }

    return summary;
}

// -----------------------------------------------------------
// Equity-based metric calculations
// -----------------------------------------------------------

std::optional<double> total_return_from(
    double initial_equity,
    const EquitySummary& summary
) {
    if (!summary.has_observations) {
        return std::nullopt;
    }

    const double final_equity = summary.final_equity;

    const double result = final_equity >= 0.0
        ? (final_equity - initial_equity) / initial_equity
        : final_equity / initial_equity - 1.0;

    return require_finite(result);
}

std::optional<double> cagr_from(
    double initial_equity,
    const EquitySummary& summary,
    std::size_t observation_count,
    double annualization_factor
) {
    if (
        !summary.has_observations ||
        !summary.growth_path_valid ||
        summary.final_equity < 0.0
    ) {
        return std::nullopt;
    }

    if (summary.final_equity == 0.0) {
        return -1.0;
    }

    const double final_equity = summary.final_equity;

    const double relative_change =
        (final_equity - initial_equity) / initial_equity;

    const double log_growth =
        std::isfinite(relative_change) && relative_change > -1.0
            ? std::log1p(relative_change)
            : std::log(final_equity) - std::log(initial_equity);

    const double exponent =
        (annualization_factor /
        static_cast<double>(observation_count)) * log_growth;

    return require_finite(std::expm1(exponent));
}

// -----------------------------------------------------------
// Return-series statistics
// -----------------------------------------------------------

std::optional<statistics::SampleMoments> summarize_returns(
    std::span<const ReturnObservation> returns
) {
    std::optional<market::SessionDate> previous_date;
    bool complete = true;

    for (const auto& observation : returns) {
        const auto date = observation.session_date;

        if (!date.ok()) {
            throw std::invalid_argument(
                "Performance requires valid return dates"
            );
        }

        if (previous_date && date <= *previous_date) {
            throw std::invalid_argument(
                "Performance requires strictly increasing return dates"
            );
        }

        if (!observation.simple_return) {
            complete = false;
        } else if (!std::isfinite(*observation.simple_return)) {
            throw std::invalid_argument(
                "Performance requires finite simple returns"
            );
        }

        previous_date = date;
    }

    if (!complete || returns.size() < 2) {
        return std::nullopt;
    }

    std::vector<double> values;
    values.reserve(returns.size());

    for (const auto& observation : returns) {
        values.push_back(*observation.simple_return);
    }

    return statistics::sample_moments(
        std::span<const double>{values}
    );
}

// -----------------------------------------------------------
// Return-based metric calculations
// -----------------------------------------------------------

std::optional<double> annualized_volatility_from(
    const std::optional<statistics::SampleMoments>& moments,
    double annualization_factor
) {
    if (!moments) {
        return std::nullopt;
    }

    const double result =
        moments->standard_deviation * std::sqrt(annualization_factor);

    return require_finite(result);
}

std::optional<double> sharpe_ratio_from(
    const std::optional<statistics::SampleMoments>& moments,
    double annualization_factor,
    double risk_free_rate_annual
) {
    if (!moments || moments->standard_deviation == 0.0) {
        return std::nullopt;
    }

    const double periodic_risk_free_rate =
        risk_free_rate_annual == 0.0
            ? 0.0
            : require_finite(std::expm1(
                std::log1p(risk_free_rate_annual) /
                annualization_factor
            ));

    const double excess_mean =
            require_finite(moments->mean - periodic_risk_free_rate);

    const double result =
        std::sqrt(annualization_factor) *
        (excess_mean / moments->standard_deviation);

    return require_finite(result);
}

// -----------------------------------------------------------
// Aggregate input alignment
// -----------------------------------------------------------

void validate_alignment(
    std::span<const portfolio::PortfolioSnapshot> snapshots,
    std::span<const ReturnObservation> returns
) {
    if (snapshots.size() != returns.size()) {
        throw std::invalid_argument(
            "Performance requires matching snapshot and return counts"
        );
    }

    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        if (snapshots[i].session_date() != returns[i].session_date) {
            throw std::invalid_argument(
                "Performance requires matching snapshot and return dates"
            );
        }
    }
}

}  // namespace

std::optional<double> calculate_total_return(
    double initial_equity,
    std::span<const portfolio::PortfolioSnapshot> snapshots
) {
    const auto summary = summarize_equity(
        initial_equity, snapshots, false, false
    );

    return total_return_from(initial_equity, summary);
}

std::optional<double> calculate_cagr(
    double initial_equity,
    std::span<const portfolio::PortfolioSnapshot> snapshots,
    double annualization_factor
) {
    validate_annualization(annualization_factor);

    const auto summary = summarize_equity(
        initial_equity, snapshots, true, false
    );

    return cagr_from(
        initial_equity,
        summary,
        snapshots.size(),
        annualization_factor
    );
}

std::optional<double> calculate_annualized_volatility(
    std::span<const ReturnObservation> returns,
    double annualization_factor
) {
    validate_annualization(annualization_factor);

    const auto moments = summarize_returns(returns);

    return annualized_volatility_from(
        moments, annualization_factor
    );
}

std::optional<double> calculate_sharpe_ratio(
    std::span<const ReturnObservation> returns,
    double annualization_factor,
    double risk_free_rate_annual
) {
    validate_annualization(annualization_factor);
    validate_risk_free_rate(risk_free_rate_annual);

    const auto moments = summarize_returns(returns);

    return sharpe_ratio_from(
        moments,
        annualization_factor,
        risk_free_rate_annual
    );
}

std::optional<double> calculate_maximum_drawdown(
    double initial_equity,
    std::span<const portfolio::PortfolioSnapshot> snapshots
) {
    const auto summary = summarize_equity(
        initial_equity, snapshots, false, true
    );

    if (!summary.has_observations) {
        return std::nullopt;
    }

    return summary.maximum_drawdown;
}

PerformanceMetrics calculate_performance_metrics(
    double initial_equity,
    std::span<const portfolio::PortfolioSnapshot> snapshots,
    std::span<const ReturnObservation> returns,
    double annualization_factor,
    double risk_free_rate_annual
) {
    validate_initial_equity(initial_equity);
    validate_annualization(annualization_factor);
    validate_risk_free_rate(risk_free_rate_annual);
    validate_alignment(snapshots, returns);

    const auto equity_summary = summarize_equity(
        initial_equity, snapshots, true, true
    );

    const auto moments = summarize_returns(returns);

    return PerformanceMetrics{
        total_return_from(initial_equity, equity_summary),
        cagr_from(
            initial_equity,
            equity_summary,
            snapshots.size(),
            annualization_factor
        ),
        annualized_volatility_from(
            moments, annualization_factor
        ),
        sharpe_ratio_from(
            moments,
            annualization_factor,
            risk_free_rate_annual
        ),
        equity_summary.has_observations
            ? std::optional<double>{equity_summary.maximum_drawdown}
            : std::nullopt
    };
}

}  // namespace backtester::performance
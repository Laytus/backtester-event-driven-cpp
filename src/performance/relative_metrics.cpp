#include "backtester/performance/relative_metrics.hpp"

#include "backtester/statistics/statistics.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace backtester::performance {

namespace {

void validate_annualization_factor(double annualization_factor) {
    if (
        !std::isfinite(annualization_factor) ||
        annualization_factor <= 0.0
    ) {
        throw std::invalid_argument(
            "Annualization factor must be finite and positive"
        );
    }
}

void validate_risk_free_rate(double risk_free_rate) {
    if (
        !std::isfinite(risk_free_rate) ||
        risk_free_rate <= -1.0
    ) {
        throw std::invalid_argument(
            "Annual risk-free rate must be finite and greater than -1"
        );
    }
}

void validate_optional_metric(std::optional<double> value) {
    if (value && !std::isfinite(*value)) {
        throw std::invalid_argument(
            "Relative performance inputs must be finite"
        );
    }
}

double checked_result(double value) {
    if (!std::isfinite(value)) {
        throw std::overflow_error(
            "Relative performance calculation produces a nonfinite result"
        );
    }

    return value;
}

// Validate a dated active-return series and compute sample moments only when
// every active return is available.
std::optional<statistics::SampleMoments> active_sample_moments(
    std::span<const ActiveReturnObservation> active_returns
) {
    std::vector<double> values;
    values.reserve(active_returns.size());

    std::optional<market::SessionDate> previous_date;
    bool complete = true;

    for (const auto& observation : active_returns) {
        if (!observation.session_date.ok()) {
            throw std::invalid_argument(
                "Active returns require valid session dates"
            );
        }

        if (
            previous_date &&
            observation.session_date <= *previous_date
        ) {
            throw std::invalid_argument(
                "Active returns require strictly increasing dates"
            );
        }

        if (observation.active_return) {
            if (!std::isfinite(*observation.active_return)) {
                throw std::invalid_argument(
                    "Active returns must be finite"
                );
            }

            values.push_back(*observation.active_return);
        } else {
            complete = false;
        }

        previous_date = observation.session_date;
    }

    if (!complete || values.size() < 2) {
        return std::nullopt;
    }

    return statistics::sample_moments(
        std::span<const double>{values}
    );
}

std::optional<statistics::SampleBivariateMoments>
paired_sample_moments(
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns,
    std::span<const ActiveReturnObservation> active_returns
) {
    if (active_returns.size() < 2) {
        return std::nullopt;
    }

    std::vector<double> strategy_values;
    std::vector<double> benchmark_values;

    strategy_values.reserve(active_returns.size());
    benchmark_values.reserve(active_returns.size());

    for (std::size_t i = 0; i < active_returns.size(); ++i) {
        if (!active_returns[i].active_return) {
            return std::nullopt;
        }

        strategy_values.push_back(
            *strategy_returns[i].simple_return
        );

        benchmark_values.push_back(
            *benchmark_returns[i].simple_return
        );
    }

    return statistics::sample_bivariate_moments(
        std::span<const double>{strategy_values},
        std::span<const double>{benchmark_values}
    );
}

std::optional<statistics::SampleBivariateMoments>
validated_paired_sample_moments(
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns
) {
    const auto active_returns = build_active_return_series(
        strategy_returns,
        benchmark_returns
    );

    return paired_sample_moments(
        strategy_returns,
        benchmark_returns,
        std::span<const ActiveReturnObservation>{active_returns}
    );
}

std::optional<double> beta_from_moments(
    const statistics::SampleBivariateMoments& moments
) {
    const double benchmark_sd = moments.standard_deviation_y;

    if (benchmark_sd == 0.0) {
        return std::nullopt;
    }

    if (moments.covariance == 0.0) {
        if (
            moments.standard_deviation_x == 0.0 ||
            !moments.correlation ||
            *moments.correlation == 0.0
        ) {
            return 0.0;
        }

        int correlation_exponent = 0;
        int strategy_sd_exponent = 0;
        int benchmark_sd_exponent = 0;

        const double correlation_mantissa = std::frexp(
            *moments.correlation,
            &correlation_exponent
        );

        const double strategy_sd_mantissa = std::frexp(
            moments.standard_deviation_x,
            &strategy_sd_exponent
        );

        const double benchmark_sd_mantissa = std::frexp(
            benchmark_sd,
            &benchmark_sd_exponent
        );

        return checked_result(std::ldexp(
            correlation_mantissa *
                (strategy_sd_mantissa / benchmark_sd_mantissa),
            correlation_exponent +
                strategy_sd_exponent - benchmark_sd_exponent
        ));
    }

    int covariance_exponent = 0;
    int sd_exponent = 0;

    const double covariance_mantissa = std::frexp(
        moments.covariance,
        &covariance_exponent
    );

    const double sd_mantissa = std::frexp(
        benchmark_sd,
        &sd_exponent
    );

    const double normalized_ratio =
        covariance_mantissa / (sd_mantissa * sd_mantissa);

    const double beta = std::ldexp(
        normalized_ratio,
        covariance_exponent - 2 * sd_exponent
    );

    return checked_result(beta);
}

double tracking_error_from_moments(
    const statistics::SampleMoments& moments,
    double annualization_factor
) {
    return checked_result(
        moments.standard_deviation * std::sqrt(annualization_factor)
    );
}

std::optional<double> information_ratio_from_moments(
    const statistics::SampleMoments& moments,
    double annualization_factor
) {
    if (moments.standard_deviation == 0.0) {
        return std::nullopt;
    }

    return checked_result(
        std::sqrt(annualization_factor) *
        (moments.mean / moments.standard_deviation)
    );
}

double periodic_risk_free_rate(
    double annualization_factor,
    double risk_free_rate_annual
) {
    const double exponent =
        std::log1p(risk_free_rate_annual) / annualization_factor;

    return checked_result(std::expm1(exponent));
}

double jensen_alpha_from_moments(
    const statistics::SampleBivariateMoments& moments,
    double beta,
    double annualization_factor,
    double risk_free_rate_annual
) {
    const double risk_free_periodic = periodic_risk_free_rate(
        annualization_factor,
        risk_free_rate_annual
    );

    const double strategy_excess = checked_result(
        moments.mean_x - risk_free_periodic
    );

    const double benchmark_excess = checked_result(
        moments.mean_y - risk_free_periodic
    );

    const double periodic_alpha = checked_result(
        std::fma(-beta, benchmark_excess, strategy_excess)
    );

    return checked_result(
        annualization_factor * periodic_alpha
    );
}

}  // namespace

std::optional<double> calculate_excess_total_return(
    std::optional<double> strategy_total_return,
    std::optional<double> benchmark_total_return
) {
    validate_optional_metric(strategy_total_return);
    validate_optional_metric(benchmark_total_return);

    if (!strategy_total_return || !benchmark_total_return) {
        return std::nullopt;
    }

    return checked_result(
        *strategy_total_return - *benchmark_total_return
    );
}

std::optional<double> calculate_annualized_return_difference(
    std::optional<double> strategy_cagr,
    std::optional<double> benchmark_cagr
) {
    validate_optional_metric(strategy_cagr);
    validate_optional_metric(benchmark_cagr);

    if (!strategy_cagr || !benchmark_cagr) {
        return std::nullopt;
    }

    return checked_result(*strategy_cagr - *benchmark_cagr);
}

std::optional<double> calculate_tracking_error(
    std::span<const ActiveReturnObservation> active_returns,
    double annualization_factor
) {
    validate_annualization_factor(annualization_factor);

    const auto moments = active_sample_moments(active_returns);

    if (!moments) {
        return std::nullopt;
    }

    return tracking_error_from_moments(
        *moments,
        annualization_factor
    );
}

std::optional<double> calculate_information_ratio(
    std::span<const ActiveReturnObservation> active_returns,
    double annualization_factor
) {
    validate_annualization_factor(annualization_factor);

    const auto moments = active_sample_moments(active_returns);

    if (!moments) {
        return std::nullopt;
    }

    return information_ratio_from_moments(
        *moments,
        annualization_factor
    );
}

std::optional<double> calculate_correlation(
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns
) {
    const auto moments = validated_paired_sample_moments(
        strategy_returns,
        benchmark_returns
    );

    if (!moments) {
        return std::nullopt;
    }

    return moments->correlation;
}

std::optional<double> calculate_beta(
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns
) {
    const auto moments = validated_paired_sample_moments(
        strategy_returns,
        benchmark_returns
    );

    if (!moments) {
        return std::nullopt;
    }

    return beta_from_moments(*moments);
}

std::optional<double> calculate_jensen_alpha(
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns,
    double annualization_factor,
    double risk_free_rate_annual
) {
    validate_annualization_factor(annualization_factor);
    validate_risk_free_rate(risk_free_rate_annual);

    const auto moments = validated_paired_sample_moments(
        strategy_returns,
        benchmark_returns
    );

    if (!moments) {
        return std::nullopt;
    }

    const auto beta = beta_from_moments(*moments);

    if (!beta) {
        return std::nullopt;
    }

    return jensen_alpha_from_moments(
        *moments,
        *beta,
        annualization_factor,
        risk_free_rate_annual
    );
}

RelativePerformanceMetrics calculate_relative_performance_metrics(
    const PerformanceMetrics& strategy_metrics,
    const PerformanceMetrics& benchmark_metrics,
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns,
    double annualization_factor,
    double risk_free_rate_annual
) {
    validate_annualization_factor(annualization_factor);
    validate_risk_free_rate(risk_free_rate_annual);

    // Validate dates, alignment, and finite simple returns.
    const auto active_returns = build_active_return_series(
        strategy_returns,
        benchmark_returns
    );

    RelativePerformanceMetrics result;

    result.excess_total_return = calculate_excess_total_return(
        strategy_metrics.total_return,
        benchmark_metrics.total_return
    );

    result.annualized_return_difference =
        calculate_annualized_return_difference(
            strategy_metrics.cagr,
            benchmark_metrics.cagr
        );

    // Compute active sample moments once for both active-risk metrics.
    const auto active_moments = active_sample_moments(
        std::span<const ActiveReturnObservation>{active_returns}
    );

    if (active_moments) {
        result.tracking_error = tracking_error_from_moments(
            *active_moments,
            annualization_factor
        );

        result.information_ratio = information_ratio_from_moments(
            *active_moments,
            annualization_factor
        );
    }

    // Compute bivariate sample moments once for correlation, beta, and
    // Jensen's alpha.
    const auto paired_moments = paired_sample_moments(
        strategy_returns,
        benchmark_returns,
        std::span<const ActiveReturnObservation>{active_returns}
    );

    if (paired_moments) {
        result.correlation = paired_moments->correlation;
        result.beta = beta_from_moments(*paired_moments);

        if (result.beta) {
            result.jensen_alpha = jensen_alpha_from_moments(
                *paired_moments,
                *result.beta,
                annualization_factor,
                risk_free_rate_annual
            );
        }
    }

    return result;
}

}  // namespace backtester::performance
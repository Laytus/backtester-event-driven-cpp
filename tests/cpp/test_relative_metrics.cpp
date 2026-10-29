#include "backtester/performance/relative_metrics.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

using backtester::market::SessionDate;
using backtester::performance::ActiveReturnObservation;
using backtester::performance::ActiveReturnSeries;
using backtester::performance::PerformanceMetrics;
using backtester::performance::RelativePerformanceMetrics;
using backtester::performance::ReturnSeries;
using backtester::performance::build_active_return_series;
using backtester::performance::calculate_annualized_return_difference;
using backtester::performance::calculate_beta;
using backtester::performance::calculate_correlation;
using backtester::performance::calculate_excess_total_return;
using backtester::performance::calculate_information_ratio;
using backtester::performance::calculate_jensen_alpha;
using backtester::performance::calculate_relative_performance_metrics;
using backtester::performance::calculate_tracking_error;

SessionDate session_date(int offset) {
    const std::chrono::sys_days first{
        std::chrono::year{2026} / std::chrono::September / 14
    };
    return SessionDate{first + std::chrono::days{offset}};
}

ReturnSeries make_returns(
    std::initializer_list<std::optional<double>> simple_returns
) {
    ReturnSeries observations;
    observations.reserve(simple_returns.size());
    int offset = 0;
    for (const auto& value : simple_returns) {
        observations.push_back({session_date(offset++), value, std::nullopt});
    }
    return observations;
}

ActiveReturnSeries make_active_returns(
    std::initializer_list<std::optional<double>> active_returns
) {
    ActiveReturnSeries observations;
    observations.reserve(active_returns.size());
    int offset = 0;
    for (const auto& value : active_returns) {
        observations.push_back({session_date(offset++), value});
    }
    return observations;
}

PerformanceMetrics make_absolute_metrics(
    std::optional<double> total_return = std::nullopt,
    std::optional<double> cagr = std::nullopt
) {
    PerformanceMetrics metrics{};
    metrics.total_return = total_return;
    metrics.cagr = cagr;
    return metrics;
}

void require_close(
    double actual,
    double expected,
    double absolute_tolerance = 1e-12,
    double relative_tolerance = 1e-10
) {
    REQUIRE(std::isfinite(actual));
    REQUIRE(std::isfinite(expected));
    const double scale = std::max(std::abs(actual), std::abs(expected));
    REQUIRE(
        std::abs(actual - expected) <=
        std::max(absolute_tolerance, relative_tolerance * scale)
    );
}

void require_present(
    const std::optional<double>& actual,
    double expected,
    double absolute_tolerance = 1e-12,
    double relative_tolerance = 1e-10
) {
    REQUIRE(actual.has_value());
    require_close(*actual, expected, absolute_tolerance, relative_tolerance);
}

void require_absent(const std::optional<double>& actual) {
    REQUIRE_FALSE(actual.has_value());
}

void require_same_optional(
    const std::optional<double>& actual,
    const std::optional<double>& expected
) {
    REQUIRE(actual.has_value() == expected.has_value());
    if (expected) {
        require_close(*actual, *expected);
    }
}

void require_same_metrics(
    const RelativePerformanceMetrics& actual,
    const RelativePerformanceMetrics& expected
) {
    require_same_optional(actual.excess_total_return, expected.excess_total_return);
    require_same_optional(
        actual.annualized_return_difference,
        expected.annualized_return_difference
    );
    require_same_optional(actual.tracking_error, expected.tracking_error);
    require_same_optional(actual.information_ratio, expected.information_ratio);
    require_same_optional(actual.correlation, expected.correlation);
    require_same_optional(actual.beta, expected.beta);
    require_same_optional(actual.jensen_alpha, expected.jensen_alpha);
}

void require_no_statistical_metrics(const RelativePerformanceMetrics& metrics) {
    require_absent(metrics.tracking_error);
    require_absent(metrics.information_ratio);
    require_absent(metrics.correlation);
    require_absent(metrics.beta);
    require_absent(metrics.jensen_alpha);
}

// Oracle D: n = 5, A = 4, annual effective risk-free rate = 0.04.
ReturnSeries oracle_d_strategy() {
    return make_returns({0.00, 0.01, 0.01, 0.02, 0.01});
}

ReturnSeries oracle_d_benchmark() {
    return make_returns({-0.02, -0.01, 0.00, 0.01, 0.02});
}

PerformanceMetrics oracle_d_strategy_metrics() {
    return make_absolute_metrics(
        0.05090702,
        std::pow(1.05090702, 4.0 / 5.0) - 1.0
    );
}

PerformanceMetrics oracle_d_benchmark_metrics() {
    return make_absolute_metrics(
        -0.00049996,
        std::pow(0.99950004, 4.0 / 5.0) - 1.0
    );
}

}  // namespace

// -----------------------------------------------------------------------------
// Endpoint differences: decimal percentage-point units, not ratios.
// -----------------------------------------------------------------------------

TEST_CASE("RelativeMetrics excess total return is an arithmetic difference") {
    require_present(calculate_excess_total_return(0.12, -0.03), 0.15);
    require_present(calculate_excess_total_return(-0.20, -0.10), -0.10);
    require_present(calculate_excess_total_return(0.0, 0.0), 0.0);
}

TEST_CASE("RelativeMetrics excess total return preserves missing inputs") {
    require_absent(calculate_excess_total_return(std::nullopt, 0.1));
    require_absent(calculate_excess_total_return(0.1, std::nullopt));
    require_absent(calculate_excess_total_return(std::nullopt, std::nullopt));
}

TEST_CASE("RelativeMetrics annualized return difference uses separate CAGRs") {
    require_present(calculate_annualized_return_difference(0.10, 0.04), 0.06);
    require_present(calculate_annualized_return_difference(-0.03, 0.02), -0.05);
    require_present(calculate_annualized_return_difference(0.0, 0.0), 0.0);
}

TEST_CASE("RelativeMetrics annualized return difference preserves missing CAGRs") {
    require_absent(calculate_annualized_return_difference(std::nullopt, 0.02));
    require_absent(calculate_annualized_return_difference(0.02, std::nullopt));
    require_absent(calculate_annualized_return_difference(std::nullopt, std::nullopt));
}

TEST_CASE("RelativeMetrics endpoint differences reject nonfinite inputs") {
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const double invalid : {nan, infinity, -infinity}) {
        REQUIRE_THROWS_AS(
            calculate_excess_total_return(invalid, 0.0), std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            calculate_excess_total_return(std::nullopt, invalid),
            std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            calculate_annualized_return_difference(invalid, 0.0),
            std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            calculate_annualized_return_difference(std::nullopt, invalid),
            std::invalid_argument
        );
    }
}

TEST_CASE("RelativeMetrics endpoint difference overflow is explicitly rejected") {
    const double max = std::numeric_limits<double>::max();
    REQUIRE_THROWS_AS(
        calculate_excess_total_return(max, -max), std::overflow_error
    );
    REQUIRE_THROWS_AS(
        calculate_annualized_return_difference(-max, max),
        std::overflow_error
    );
}

// -----------------------------------------------------------------------------
// Active return metrics: sample standard deviation and arithmetic mean.
// -----------------------------------------------------------------------------

TEST_CASE("RelativeMetrics tracking error matches approved Oracle D") {
    const auto active = build_active_return_series(
        oracle_d_strategy(), oracle_d_benchmark()
    );
    require_present(
        calculate_tracking_error(active, 4.0),
        0.024494897427832
    );
}

TEST_CASE("RelativeMetrics information ratio matches approved Oracle D") {
    const auto active = build_active_return_series(
        oracle_d_strategy(), oracle_d_benchmark()
    );
    require_present(
        calculate_information_ratio(active, 4.0),
        1.632993161855452
    );
}

TEST_CASE("RelativeMetrics tracking error uses sample rather than population SD") {
    const auto active = make_active_returns({0.0, 0.20});
    // Sample SD = sqrt(0.02), not the population SD of 0.1.
    require_present(calculate_tracking_error(active, 1.0), std::sqrt(0.02));
}

TEST_CASE("RelativeMetrics information ratio uses arithmetic mean active return") {
    const auto active = make_active_returns({0.0, 0.20});
    // Arithmetic mean = 0.1, sample SD = sqrt(0.02).
    require_present(
        calculate_information_ratio(active, 1.0),
        0.1 / std::sqrt(0.02)
    );
}

TEST_CASE("RelativeMetrics annualization scales TE and IR by square root") {
    const auto active = make_active_returns({-0.10, 0.00, 0.20, 0.10});
    const auto te_1 = calculate_tracking_error(active, 1.0);
    const auto te_9 = calculate_tracking_error(active, 9.0);
    const auto ir_1 = calculate_information_ratio(active, 1.0);
    const auto ir_9 = calculate_information_ratio(active, 9.0);
    REQUIRE(te_1.has_value());
    REQUIRE(ir_1.has_value());
    require_present(te_9, 3.0 * *te_1);
    require_present(ir_9, 3.0 * *ir_1);
}

TEST_CASE("RelativeMetrics constant zero active returns have zero TE and absent IR") {
    const auto active = make_active_returns({0.0, 0.0, 0.0, 0.0});
    require_present(calculate_tracking_error(active), 0.0);
    require_absent(calculate_information_ratio(active));
}

TEST_CASE("RelativeMetrics constant positive active returns have zero TE and absent IR") {
    // Binary fractions give exactly constant differences in double.
    const auto active = make_active_returns({0.125, 0.125, 0.125});
    require_present(calculate_tracking_error(active, 4.0), 0.0);
    require_absent(calculate_information_ratio(active, 4.0));
}

TEST_CASE("RelativeMetrics negative mean produces negative information ratio") {
    const auto active = make_active_returns({-0.10, -0.20, 0.00});
    const auto ir = calculate_information_ratio(active, 4.0);
    REQUIRE(ir.has_value());
    REQUIRE(*ir < 0.0);
}

TEST_CASE("RelativeMetrics empty and singleton active histories lack sample metrics") {
    for (const auto& active : {
        make_active_returns({}),
        make_active_returns({0.02}),
        make_active_returns({std::nullopt})
    }) {
        require_absent(calculate_tracking_error(active));
        require_absent(calculate_information_ratio(active));
    }
}

TEST_CASE("RelativeMetrics missing active return invalidates full-series TE and IR") {
    const auto active = make_active_returns({0.01, std::nullopt, -0.02, 0.03});
    require_absent(calculate_tracking_error(active));
    require_absent(calculate_information_ratio(active));
}

TEST_CASE("RelativeMetrics validates dates even if all active returns are missing") {
    auto duplicate = make_active_returns({std::nullopt, std::nullopt});
    duplicate[1].session_date = duplicate[0].session_date;
    REQUIRE_THROWS_AS(calculate_tracking_error(duplicate), std::invalid_argument);
    REQUIRE_THROWS_AS(calculate_information_ratio(duplicate), std::invalid_argument);

    auto reversed = make_active_returns({0.0, std::nullopt});
    reversed[1].session_date = session_date(-1);
    REQUIRE_THROWS_AS(calculate_tracking_error(reversed), std::invalid_argument);
    REQUIRE_THROWS_AS(calculate_information_ratio(reversed), std::invalid_argument);
}

TEST_CASE("RelativeMetrics rejects invalid active return dates") {
    auto active = make_active_returns({0.0, 0.1});
    active[0].session_date = SessionDate{
        std::chrono::year{2026}, std::chrono::month{2}, std::chrono::day{30}
    };
    REQUIRE_THROWS_AS(calculate_tracking_error(active), std::invalid_argument);
    REQUIRE_THROWS_AS(calculate_information_ratio(active), std::invalid_argument);
}

TEST_CASE("RelativeMetrics rejects nonfinite active values even with missing neighbors") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    for (const double invalid : {nan, infinity, -infinity}) {
        const auto active = make_active_returns({std::nullopt, invalid, 0.0});
        REQUIRE_THROWS_AS(calculate_tracking_error(active), std::invalid_argument);
        REQUIRE_THROWS_AS(calculate_information_ratio(active), std::invalid_argument);
    }
}

TEST_CASE("RelativeMetrics TE and IR reject invalid annualization factors") {
    const auto active = make_active_returns({0.0, 0.1});
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    for (const double invalid : {0.0, -1.0, nan, infinity, -infinity}) {
        REQUIRE_THROWS_AS(
            calculate_tracking_error(active, invalid), std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            calculate_information_ratio(active, invalid), std::invalid_argument
        );
    }
}

TEST_CASE("RelativeMetrics tracking error rejects annualization overflow") {
    const auto active = make_active_returns({-1e300, 1e300});
    REQUIRE_THROWS_AS(
        calculate_tracking_error(active, 1e100), std::overflow_error
    );
}

TEST_CASE("RelativeMetrics permits finite large and tiny tracking errors") {
    const auto large = make_active_returns({-1e200, 1e200});
    require_present(
        calculate_tracking_error(large, 1.0),
        std::sqrt(2.0) * 1e200,
        0.0, 1e-12
    );

    const auto tiny = make_active_returns({-1e-200, 1e-200});
    require_present(
        calculate_tracking_error(tiny, 1.0),
        std::sqrt(2.0) * 1e-200,
        0.0, 1e-12
    );
}

// -----------------------------------------------------------------------------
// Paired series: Pearson correlation, beta, and Jensen's alpha.
// -----------------------------------------------------------------------------

TEST_CASE("RelativeMetrics correlation matches approved Oracle D") {
    require_present(
        calculate_correlation(oracle_d_strategy(), oracle_d_benchmark()),
        0.670820393249937
    );
}

TEST_CASE("RelativeMetrics beta matches approved Oracle D") {
    require_present(
        calculate_beta(oracle_d_strategy(), oracle_d_benchmark()),
        0.3
    );
}

TEST_CASE("RelativeMetrics Jensen alpha matches approved Oracle D") {
    require_present(
        calculate_jensen_alpha(
            oracle_d_strategy(), oracle_d_benchmark(), 4.0, 0.04
        ),
        0.012410461662887
    );
}

TEST_CASE("RelativeMetrics identical variable returns have unit beta and correlation") {
    const auto returns = make_returns({-0.125, 0.0, 0.25});
    require_present(calculate_correlation(returns, returns), 1.0);
    require_present(calculate_beta(returns, returns), 1.0);
    require_present(calculate_jensen_alpha(returns, returns, 4.0, 0.04), 0.0);
}

TEST_CASE("RelativeMetrics negative affine relationship has negative beta") {
    const auto strategy = make_returns({2.0, 0.0, -2.0});
    const auto benchmark = make_returns({-1.0, 0.0, 1.0});
    require_present(calculate_correlation(strategy, benchmark), -1.0);
    require_present(calculate_beta(strategy, benchmark), -2.0);
    require_present(calculate_jensen_alpha(strategy, benchmark, 1.0, 0.0), 0.0);
}

TEST_CASE("RelativeMetrics orthogonal centered series have zero beta and correlation") {
    const auto strategy = make_returns({-1.0, 0.0, 1.0});
    const auto benchmark = make_returns({1.0, -2.0, 1.0});
    require_present(calculate_correlation(strategy, benchmark), 0.0);
    require_present(calculate_beta(strategy, benchmark), 0.0);
    require_present(calculate_jensen_alpha(strategy, benchmark, 4.0, 0.0), 0.0);
}

TEST_CASE("RelativeMetrics constant strategy has absent correlation but zero beta") {
    const auto strategy = make_returns({0.0, 0.0, 0.0});
    const auto benchmark = make_returns({-0.125, 0.0, 0.125});
    require_absent(calculate_correlation(strategy, benchmark));
    require_present(calculate_beta(strategy, benchmark), 0.0);
    require_present(calculate_jensen_alpha(strategy, benchmark, 4.0, 0.0), 0.0);
}

TEST_CASE("RelativeMetrics Jensen alpha remains defined for a constant strategy") {
    const auto strategy = make_returns({0.125, 0.125, 0.125});
    const auto benchmark = make_returns({-0.125, 0.0, 0.125});
    require_present(calculate_beta(strategy, benchmark), 0.0);
    require_present(calculate_jensen_alpha(strategy, benchmark, 4.0, 0.0), 0.5);
}

TEST_CASE("RelativeMetrics varying strategy against constant benchmark has no beta or alpha") {
    const auto strategy = make_returns({0.0, 0.125, 0.25});
    const auto benchmark = make_returns({0.0, 0.0, 0.0});
    require_absent(calculate_correlation(strategy, benchmark));
    require_absent(calculate_beta(strategy, benchmark));
    require_absent(calculate_jensen_alpha(strategy, benchmark));
    const auto active = build_active_return_series(strategy, benchmark);
    REQUIRE(calculate_tracking_error(active).has_value());
    REQUIRE(calculate_information_ratio(active).has_value());
}

TEST_CASE("RelativeMetrics two constant histories have undefined relative regression") {
    const auto strategy = make_returns({0.125, 0.125, 0.125});
    const auto benchmark = make_returns({0.0, 0.0, 0.0});
    require_absent(calculate_correlation(strategy, benchmark));
    require_absent(calculate_beta(strategy, benchmark));
    require_absent(calculate_jensen_alpha(strategy, benchmark));
    const auto active = build_active_return_series(strategy, benchmark);
    require_present(calculate_tracking_error(active), 0.0);
    require_absent(calculate_information_ratio(active));
}

TEST_CASE("RelativeMetrics empty and singleton paired histories have no sample metrics") {
    for (const auto& series : {
        make_returns({}), make_returns({0.125}), make_returns({std::nullopt})
    }) {
        require_absent(calculate_correlation(series, series));
        require_absent(calculate_beta(series, series));
        require_absent(calculate_jensen_alpha(series, series));
    }
}

TEST_CASE("RelativeMetrics missing simple returns invalidate whole-series regression") {
    const auto strategy = make_returns({0.0, std::nullopt, 0.25, 0.125});
    const auto benchmark = make_returns({-0.125, 0.0, 0.125, 0.25});
    require_absent(calculate_correlation(strategy, benchmark));
    require_absent(calculate_beta(strategy, benchmark));
    require_absent(calculate_jensen_alpha(strategy, benchmark));
    const auto active = build_active_return_series(strategy, benchmark);
    require_absent(calculate_tracking_error(active));
    require_absent(calculate_information_ratio(active));
}

TEST_CASE("RelativeMetrics mismatched paired lengths are rejected") {
    const auto short_series = make_returns({0.0, 0.1});
    const auto long_series = make_returns({0.0, 0.1, 0.2});
    REQUIRE_THROWS_AS(
        calculate_correlation(short_series, long_series), std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        calculate_beta(long_series, short_series), std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        calculate_jensen_alpha(long_series, short_series), std::invalid_argument
    );
}

TEST_CASE("RelativeMetrics mismatched paired dates are rejected") {
    const auto strategy = make_returns({0.0, 0.1, 0.2});
    auto benchmark = make_returns({0.0, 0.1, 0.2});
    benchmark[2].session_date = session_date(5);
    REQUIRE_THROWS_AS(
        calculate_correlation(strategy, benchmark), std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        calculate_beta(strategy, benchmark), std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        calculate_jensen_alpha(strategy, benchmark), std::invalid_argument
    );
}

TEST_CASE("RelativeMetrics duplicate and descending paired dates are rejected") {
    const auto strategy = make_returns({0.0, 0.1, 0.2});
    auto benchmark = make_returns({0.0, 0.1, 0.2});
    for (const int offset : {1, -1}) {
        benchmark[2].session_date = session_date(offset);
        REQUIRE_THROWS_AS(
            calculate_correlation(benchmark, benchmark), std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            calculate_beta(benchmark, benchmark), std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            calculate_jensen_alpha(benchmark, benchmark), std::invalid_argument
        );
    }
    REQUIRE(strategy.size() == 3);
}

TEST_CASE("RelativeMetrics invalid session dates are rejected") {
    auto strategy = make_returns({0.0, 0.1});
    const auto benchmark = make_returns({0.0, 0.1});
    strategy[0].session_date = SessionDate{
        std::chrono::year{2026}, std::chrono::month{2}, std::chrono::day{30}
    };
    REQUIRE_THROWS_AS(
        calculate_correlation(strategy, benchmark), std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        calculate_beta(strategy, benchmark), std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        calculate_jensen_alpha(strategy, benchmark), std::invalid_argument
    );
}

TEST_CASE("RelativeMetrics nonfinite simple returns are rejected") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    for (const double invalid : {nan, infinity, -infinity}) {
        const auto strategy = make_returns({0.0, invalid, 0.25});
        const auto benchmark = make_returns({0.0, 0.125, 0.25});
        REQUIRE_THROWS_AS(
            calculate_correlation(strategy, benchmark), std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            calculate_beta(strategy, benchmark), std::invalid_argument
        );
        REQUIRE_THROWS_AS(
            calculate_jensen_alpha(strategy, benchmark), std::invalid_argument
        );
    }
}

TEST_CASE("RelativeMetrics Jensen alpha rejects invalid annualization") {
    const auto strategy = make_returns({0.0, 0.1});
    const auto benchmark = make_returns({0.0, 0.2});
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    for (const double invalid : {0.0, -4.0, nan, infinity, -infinity}) {
        REQUIRE_THROWS_AS(
            calculate_jensen_alpha(strategy, benchmark, invalid, 0.0),
            std::invalid_argument
        );
    }
}

TEST_CASE("RelativeMetrics Jensen alpha rejects invalid annual risk-free rates") {
    const auto strategy = make_returns({0.0, 0.1});
    const auto benchmark = make_returns({0.0, 0.2});
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    for (const double invalid : {-1.0, -1.5, nan, infinity, -infinity}) {
        REQUIRE_THROWS_AS(
            calculate_jensen_alpha(strategy, benchmark, 4.0, invalid),
            std::invalid_argument
        );
    }
}

TEST_CASE("RelativeMetrics Jensen alpha uses an effective periodic risk-free rate") {
    const auto strategy = make_returns({0.0, 0.0, 0.0});
    const auto benchmark = make_returns({-0.125, 0.0, 0.125});
    // Beta is zero, so annual alpha = -A * periodic risk-free return.
    require_present(
        calculate_jensen_alpha(strategy, benchmark, 4.0, 0.04),
        -4.0 * std::expm1(std::log1p(0.04) / 4.0)
    );
    require_present(calculate_jensen_alpha(strategy, benchmark, 4.0, 0.0), 0.0);
}

TEST_CASE("RelativeMetrics Jensen alpha rejects nonrepresentable annual output") {
    const auto strategy = make_returns({1e200, 1e200, 1e200});
    const auto benchmark = make_returns({-0.125, 0.0, 0.125});
    REQUIRE_THROWS_AS(
        calculate_jensen_alpha(strategy, benchmark, 1e200, 0.0),
        std::overflow_error
    );
}

TEST_CASE("RelativeMetrics beta retains finite value when sample covariance underflows") {
    // Sample SDs remain representable; sample covariance is below denorm_min.
    // Pearson correlation is 1. Beta must be 1, not 0.
    const auto returns = make_returns({-1e-200, 0.0, 1e-200});
    require_present(calculate_correlation(returns, returns), 1.0);
    require_present(calculate_beta(returns, returns), 1.0);
}

// -----------------------------------------------------------------------------
// Aggregate results: Oracle C, Oracle D, Oracle E, availability, invariants.
// -----------------------------------------------------------------------------

TEST_CASE("RelativeMetrics aggregate matches all seven approved Oracle D values") {
    const auto result = calculate_relative_performance_metrics(
        oracle_d_strategy_metrics(),
        oracle_d_benchmark_metrics(),
        oracle_d_strategy(),
        oracle_d_benchmark(),
        4.0,
        0.04
    );
    require_present(result.excess_total_return, 0.05140698);
    require_present(result.annualized_return_difference, 0.040922389200769);
    require_present(result.tracking_error, 0.024494897427832);
    require_present(result.information_ratio, 1.632993161855452);
    require_present(result.correlation, 0.670820393249937);
    require_present(result.beta, 0.3);
    require_present(result.jensen_alpha, 0.012410461662887);
}

TEST_CASE("RelativeMetrics aggregate Oracle C recognizes flat outperforming falling benchmark") {
    const auto result = calculate_relative_performance_metrics(
        make_absolute_metrics(0.0),
        make_absolute_metrics(-0.10),
        make_returns({0.0, 0.0, 0.0, 0.0}),
        make_returns({0.0, 0.10, 1.0 / 11.0, -0.25}),
        4.0,
        0.0
    );
    require_present(result.excess_total_return, 0.10);
    require_absent(result.annualized_return_difference);
    REQUIRE(result.tracking_error.has_value());
    REQUIRE(*result.tracking_error > 0.0);
    REQUIRE(result.information_ratio.has_value());
    require_absent(result.correlation);
    require_present(result.beta, 0.0);
    require_present(result.jensen_alpha, 0.0);
}

TEST_CASE("RelativeMetrics aggregate agrees with all individual calculations") {
    const auto strategy = oracle_d_strategy();
    const auto benchmark = oracle_d_benchmark();
    const auto strategy_metrics = oracle_d_strategy_metrics();
    const auto benchmark_metrics = oracle_d_benchmark_metrics();
    const auto active = build_active_return_series(strategy, benchmark);
    const auto result = calculate_relative_performance_metrics(
        strategy_metrics, benchmark_metrics, strategy, benchmark, 4.0, 0.04
    );
    require_same_optional(
        result.excess_total_return,
        calculate_excess_total_return(
            strategy_metrics.total_return, benchmark_metrics.total_return
        )
    );
    require_same_optional(
        result.annualized_return_difference,
        calculate_annualized_return_difference(
            strategy_metrics.cagr, benchmark_metrics.cagr
        )
    );
    require_same_optional(result.tracking_error, calculate_tracking_error(active, 4.0));
    require_same_optional(result.information_ratio, calculate_information_ratio(active, 4.0));
    require_same_optional(result.correlation, calculate_correlation(strategy, benchmark));
    require_same_optional(result.beta, calculate_beta(strategy, benchmark));
    require_same_optional(
        result.jensen_alpha,
        calculate_jensen_alpha(strategy, benchmark, 4.0, 0.04)
    );
}

TEST_CASE("RelativeMetrics aggregate retains endpoint values for empty histories") {
    const ReturnSeries empty;
    const auto result = calculate_relative_performance_metrics(
        make_absolute_metrics(0.10, 0.05),
        make_absolute_metrics(0.04, 0.02),
        empty, empty
    );
    require_present(result.excess_total_return, 0.06);
    require_present(result.annualized_return_difference, 0.03);
    require_no_statistical_metrics(result);
}

TEST_CASE("RelativeMetrics aggregate singleton preserves endpoint metrics") {
    const auto strategy = make_returns({0.125});
    const auto benchmark = make_returns({0.0});
    const auto result = calculate_relative_performance_metrics(
        make_absolute_metrics(0.125),
        make_absolute_metrics(0.0),
        strategy, benchmark
    );
    require_present(result.excess_total_return, 0.125);
    require_absent(result.annualized_return_difference);
    require_no_statistical_metrics(result);
}

TEST_CASE("RelativeMetrics aggregate preserves endpoint metrics when a simple return is missing") {
    const auto strategy = make_returns({0.0, std::nullopt, 0.10});
    const auto benchmark = make_returns({0.0, 0.05, 0.02});
    const auto result = calculate_relative_performance_metrics(
        make_absolute_metrics(0.12, 0.08),
        make_absolute_metrics(0.02, 0.01),
        strategy, benchmark
    );
    require_present(result.excess_total_return, 0.10);
    require_present(result.annualized_return_difference, 0.07);
    require_no_statistical_metrics(result);
}

TEST_CASE("RelativeMetrics aggregate retains only available endpoint differences") {
    const ReturnSeries empty;
    const auto result = calculate_relative_performance_metrics(
        make_absolute_metrics(std::nullopt, 0.05),
        make_absolute_metrics(0.04, std::nullopt),
        empty, empty
    );
    require_absent(result.excess_total_return);
    require_absent(result.annualized_return_difference);
    require_no_statistical_metrics(result);
}

TEST_CASE("RelativeMetrics aggregate Oracle E identical series have zero tracking error") {
    const auto returns = make_returns({-0.125, 0.0, 0.125});
    const auto result = calculate_relative_performance_metrics(
        make_absolute_metrics(0.0, 0.0),
        make_absolute_metrics(0.0, 0.0),
        returns, returns, 4.0, 0.04
    );
    require_present(result.excess_total_return, 0.0);
    require_present(result.annualized_return_difference, 0.0);
    require_present(result.tracking_error, 0.0);
    require_absent(result.information_ratio);
    require_present(result.correlation, 1.0);
    require_present(result.beta, 1.0);
    require_present(result.jensen_alpha, 0.0);
}

TEST_CASE("RelativeMetrics aggregate Oracle E constant strategy retains beta and alpha") {
    const auto strategy = make_returns({0.125, 0.125, 0.125});
    const auto benchmark = make_returns({-0.125, 0.0, 0.125});
    const auto result = calculate_relative_performance_metrics(
        {}, {}, strategy, benchmark, 4.0, 0.0
    );
    require_absent(result.correlation);
    require_present(result.beta, 0.0);
    require_present(result.jensen_alpha, 0.5);
}

TEST_CASE("RelativeMetrics aggregate Oracle E constant benchmark leaves regression unavailable") {
    const auto strategy = make_returns({0.0, 0.125, 0.25});
    const auto benchmark = make_returns({0.0, 0.0, 0.0});
    const auto result = calculate_relative_performance_metrics(
        {}, {}, strategy, benchmark, 4.0, 0.0
    );
    REQUIRE(result.tracking_error.has_value());
    REQUIRE(result.information_ratio.has_value());
    require_absent(result.correlation);
    require_absent(result.beta);
    require_absent(result.jensen_alpha);
}

TEST_CASE("RelativeMetrics aggregate Oracle E both constant series are degenerate") {
    const auto strategy = make_returns({0.125, 0.125, 0.125});
    const auto benchmark = make_returns({0.0, 0.0, 0.0});
    const auto result = calculate_relative_performance_metrics(
        {}, {}, strategy, benchmark
    );
    require_present(result.tracking_error, 0.0);
    require_absent(result.information_ratio);
    require_absent(result.correlation);
    require_absent(result.beta);
    require_absent(result.jensen_alpha);
}

TEST_CASE("RelativeMetrics aggregate rejects mismatched dates before publishing endpoint values") {
    auto benchmark = make_returns({0.0, 0.125, 0.25});
    benchmark[1].session_date = session_date(4);
    REQUIRE_THROWS_AS(
        calculate_relative_performance_metrics(
            make_absolute_metrics(0.10, 0.05),
            make_absolute_metrics(0.04, 0.02),
            make_returns({0.0, 0.125, 0.25}), benchmark
        ),
        std::invalid_argument
    );
}

TEST_CASE("RelativeMetrics aggregate rejects differing return history lengths") {
    REQUIRE_THROWS_AS(
        calculate_relative_performance_metrics(
            {}, {}, make_returns({0.0, 0.1}), make_returns({0.0})
        ),
        std::invalid_argument
    );
}

TEST_CASE("RelativeMetrics aggregate rejects nonfinite endpoint inputs") {
    const double infinity = std::numeric_limits<double>::infinity();
    const ReturnSeries empty;
    REQUIRE_THROWS_AS(
        calculate_relative_performance_metrics(
            make_absolute_metrics(infinity), {}, empty, empty
        ),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        calculate_relative_performance_metrics(
            {}, make_absolute_metrics(std::nullopt, infinity), empty, empty
        ),
        std::invalid_argument
    );
}

TEST_CASE("RelativeMetrics aggregate rejects invalid annualization and risk-free inputs") {
    const ReturnSeries empty;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    for (const double invalid : {0.0, -1.0, nan, infinity, -infinity}) {
        REQUIRE_THROWS_AS(
            calculate_relative_performance_metrics(
                {}, {}, empty, empty, invalid, 0.0
            ),
            std::invalid_argument
        );
    }
    for (const double invalid : {-1.0, -2.0, nan, infinity, -infinity}) {
        REQUIRE_THROWS_AS(
            calculate_relative_performance_metrics(
                {}, {}, empty, empty, 4.0, invalid
            ),
            std::invalid_argument
        );
    }
}

TEST_CASE("RelativeMetrics aggregate preserves all caller owned inputs") {
    auto strategy = oracle_d_strategy();
    auto benchmark = oracle_d_benchmark();
    const auto saved_strategy = strategy;
    const auto saved_benchmark = benchmark;
    auto sm = oracle_d_strategy_metrics();
    auto bm = oracle_d_benchmark_metrics();
    const auto saved_sm = sm;
    const auto saved_bm = bm;
    (void)calculate_relative_performance_metrics(sm, bm, strategy, benchmark, 4.0, 0.04);
    REQUIRE(strategy.size() == saved_strategy.size());
    REQUIRE(benchmark.size() == saved_benchmark.size());
    for (std::size_t i = 0; i < strategy.size(); ++i) {
        REQUIRE(strategy[i].session_date == saved_strategy[i].session_date);
        REQUIRE(strategy[i].simple_return == saved_strategy[i].simple_return);
        REQUIRE(benchmark[i].session_date == saved_benchmark[i].session_date);
        REQUIRE(benchmark[i].simple_return == saved_benchmark[i].simple_return);
    }
    REQUIRE(sm.total_return == saved_sm.total_return);
    REQUIRE(sm.cagr == saved_sm.cagr);
    REQUIRE(bm.total_return == saved_bm.total_return);
    REQUIRE(bm.cagr == saved_bm.cagr);
}

TEST_CASE("RelativeMetrics aggregate is deterministic across repeated queries") {
    const auto strategy = oracle_d_strategy();
    const auto benchmark = oracle_d_benchmark();
    const auto sm = oracle_d_strategy_metrics();
    const auto bm = oracle_d_benchmark_metrics();
    const auto reference = calculate_relative_performance_metrics(
        sm, bm, strategy, benchmark, 4.0, 0.04
    );
    for (int repetition = 0; repetition < 20; ++repetition) {
        const auto actual = calculate_relative_performance_metrics(
            sm, bm, strategy, benchmark, 4.0, 0.04
        );
        require_same_metrics(actual, reference);
    }
}

TEST_CASE("RelativeMetrics aggregate supports a long aligned history") {
    constexpr std::size_t count = 20'000;
    ReturnSeries strategy;
    ReturnSeries benchmark;
    strategy.reserve(count);
    benchmark.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto date = session_date(static_cast<int>(i));
        const double s = (i % 2 == 0) ? 0.125 : -0.125;
        const double b = (i % 3 == 0) ? 0.25 : -0.25;
        strategy.push_back({date, s, std::nullopt});
        benchmark.push_back({date, b, std::nullopt});
    }
    const auto result = calculate_relative_performance_metrics(
        {}, {}, strategy, benchmark, 252.0, 0.0
    );
    REQUIRE(result.tracking_error.has_value());
    REQUIRE(result.information_ratio.has_value());
    REQUIRE(result.correlation.has_value());
    REQUIRE(result.beta.has_value());
    REQUIRE(result.jensen_alpha.has_value());
    require_absent(result.excess_total_return);
    require_absent(result.annualized_return_difference);
}

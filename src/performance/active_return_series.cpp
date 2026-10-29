#include "backtester/performance/active_return_series.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>

namespace backtester::performance {

ActiveReturnSeries build_active_return_series(
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns
) {
    if (strategy_returns.size() != benchmark_returns.size()) {
        throw std::invalid_argument(
            "ActiveReturnSeries requires matching observation counts"
        );
    }

    ActiveReturnSeries active_returns;
    active_returns.reserve(strategy_returns.size());

    std::optional<market::SessionDate> previous_date;

    for (std::size_t i = 0; i < strategy_returns.size(); ++i) {
        const auto& strategy = strategy_returns[i];
        const auto& benchmark = benchmark_returns[i];

        if (
            !strategy.session_date.ok() ||
            !benchmark.session_date.ok()
        ) {
            throw std::invalid_argument(
                "ActiveReturnSeries requires valid session dates"
            );
        }

        if (strategy.session_date != benchmark.session_date) {
            throw std::invalid_argument(
                "ActiveReturnSeries requires matching session dates"
            );
        }

        if (
            previous_date &&
            strategy.session_date <= *previous_date
        ) {
            throw std::invalid_argument(
                "ActiveReturnSeries requires strictly increasing dates"
            );
        }

        if (
            (strategy.simple_return &&
            !std::isfinite(*strategy.simple_return)) ||
            (benchmark.simple_return &&
            !std::isfinite(*benchmark.simple_return))
        ) {
            throw std::invalid_argument(
                "ActiveReturnSeries requires finite simple returns"
            );
        }

        std::optional<double> active_return;

        if (strategy.simple_return && benchmark.simple_return) {
            const double difference =
                *strategy.simple_return - *benchmark.simple_return;

            if (!std::isfinite(difference)) {
                throw std::overflow_error(
                    "ActiveReturnSeries calculation produced a nonfinite result"
                );
            }

            active_return = difference;
        }

        active_returns.push_back({
            strategy.session_date,
            active_return
        });

        previous_date = strategy.session_date;
    }

    return active_returns;
}

}  // namespace backtester::performance
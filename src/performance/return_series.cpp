#include "backtester/performance/return_series.hpp"

#include <cmath>
#include <stdexcept>
#include <optional>

namespace backtester::performance {

ReturnSeries build_return_series(
    double initial_equity,
    std::span<const portfolio::PortfolioSnapshot> snapshots
) {
    if (!std::isfinite(initial_equity) || initial_equity <= 0.0) {
        throw std::invalid_argument(
            "ReturnSeries requires finite and positive initial equity"
        );
    }

    ReturnSeries returns;
    returns.reserve(snapshots.size());

    double previous_equity = initial_equity;
    std::optional<market::SessionDate> previous_date;

    for (const auto& snapshot : snapshots) {
        const auto date = snapshot.session_date();
        const double equity = snapshot.equity();

        if (!date.ok()) {
            throw std::invalid_argument(
                "ReturnSeries requires valid session date"
            );
        }

        if (previous_date && date <= *previous_date) {
            throw std::invalid_argument(
                "ReturnSeries requires strictly increasing session dates"
            );
        }

        if (!std::isfinite(equity)) {
            throw std::invalid_argument(
                "ReturnSeries requires finite snapshot equity"
            );
        }

        std::optional<double> simple_return;
        std::optional<double> log_return;

        // Preserve precision for small positive-equity changes.
        // The ratio form avoids subtraction overflow for negative equity.
        if (previous_equity > 0.0) {
            const double simple = equity >= 0.0
                ? (equity - previous_equity) / previous_equity
                : equity / previous_equity - 1.0;

            if (!std::isfinite(simple)) {
                throw std::overflow_error(
                    "ReturnSeries simple return is not finite"
                );
            }

            simple_return = simple;

            if (equity > 0.0) {
                // Extremely small positive equity can produce a rounded simple
                // return of -1.0. In that case, we use log differences to
                // preserve a finite result.
                const double logarithmic = simple > -1.0
                    ? std::log1p(simple)
                    : std::log(equity) - std::log(previous_equity);

                if (!std::isfinite(logarithmic)) {
                    throw std::overflow_error(
                        "ReturnSeries logarithmic return is not finite"
                    );
                }

                log_return = logarithmic;
            }
        }

        returns.push_back({
            date,
            simple_return,
            log_return
        });

        previous_equity = equity;
        previous_date = date;
    }

    return returns;
}

}  // namespace backtester::performance
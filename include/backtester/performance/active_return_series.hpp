#pragma once

#include "backtester/performance/return_series.hpp"

#include <optional>
#include <span>
#include <vector>

namespace backtester::performance {

struct ActiveReturnObservation final {
    market::SessionDate session_date;
    std::optional<double> active_return;
};

using ActiveReturnSeries = std::vector<ActiveReturnObservation>;

[[nodiscard]]
ActiveReturnSeries build_active_return_series(
    std::span<const ReturnObservation> strategy_returns,
    std::span<const ReturnObservation> benchmark_returns
);

}  // namespace backtester::performance
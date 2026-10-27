#pragma once

#include "backtester/market/bar.hpp"
#include "backtester/portfolio/portfolio.hpp"

#include <optional>
#include <span>
#include <vector>

namespace backtester::performance {

struct ReturnObservation {
    market::SessionDate session_date;
    std::optional<double> simple_return;
    std::optional<double> log_return;
};

using ReturnSeries = std::vector<ReturnObservation>;

[[nodiscard]]
ReturnSeries build_return_series(
    double initial_equity,
    std::span<const portfolio::PortfolioSnapshot> snapshots
);

}  // namespace backtester::performance
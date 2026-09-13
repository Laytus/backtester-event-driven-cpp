#pragma once

#include "backtester/market/bar.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace backtester::data {

class HistoricalDataFeed {
public:
    explicit HistoricalDataFeed(
        std::vector<market::Bar> bars
    );

    [[nodiscard]]
    std::optional<market::Bar> next();

private:
    std::vector<market::Bar> bars_;
    std::size_t next_index_{0};
};

}  // namespace backtester::data
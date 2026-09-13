#include "backtester/data/historical_data_feed.hpp"

#include <cstddef>
#include <optional>
#include <vector>

#include <utility>

namespace backtester::data {

HistoricalDataFeed::HistoricalDataFeed(
    std::vector<market::Bar> bars
)
    : bars_(std::move(bars)) {}

std::optional<market::Bar> HistoricalDataFeed::next() {
    if (next_index_ >= bars_.size()) {
        return std::nullopt;
    }

    const market::Bar bar = bars_[next_index_];

    ++next_index_;

    return bar;
}

}  // namespace backtester::data
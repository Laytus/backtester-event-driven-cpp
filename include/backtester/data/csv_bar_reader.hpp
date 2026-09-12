#pragma once

#include "backtester/market/bar.hpp"

#include <filesystem>
#include <vector>

namespace backtester::data {

class CsvBarReader {
public:
    [[nodiscard]]
    static std::vector<market::Bar> read(
        const std::filesystem::path& path
    );
};

}  // namespace backtester::data
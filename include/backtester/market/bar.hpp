#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace backtester::market {

class Bar {
public:
    using SessionDate = std::chrono::year_month_day;

    Bar(
        SessionDate session_date,
        std::string symbol,
        double open,
        double high,
        double low,
        double close,
        std::uint64_t volume
    );

    [[nodiscard]] SessionDate session_date() const noexcept;
    [[nodiscard]] const std::string& symbol() const noexcept;

    [[nodiscard]] double open() const noexcept;
    [[nodiscard]] double high() const noexcept;
    [[nodiscard]] double low() const noexcept;
    [[nodiscard]] double close() const noexcept;

    [[nodiscard]] std::uint64_t volume() const noexcept;

private:
    SessionDate session_date_;
    std::string symbol_;

    double open_;
    double high_;
    double low_;
    double close_;

    std::uint64_t volume_;
};

}  // namespace backtester::market
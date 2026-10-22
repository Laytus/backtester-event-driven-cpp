#pragma once

#include <cstddef>
#include <deque>
#include <optional>

namespace backtester::indicators {

struct RollingMinMaxValue final {
    double minimum;
    double maximum;
};

class RollingMinMax final {
public:
    explicit RollingMinMax(std::size_t period = 20);

    void update(double observation);

    [[nodiscard]]
    std::optional<RollingMinMaxValue> value() const noexcept;

    [[nodiscard]]
    std::size_t period() const noexcept;

private:
    struct Candidate {
        std::size_t index;
        double value;
    };

    std::size_t period_;
    std::size_t observations_seen_{0};

    std::deque<Candidate> min_candidates_;
    std::deque<Candidate> max_candidates_;
};

}  // namespace backtester::indicators
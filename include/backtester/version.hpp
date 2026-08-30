#pragma once

#include <string_view>

namespace backtester {

[[nodiscard]] std::string_view version() noexcept;

}  // namespace backtester


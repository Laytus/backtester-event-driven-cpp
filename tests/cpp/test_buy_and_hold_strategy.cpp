#include "backtester/strategy/buy_and_hold_strategy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using backtester::events::MarketEvent;
using backtester::events::SignalDirection;
using backtester::events::SignalEvent;
using backtester::market::Bar;
using backtester::market::SessionDate;
using backtester::strategy::BuyAndHoldStrategy;

MarketEvent make_market_event(
    std::string symbol,
    SessionDate session_date
) {
    return MarketEvent{
        Bar{
            session_date,
            std::move(symbol),
            100.0,
            105.0,
            95.0,
            102.0,
            1'000
        }
    };
}

void require_same_signals(
    const std::vector<SignalEvent>& actual,
    const std::vector<SignalEvent>& expected
) {
    REQUIRE(actual.size() == expected.size());

    for (std::size_t i = 0; i < actual.size(); ++i) {
        REQUIRE(actual[i].symbol() == expected[i].symbol());
        REQUIRE(actual[i].direction() == expected[i].direction());
        REQUIRE(actual[i].signal_date() == expected[i].signal_date());
    }
}

static_assert(
    !std::is_copy_constructible_v<BuyAndHoldStrategy>
);

static_assert(
    !std::is_copy_assignable_v<BuyAndHoldStrategy>
);

static_assert(
    std::is_move_constructible_v<BuyAndHoldStrategy>
);

static_assert(
    std::is_move_assignable_v<BuyAndHoldStrategy>
);

}  // namespace

TEST_CASE(
    "BuyAndHoldStrategy accepts a non-empty symbol",
    "[strategy][buy-and-hold]"
) {
    REQUIRE_NOTHROW(
        BuyAndHoldStrategy{"AAPL"}
    );
}

TEST_CASE(
    "BuyAndHoldStrategy rejects an empty symbol",
    "[strategy][buy-and-hold]"
) {
    REQUIRE_THROWS_AS(
        BuyAndHoldStrategy{""},
        std::invalid_argument
    );
}

TEST_CASE(
    "BuyAndHoldStrategy emits Long on the first matching market event",
    "[strategy][buy-and-hold]"
) {
    BuyAndHoldStrategy strategy{"AAPL"};

    const SessionDate session_date{
        std::chrono::year{2026},
        std::chrono::month{1},
        std::chrono::day{2}
    };

    const auto signals = strategy.on_market(
        make_market_event("AAPL", session_date)
    );

    REQUIRE(signals.size() == 1);

    const auto& signal = signals.front();

    REQUIRE(signal.symbol() == "AAPL");
    REQUIRE(signal.direction() == SignalDirection::Long);
    REQUIRE(signal.signal_date() == session_date);
}

TEST_CASE(
    "BuyAndHoldStrategy does not repeat the entry signal",
    "[strategy][buy-and-hold]"
) {
    BuyAndHoldStrategy strategy{"AAPL"};

    const auto first = strategy.on_market(
        make_market_event(
            "AAPL",
            SessionDate{
                std::chrono::year{2026},
                std::chrono::month{1},
                std::chrono::day{2}
            }
        )
    );

    const auto second = strategy.on_market(
        make_market_event(
            "AAPL",
            SessionDate{
                std::chrono::year{2026},
                std::chrono::month{1},
                std::chrono::day{5}
            }
        )
    );

    const auto third = strategy.on_market(
        make_market_event(
            "AAPL",
            SessionDate{
                std::chrono::year{2026},
                std::chrono::month{1},
                std::chrono::day{6}
            }
        )
    );

    REQUIRE(first.size() == 1);
    REQUIRE(second.empty());
    REQUIRE(third.empty());
}

TEST_CASE(
    "BuyAndHoldStrategy ignores unrelated symbols",
    "[strategy][buy-and-hold]"
) {
    BuyAndHoldStrategy strategy{"AAPL"};

    const auto signals = strategy.on_market(
        make_market_event(
            "MSFT",
            SessionDate{
                std::chrono::year{2026},
                std::chrono::month{1},
                std::chrono::day{2}
            }
        )
    );

    REQUIRE(signals.empty());
}

TEST_CASE(
    "BuyAndHoldStrategy unrelated symbols do not consume the entry opportunity",
    "[strategy][buy-and-hold]"
) {
    BuyAndHoldStrategy strategy{"AAPL"};

    const auto unrelated = strategy.on_market(
        make_market_event(
            "MSFT",
            SessionDate{
                std::chrono::year{2026},
                std::chrono::month{1},
                std::chrono::day{2}
            }
        )
    );

    const SessionDate matching_date{
        std::chrono::year{2026},
        std::chrono::month{1},
        std::chrono::day{5}
    };

    const auto matching = strategy.on_market(
        make_market_event("AAPL", matching_date)
    );

    REQUIRE(unrelated.empty());

    REQUIRE(matching.size() == 1);
    REQUIRE(matching.front().symbol() == "AAPL");
    REQUIRE(matching.front().direction() == SignalDirection::Long);
    REQUIRE(matching.front().signal_date() == matching_date);
}

TEST_CASE(
    "BuyAndHoldStrategy instances maintain independent state",
    "[strategy][buy-and-hold]"
) {
    BuyAndHoldStrategy first_strategy{"AAPL"};
    BuyAndHoldStrategy second_strategy{"AAPL"};

    const SessionDate session_date{
        std::chrono::year{2026},
        std::chrono::month{1},
        std::chrono::day{2}
    };

    const auto first_signals = first_strategy.on_market(
        make_market_event("AAPL", session_date)
    );

    const auto second_signals = second_strategy.on_market(
        make_market_event("AAPL", session_date)
    );

    REQUIRE(first_signals.size() == 1);
    REQUIRE(second_signals.size() == 1);

    REQUIRE(first_signals.front().direction() == SignalDirection::Long);
    REQUIRE(second_signals.front().direction() == SignalDirection::Long);
}

TEST_CASE(
    "BuyAndHoldStrategy is deterministic for identical event sequences",
    "[strategy][buy-and-hold]"
) {
    BuyAndHoldStrategy first_strategy{"AAPL"};
    BuyAndHoldStrategy second_strategy{"AAPL"};

    const std::vector<MarketEvent> events{
        make_market_event(
            "MSFT",
            SessionDate{
                std::chrono::year{2026},
                std::chrono::month{1},
                std::chrono::day{2}
            }
        ),
        make_market_event(
            "AAPL",
            SessionDate{
                std::chrono::year{2026},
                std::chrono::month{1},
                std::chrono::day{5}
            }
        ),
        make_market_event(
            "AAPL",
            SessionDate{
                std::chrono::year{2026},
                std::chrono::month{1},
                std::chrono::day{6}
            }
        ),
        make_market_event(
            "AAPL",
            SessionDate{
                std::chrono::year{2026},
                std::chrono::month{1},
                std::chrono::day{7}
            }
        )
    };

    for (const auto& event : events) {
        const auto first_signals =
            first_strategy.on_market(event);

        const auto second_signals =
            second_strategy.on_market(event);

        require_same_signals(
            first_signals,
            second_signals
        );
    }
}
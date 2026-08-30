# Architecture

> **Purpose**
>
> This document defines the evolving technical architecture of the
> Event-Driven Backtester.
>
> It translates the conceptual model and MVP scope into concrete software
> components, responsibilities, ownership rules, interfaces, invariants, and
> dependencies.
>
> Unlike the other core project documents, this is a **living document**.
> It should evolve when implementation reveals a better architectural decision.
>
> Architectural changes must remain consistent with:
>
> - `MVP-scope.md`
> - `project-principles.md`
> - `development-guide.md`
> - `knowledge-guide.md`
> - `references.md`

---

# 1. Architectural Objective

The MVP architecture must support a backtester that is:

- deterministic;
- sequential;
- testable;
- reproducible;
- easy to explain;
- small enough to understand completely;
- extensible through clean component boundaries.

The architecture should avoid both extremes:

```text
monolithic backtester
        ← avoid →

speculative framework for future features
```

The target is:

> The smallest architecture that correctly represents the complete event-driven
> trading lifecycle required by the MVP.

---

# 2. System Context

The complete project contains two main technical environments.

```text
Python Tooling
        │
        │ acquisition / validation / normalization
        ▼
Canonical Historical Dataset
        │
        ▼
C++ Backtesting Engine
        │
        │ results
        ▼
CSV Outputs
        │
        ▼
Python Analysis / Visualization
```

Python supports the backtester.

C++ owns the core simulation.

---

# 3. High-Level Architecture

The MVP system is organized around the following flow:

```text
Canonical Dataset
        ↓
CsvBarReader
        ↓
HistoricalDataFeed
        ↓
MarketEvent
        ↓
BacktestEngine
        ↓
Strategy
        ↓
SignalEvent
        ↓
Portfolio / PositionSizer
        ↓
OrderEvent
        ↓
ExecutionHandler
        ↓
FillEvent
        ↓
Portfolio
        ↓
Portfolio Snapshot / Equity History
        ↓
Performance
        ↓
Reporting
```

The `BacktestEngine` coordinates the flow.

It must not absorb the responsibilities of the domain components.

---

# 4. Main Architectural Layers

The MVP can be understood through six logical layers.

```text
1. Data Infrastructure
2. Event Protocol
3. Strategy
4. Portfolio and Sizing
5. Execution
6. Performance and Reporting
```

These are logical boundaries.

They do not necessarily require separate libraries or complex package
boundaries.

---

# 5. Data Infrastructure

The data layer converts a canonical dataset into sequential market observations.

## Components

- `Bar`
- `CsvBarReader`
- `HistoricalDataFeed`

Conceptual flow:

```text
Canonical CSV
        ↓
CsvBarReader
        ↓
std::vector<Bar> or equivalent validated storage
        ↓
HistoricalDataFeed
        ↓
sequential observable bars
```

Provider-specific acquisition and normalization remain outside this C++ layer.

---

# 6. Bar

## Responsibility

Represent one canonical daily OHLCV observation.

## Suggested Data

Conceptually:

```cpp
struct Bar {
    Timestamp timestamp;
    double open;
    double high;
    double low;
    double close;
    double volume;
};
```

The exact timestamp type should use `std::chrono`-based semantics.

The exact volume type should be chosen during implementation based on the
canonical dataset contract.

## Invariants

A valid `Bar` should satisfy the canonical data rules established by the data
pipeline.

Examples may include:

- valid timestamp;
- finite numeric values;
- positive prices where required;
- `high >= open`;
- `high >= close`;
- `low <= open`;
- `low <= close`;
- `high >= low`;
- non-negative volume.

Validation responsibilities between Python normalization and C++ parsing should
remain explicit.

## Ownership

`Bar` is a value type.

It does not own external resources.

---

# 7. CsvBarReader

## Responsibility

Parse the provider-independent canonical CSV format into validated `Bar`
objects.

## Inputs

- canonical CSV path or input stream.

## Outputs

- successfully parsed bars;
- explicit parsing/validation error.

## Must Not

- download data;
- know about `yfinance`;
- rename provider-specific columns;
- implement strategy logic;
- expose future-data semantics.

## Important Failure Modes

- missing required columns;
- malformed numeric values;
- invalid timestamps;
- missing values;
- duplicate timestamps;
- non-monotonic timestamps;
- invalid rows.

## Design Principle

Parsing errors must be explicit.

The reader should not silently repair ambiguous input.

---

# 8. HistoricalDataFeed

## Responsibility

Reveal historical bars sequentially as simulated time advances.

## Owned State

Conceptually:

- historical bar storage or reference to immutable storage;
- current cursor/index;
- end-of-data state.

## Public Behavior

The feed should support operations equivalent to:

```text
has_next()
next()
```

or another minimal interface with the same semantics.

The exact interface should be chosen during Phase 1.

## Invariants

- bars are exposed chronologically;
- each bar is exposed at most once;
- future bars cannot be retrieved through arbitrary indexing in normal engine
  use;
- end-of-data behavior is deterministic.

## Must Not

- generate strategy signals;
- modify portfolio state;
- determine execution;
- calculate performance.

---

# 9. Event Protocol

The MVP contains four explicit domain events:

```text
MarketEvent
SignalEvent
OrderEvent
FillEvent
```

These events form the causal protocol between components.

The event model must remain explicit and type-safe.

---

# 10. Event Representation

The preferred initial C++ direction is a closed event set represented using
value types.

A likely design is:

```cpp
using Event = std::variant<
    MarketEvent,
    SignalEvent,
    OrderEvent,
    FillEvent
>;
```

with dispatch through `std::visit`.

This is an initial architectural preference, not an immutable requirement.

## Why This Fits the MVP

- the event set is small and known;
- events are value-like messages;
- dispatch can remain explicit;
- ownership is simple;
- no inheritance hierarchy is required;
- deterministic behavior is easy to test.

## Reconsider Only If

Implementation reveals a concrete requirement that makes the closed variant
model unnecessarily restrictive.

Do not introduce polymorphic heap-allocated events preemptively.

---

# 11. MarketEvent

## Meaning

New market information is available.

## Producer

- `HistoricalDataFeed` directly or through `BacktestEngine`.

## Consumer

- `Strategy`;
- potentially portfolio valuation logic where appropriate.

## Data

The event should expose the information required to identify the newly
available market observation.

Two plausible designs are:

```text
MarketEvent owns Bar
```

or:

```text
MarketEvent references / identifies currently available Bar
```

The final choice should minimize lifetime complexity.

For the MVP, value semantics are preferred when practical.

## Timestamp Meaning

The timestamp represents the market observation that has become available.

Time semantics must remain consistent with the completed-bar model.

---

# 12. SignalEvent

## Meaning

The strategy expresses trading intent.

## Producer

- `Strategy`.

## Consumer

- `Portfolio` and/or dedicated order-generation / position-sizing logic.

## Suggested Data

Conceptually:

```text
instrument
signal type
signal time
```

MVP signal types:

```text
LONG
EXIT
```

## Must Not Contain

- execution price;
- fill price;
- hidden portfolio mutation;
- assumed trade result.

---

# 13. OrderEvent

## Meaning

A concrete trading instruction has been created.

## Producer

- portfolio/order-generation layer after position sizing.

## Consumer

- `ExecutionHandler`.

## Required MVP Data

Conceptually:

```text
instrument
side
quantity
order type
submission time
```

MVP order type:

```text
MARKET
```

MVP sides:

```text
BUY
SELL
```

## Invariant

Quantity must be valid according to the order contract.

The order does not modify executed portfolio holdings.

---

# 14. FillEvent

## Meaning

The simulated order has executed.

## Producer

- `ExecutionHandler`.

## Consumer

- `Portfolio`.

## Suggested Data

Conceptually:

```text
instrument
side
quantity
fill price
fill time
```

Additional fields should be added only if required by accounting.

## Invariant

A fill represents executed quantity.

Portfolio holdings and cash change as a consequence of fill processing.

---

# 15. Event Queue

## Responsibility

Store pending events awaiting deterministic processing.

## Initial Design

A simple FIFO queue is sufficient if event creation and processing semantics
guarantee the required chronological behavior.

Likely structure:

```cpp
std::queue<Event>
```

or an equivalent container.

## Invariants

- deterministic processing order;
- no implicit parallel dispatch;
- an event is processed at most once.

## Important Principle

The queue orders **simulation reactions**, not market timestamps independently.

If later event scheduling requires explicit future timestamps, the architecture
may evolve.

The MVP should not implement a generalized scheduler unless required.

---

# 16. BacktestEngine

## Responsibility

Coordinate the simulation lifecycle.

The engine is the system orchestrator.

## Dependencies

The engine conceptually depends on:

- `HistoricalDataFeed`;
- event queue;
- `Strategy`;
- `Portfolio`;
- `ExecutionHandler`.

Performance and reporting may run after or alongside portfolio-history
generation depending on the final implementation.

## Main Loop

Conceptually:

```text
while market data remain or events remain:

    expose next available market observation

    enqueue MarketEvent

    process pending events deterministically

        MarketEvent
            → Strategy
            → optional SignalEvent

        SignalEvent
            → Portfolio / PositionSizer
            → optional OrderEvent

        OrderEvent
            → ExecutionHandler
            → pending execution state

        future MarketEvent / next bar open
            → ExecutionHandler
            → FillEvent

        FillEvent
            → Portfolio

    record portfolio valuation when required
```

The exact ordering between:

- pending next-open fills;
- publication of the newly completed bar;
- portfolio valuation;

must be finalized and tested before Phase 3 implementation.

This is one of the most important temporal design decisions in the engine.

---

# 17. Engine Responsibilities

The engine owns coordination.

It may:

- advance the market-data feed;
- enqueue events;
- dispatch events;
- coordinate execution opportunities;
- terminate the simulation.

It should not:

- implement strategy rules;
- calculate order quantity itself;
- mutate portfolio accounting directly;
- choose execution prices;
- calculate performance formulas.

---

# 18. Strategy Interface

## Responsibility

Consume observable market information and optionally produce trading intent.

A minimal conceptual contract is:

```text
on_market(event / bar)
    → optional SignalEvent
```

The exact C++ interface may use:

- a concrete strategy type;
- a small runtime-polymorphic interface;
- another minimal abstraction.

The design should support the two MVP strategies without creating a framework
for hypothetical strategies.

## Strategy-Owned State

Examples:

- entered/not-entered state for Buy-and-Hold;
- moving-average rolling state;
- previous regime/signal state.

## Must Not Own

- cash;
- executed position quantity;
- execution state.

---

# 19. BuyAndHoldStrategy

## Responsibility

Generate the initial long intent required to establish the Buy-and-Hold
position.

## State

Likely minimal state:

```text
has_signaled_entry
```

## Invariant

The strategy should not repeatedly generate unintended entry signals after its
initial desired state is established.

This strategy should be implemented first because it exercises the whole system
with minimal strategy complexity.

---

# 20. MovingAverageCrossoverStrategy

## Responsibility

Generate long/exit signals based on the relationship between short and long
moving averages.

## Owned State

Potentially:

- short-window rolling data;
- long-window rolling data;
- current strategy regime;
- warm-up state.

## Configuration

- short window;
- long window.

## Invariants

- no signal before required warm-up data are available;
- short window must be valid relative to long window;
- transitions are deterministic;
- duplicate unintended signals are avoided.

The exact crossover convention must be documented before implementation.

---

# 21. Moving Average Utility

A minimal reusable moving-average primitive may be introduced if it clearly
simplifies strategy implementation.

## Responsibility

Maintain or calculate a rolling arithmetic mean.

## Non-Goal

Do not build a generic technical-analysis indicator framework.

Only implement what the MVP strategy needs.

---

# 22. Portfolio

## Responsibility

Own the financial state of the backtest.

## Owned State

For the MVP:

```text
initial capital
current cash
current signed position quantity
latest valuation price
current market value
current equity
portfolio history
```

Potential exact representation should remain simple because only one instrument
exists.

## Main Inputs

- `SignalEvent` indirectly for order generation;
- `FillEvent` for executed state changes;
- observable market prices for valuation.

## Main Outputs

- `OrderEvent`;
- portfolio snapshots/history.

---

# 23. Portfolio Responsibilities

The portfolio should:

- own cash;
- own executed position quantity;
- apply fills;
- calculate current market value;
- calculate current equity;
- record portfolio snapshots;
- provide the information needed for position sizing.

The portfolio should not:

- acquire market data;
- generate strategy signals;
- choose execution price;
- calculate final performance statistics.

---

# 24. Position Representation

Because the MVP contains only one instrument, the position representation can
remain deliberately small.

Conceptually:

```cpp
using Quantity = ...;
Quantity position_quantity;
```

or a small `Position` value type if useful.

Signed semantics:

```text
quantity > 0  → long
quantity = 0  → flat
quantity < 0  → short representation
```

The MVP must prevent actual short behavior even though the representation is
signed.

Do not build a generic multi-position container solely for future multi-asset
support.

---

# 25. PositionSizer

## Responsibility

Convert trading intent into a concrete quantity.

Conceptually:

```text
SignalEvent
+ Portfolio State
+ Sizing Policy
        ↓
Order Quantity
```

The MVP should use a deterministic policy.

## Architecture Choice

The position-sizing logic may initially be:

- a small dedicated component; or
- a clearly isolated policy used by `Portfolio`.

A separate class is justified only if it improves clarity and testing.

The important boundary is conceptual:

> Strategy decides direction; sizing decides quantity.

---

# 26. Order Generation

The portfolio/order-generation layer converts a target intent into a required
trade.

Examples:

```text
currently flat
+ LONG signal
→ BUY order
```

```text
currently long
+ EXIT signal
→ SELL order for held quantity
```

Examples of no-op transitions:

```text
currently long
+ LONG signal
→ no order
```

```text
currently flat
+ EXIT signal
→ no order
```

These transitions must be deterministic and directly testable.

---

# 27. ExecutionHandler

## Responsibility

Own simulated execution assumptions.

The MVP execution handler receives market orders and produces fills according to
the next-available-open rule.

## Required Behavior

```text
Order submitted after completed bar t
        ↓
order remains pending
        ↓
bar t+1 becomes available
        ↓
use bar t+1 open
        ↓
FillEvent
```

## Owned State

The handler may need minimal pending-order state.

Because the MVP supports only one instrument and simple market orders, this
state should remain small.

## Invariants

- order cannot fill before submission;
- order cannot use the same completed bar's open after observing its close;
- valid pending market order fills at the next available open;
- fill quantity equals order quantity;
- execution is deterministic.

---

# 28. End-of-Data Execution Policy

A specific edge case must be defined before Phase 3 is completed:

```text
signal/order generated on final available bar
        ↓
no next bar exists
```

The recommended MVP policy is:

> The order remains unfilled and the backtest ends without synthesizing an
> execution price.

This should be confirmed during implementation and covered by a deterministic
test.

Do not invent a final close fill unless the MVP scope is explicitly changed.

---

# 29. Portfolio Valuation

Portfolio valuation is distinct from execution.

The portfolio must record equity using information that is valid at the
simulated point in time.

Conceptually:

```text
cash
+
position quantity × observable valuation price
=
equity
```

The exact daily snapshot timing must be fixed during Phase 3.

A recommended direction is to maintain one consistent end-of-step valuation
convention after all events valid for that simulation step have been processed.

The convention must be:

- explicit;
- deterministic;
- compatible with the next-open execution model;
- used consistently by performance calculations.

---

# 30. PortfolioSnapshot

A small value type is recommended for portfolio history.

Conceptually:

```text
timestamp
cash
position quantity
market value
equity
```

Potential additional fields should be added only if required by outputs or
validation.

Snapshots should represent observed portfolio state, not expose mutable
portfolio internals.

---

# 31. Performance

## Responsibility

Calculate performance statistics from completed portfolio/equity history.

## Inputs

- portfolio snapshots and/or equity series.

## Outputs

MVP metrics:

- total return;
- annualized return;
- annualized volatility;
- Sharpe ratio;
- maximum drawdown.

## Design Principle

Performance should be largely pure/stateless where practical.

Quantitative functions should be easy to test independently with known values.

---

# 32. Return Series

A dedicated return-series calculation may exist either as:

- a helper function;
- a performance utility;
- a small value transformation.

Do not create a large analytics hierarchy.

The exact return convention must be documented and tested.

---

# 33. Benchmark

The benchmark should reuse the same data assumptions and compatible performance
conventions.

Where possible, benchmark calculation should remain outside the event engine
unless there is a concrete reason to run it as a separate strategy instance.

The simplest correct comparison should be preferred.

---

# 34. Reporting

## Responsibility

Serialize and visualize completed backtest outputs.

## C++ Responsibilities

Likely:

- write result CSV files;
- write portfolio/equity history;
- write performance summary.

## Python Responsibilities

Likely:

- load exported results;
- generate plots;
- compare strategy and benchmark visually.

## Must Not

Reporting must not modify simulation state.

---

# 35. CLI and Configuration

The MVP requires a minimal entry point.

Conceptually:

```text
CLI arguments / configuration
        ↓
Backtest configuration
        ↓
construct components
        ↓
run engine
        ↓
export results
```

Likely configuration fields include:

- canonical dataset path;
- strategy selection;
- initial capital;
- strategy parameters;
- output directory.

Do not build a generic configuration subsystem unless the MVP requires it.

---

# 36. Dependency Direction

The intended dependency direction is:

```text
Domain Value Types
        ↑
Data / Events / Portfolio / Execution / Strategy
        ↑
BacktestEngine
        ↑
CLI / Application Wiring
```

Performance and reporting depend on completed results but should not become
dependencies of trading logic.

Provider-specific Python code is outside the C++ domain dependency graph.

---

# 37. Domain Types

The project should introduce explicit domain types where they materially reduce
ambiguity.

Possible examples:

```text
Timestamp
Symbol
Quantity
Price
Cash
```

However, strong wrappers should be added selectively.

Do not create extensive type machinery unless it improves correctness,
readability, or unit safety.

---

# 38. Enumerations

Likely explicit enums include:

```text
SignalType
    LONG
    EXIT

OrderSide
    BUY
    SELL

OrderType
    MARKET
```

Use `enum class` rather than unscoped enums.

Do not add post-MVP enum values solely for future completeness.

---

# 39. Ownership Model

Prefer simple ownership.

General direction:

- immutable/value-like events;
- value-like domain objects;
- components constructed by application wiring;
- references or pointers used only when lifetime is obvious;
- no global mutable state.

The `BacktestEngine` may own its major components or hold non-owning references
to externally constructed components.

The final choice should optimize clarity and testability.

Ownership must be obvious from the public API.

---

# 40. Error Model

Errors should be explicit at system boundaries.

Typical categories:

```text
configuration error
data acquisition / normalization error
CSV parsing error
invalid canonical data
invalid strategy configuration
violated runtime invariant
output/reporting error
```

For recoverable C++ operations, `std::expected` may be appropriate.

Exceptions may still be appropriate where they make ownership and control flow
clearer.

The project should not force a single error mechanism everywhere.

---

# 41. Testing Architecture

Testing should mirror component boundaries.

```text
Unit Tests
    ↓
Component Integration Tests
    ↓
Engine Integration Tests
    ↓
Manual-Oracle End-to-End Test
```

---

# 42. Unit-Test Targets

Examples:

## Data

- `Bar` validation where applicable;
- CSV parsing;
- timestamp parsing;
- invalid-row behavior.

## Events

- event construction;
- event type dispatch.

## Strategy

- Buy-and-Hold signal behavior;
- MA warm-up;
- crossover transitions.

## Portfolio

- signal-to-order transitions;
- fill accounting;
- equity calculation.

## Execution

- next-open fill timing;
- final-bar unfilled order;
- fill price and quantity.

## Performance

- each metric against known values.

---

# 43. Integration Tests

Important integration boundaries include:

```text
CsvBarReader
→ HistoricalDataFeed
```

```text
HistoricalDataFeed
→ BacktestEngine
→ MarketEvent
```

```text
MarketEvent
→ Strategy
→ SignalEvent
```

```text
SignalEvent
→ Portfolio
→ OrderEvent
```

```text
OrderEvent
→ ExecutionHandler
→ FillEvent
```

```text
FillEvent
→ Portfolio
```

The most important integration test is the complete manual-oracle backtest.

---

# 44. Manual-Oracle Architecture

The end-to-end deterministic fixture should be isolated from real provider data.

Recommended structure:

```text
tests/
└── fixtures/
    └── manual_oracle/
        ├── bars.csv
        └── expected_results...
```

The exact file layout may change.

The important principle is:

> The oracle must remain small enough that every expected transition can be
> independently verified.

---

# 45. Proposed Repository Structure

The exact repository may evolve, but the initial structure should remain small.

```text
event-driven-backtester/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
│
├── docs/
│   ├── MVP-scope.md
│   ├── project-principles.md
│   ├── references.md
│   ├── development-guide.md
│   ├── knowledge-guide.md
│   └── architecture.md
│
├── include/
│   └── backtester/
│       ├── data/
│       ├── events/
│       ├── strategy/
│       ├── portfolio/
│       ├── execution/
│       ├── performance/
│       └── engine/
│
├── src/
│   ├── data/
│   ├── strategy/
│   ├── portfolio/
│   ├── execution/
│   ├── performance/
│   ├── engine/
│   └── main.cpp
│
├── tests/
│   ├── unit/
│   ├── integration/
│   └── fixtures/
│
├── python/
│   ├── data/
│   ├── analysis/
│   └── tests/
│
├── data/
│   ├── raw/
│   └── canonical/
│
└── output/
```

This is a starting point, not a requirement to create empty directories before
they are needed.

---

# 46. Header and Source Organization

Do not create one class merely to match every directory.

Create files only when a real component exists.

Potential early files may include:

```text
include/backtester/data/bar.hpp
include/backtester/data/csv_bar_reader.hpp
include/backtester/data/historical_data_feed.hpp

include/backtester/events/event.hpp

include/backtester/strategy/strategy.hpp
include/backtester/strategy/buy_and_hold.hpp
include/backtester/strategy/moving_average_crossover.hpp

include/backtester/portfolio/portfolio.hpp

include/backtester/execution/execution_handler.hpp

include/backtester/performance/performance.hpp

include/backtester/engine/backtest_engine.hpp
```

Exact names may change during implementation.

Avoid premature file proliferation.

---

# 47. Namespace

Use a single top-level namespace.

Recommended:

```cpp
namespace backtester {
    ...
}
```

Nested namespaces may mirror meaningful domains:

```cpp
backtester::data
backtester::strategy
backtester::execution
```

Only introduce them if they improve clarity.

Do not create deep namespace hierarchies.

---

# 48. CMake Architecture

Prefer target-based CMake.

Likely high-level targets:

```text
backtester_core
backtester_cli
backtester_tests
```

A single core library target is preferable to many small libraries for the MVP.

Additional targets should be introduced only when there is a concrete build
boundary.

---

# 49. External C++ Dependencies

Keep dependencies minimal.

Current required testing dependency:

- Catch2 v3.

The core engine should rely primarily on the C++ standard library.

Avoid adding:

- CSV frameworks;
- date libraries;
- logging frameworks;
- CLI frameworks;

unless implementation shows that the standard-library solution would materially
reduce quality or waste significant development effort.

---

# 50. Python Architecture

Python is intentionally outside the simulation core.

Suggested responsibilities:

## Data

```text
download
→ preserve raw data
→ normalize
→ validate
→ write canonical CSV
```

## Analysis

```text
read backtest outputs
→ generate plots
→ optional external validation
```

## Tests

Python-side tests should validate provider-specific and normalization behavior.

C++ tests validate the canonical-domain and simulation behavior.

---

# 51. Data Directory Policy

Conceptually:

```text
data/raw/
```

contains provider-originated data preserved for traceability.

```text
data/canonical/
```

contains normalized provider-independent datasets.

Large datasets should generally not be committed.

Small deterministic fixtures used by tests should be committed under `tests/`.

---

# 52. Simulation State Ownership Summary

| State | Owner |
|---|---|
| historical dataset / cursor | `HistoricalDataFeed` |
| pending events | event queue / engine |
| strategy-specific state | `Strategy` |
| pending execution state | `ExecutionHandler` |
| cash | `Portfolio` |
| position quantity | `Portfolio` |
| portfolio valuation history | `Portfolio` |
| completed performance results | performance/reporting layer |

Every mutable state item should have one obvious owner.

---

# 53. Responsibility Summary

| Component | Responsibility | Must Not |
|---|---|---|
| `CsvBarReader` | Parse canonical data | Know provider details |
| `HistoricalDataFeed` | Reveal bars sequentially | Generate signals |
| `Strategy` | Generate trading intent | Modify portfolio |
| `PositionSizer` | Determine quantity | Choose execution price |
| `Portfolio` | Own cash/positions/equity | Acquire data |
| `ExecutionHandler` | Apply fill assumptions | Generate strategy intent |
| `BacktestEngine` | Coordinate event flow | Absorb domain logic |
| `Performance` | Evaluate results | Affect trading decisions |
| `Reporting` | Serialize/visualize results | Alter simulation |

---

# 54. Important MVP Invariants

The following invariants should shape implementation and tests.

## Data

- bars are valid;
- bars are chronologically ordered;
- future bars are inaccessible through normal simulation behavior.

## Events

- each event has explicit meaning;
- event processing is deterministic;
- events are not processed more than once.

## Strategy

- signals use only currently available information;
- strategy does not mutate portfolio state.

## Orders

- an order exists before its fill;
- quantity and side are explicit.

## Execution

- next-open rule is enforced;
- same-bar look-ahead execution is impossible;
- fill price is observable at the fill time.

## Portfolio

- holdings change only after fills;
- cash changes consistently with fills;
- equity equals cash plus marked position value.

## Performance

- metrics are derived from completed portfolio history;
- formula conventions are explicit;
- deterministic known-result tests exist.

---

# 55. Architecture Decisions Deferred Until Implementation

The following choices should **not** be frozen prematurely.

## Event Payload Ownership

Whether `MarketEvent` owns a `Bar`, contains a reference, or identifies current
feed state.

Choose when implementing Phase 2.

## Strategy Polymorphism

Whether strategies use:

- runtime polymorphism;
- templates;
- variants;
- another minimal approach.

Choose the simplest design needed by CLI/runtime strategy selection.

## Engine Component Ownership

Whether the engine owns components directly or receives references.

Choose based on testability and lifetime clarity.

## PositionSizer Shape

Separate component versus isolated portfolio policy.

Choose when implementing Phase 3.

## Exact Portfolio Snapshot Timing

Finalize with the manual oracle and temporal model.

## Error Propagation Style

Use the simplest consistent approach for each boundary.

These are implementation decisions, not missing project planning.

---

# 56. Architectural Decisions Already Fixed

The following decisions are fixed for the MVP unless a correctness issue is
found:

- C++23 core;
- Python for acquisition/normalization/visualization;
- one instrument;
- daily OHLCV;
- provider-independent canonical dataset;
- event-driven sequential engine;
- explicit signal/order/fill distinction;
- long/flat trading behavior;
- signed position representation;
- market orders;
- next-available-open execution;
- deterministic full fills;
- no concurrency;
- strategy separated from execution;
- portfolio separated from strategy;
- performance separated from trading logic.

---

# 57. Phase-to-Architecture Mapping

## Phase 1

Build:

```text
Bar
CsvBarReader
HistoricalDataFeed
```

## Phase 2

Build:

```text
Event types
Event queue
BacktestEngine
```

## Phase 3

Build:

```text
Strategy
BuyAndHoldStrategy
PositionSizer / sizing policy
Portfolio
ExecutionHandler
complete event flow
manual oracle
```

## Phase 4

Build:

```text
MovingAverage
MovingAverageCrossoverStrategy
Performance
Benchmark
```

## Phase 5

Build:

```text
CLI
Configuration
Reporting
Python plots
final integration
```

This mapping is intentionally aligned with `development-guide.md`.

---

# 58. Architecture Update Rule

Update this document when implementation changes a durable technical decision.

Examples:

- component ownership changes;
- an interface changes materially;
- event semantics change;
- the temporal processing order changes;
- a new invariant is introduced;
- a deferred architectural choice is resolved.

Do not update it for trivial refactors.

When code and this document disagree, resolve the discrepancy quickly.

The architecture document should describe the system that actually exists, not
an abandoned design.

---

# 59. Initial Implementation Boundary

Once the core documents are frozen, the first implementation task should be:

```text
Define the canonical Bar contract
        ↓
implement Bar
        ↓
create deterministic fixtures
        ↓
implement canonical CSV parsing
        ↓
validate parser behavior
```

This begins Phase 1 without requiring the rest of the architecture to be
implemented first.

The engine should grow from validated foundations rather than from skeleton
classes.

---

# 60. Final Architectural Principle

The architecture should preserve the causal structure of the trading system:

```text
Market Information
        ↓
Strategy Intent
        ↓
Order
        ↓
Execution
        ↓
Fill
        ↓
Portfolio State
        ↓
Performance
```

Each stage should have:

- a clear responsibility;
- explicit inputs and outputs;
- obvious state ownership;
- deterministic behavior;
- testable invariants.

The architecture is successful if the MVP remains:

```text
small enough to finish
+
clear enough to defend
+
correct enough to trust
+
structured enough to extend
```

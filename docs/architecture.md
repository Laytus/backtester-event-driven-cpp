# Architecture

> **Purpose**
>
> This document defines the evolving technical architecture of the Event-Driven Backtester.
>
> It translates the conceptual model and MVP scope into concrete software components, responsibilities, ownership rules, interfaces, invariants, and dependencies.
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

- deterministic
- sequential
- testable
- reproducible
- easy to explain
- small enough to understand completely
- extensible through clean component boundaries

The architecture should avoid both extremes:

```text
monolithic backtester
        ← avoid →

speculative framework for future features
```

The target is:

> The smallest architecture that correctly represents the complete event-driven trading lifecycle required by the MVP.

---

# 2. System Context

The complete project contains two main technical environments.

```text
Python Data Tooling
        │
        │ yfinance acquisition
        ▼
Raw Provider Dataset + Acquisition Metadata
        │
        │ provider validation / symbol selection / normalization
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

Python supports the backtester by owning provider-specific acquisition, normalization, persistence, and later analysis/visualization.

C++ owns the provider-independent simulation core.

The durable boundary between the two environments is the canonical CSV contract.

---

# 3. High-Level Architecture

The implemented system through Phase 3 is organized around a deterministic daily-session lifecycle.

The data path is:

```text
Canonical Dataset
        ↓
CsvBarReader
        ↓
HistoricalDataFeed
        ↓
BacktestEngine
```

Inside `BacktestEngine`, one active historical session is processed causally as:

```text
previous pending OrderEvent
        ↓
current session Open
        ↓
ExecutionHandler
        ↓
optional FillEvent
        ↓
Portfolio.apply_fill()

completed Bar
        ↓
MarketEvent
        ↓
Strategy
        ↓
optional SignalEvent
        ↓
PositionSizer
        ↓
target position
        ↓
OrderGenerator
        ↓
optional OrderEvent
        ↓
ExecutionHandler pending state

current session Close
        ↓
Portfolio.mark_to_market()
        ↓
PortfolioSnapshot
```

Later phases consume completed portfolio history through:

```text
Portfolio snapshots / equity history
        ↓
Performance
        ↓
Reporting
```

`BacktestEngine` coordinates the temporal flow.

It must not absorb the domain responsibilities of Strategy, Portfolio, sizing, order generation, execution, performance, or reporting.

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

They do not necessarily require separate libraries or complex package boundaries.

---

# 5. Data Infrastructure

The Phase 1 data architecture is implemented as two explicit layers separated by the canonical CSV contract.

## Python provider layer

Implemented components:

- `acquisition.py`
- `validation.py`
- `normalization.py`
- `persistence.py`
- `pipeline.py`
- `scripts/build_daily_data.py`

Implemented flow:

```text
Yahoo Finance
        ↓
yf.download(...)
        ↓
raw provider DataFrame
        ↓
raw CSV + acquisition metadata persisted
        ↓
provider validation
        ↓
single-symbol selection
        ↓
normalization
        ↓
canonical validation
        ↓
canonical CSV
```

Raw provider data are preserved before provider validation so failed acquisitions or structurally invalid provider responses remain inspectable.

## Canonical boundary

The exact MVP canonical CSV schema is:

```text
session_date,symbol,open,high,low,close,volume
```

The canonical representation is provider-independent.

## C++ data layer

Implemented components:

- `Bar`
- `CsvBarReader`
- `HistoricalDataFeed`

Implemented flow:

```text
Canonical CSV
        ↓
CsvBarReader::read(...)
        ↓
std::vector<Bar>
        ↓
HistoricalDataFeed
        ↓
sequential observable bars
```

Provider-specific acquisition and normalization do not enter the C++ domain dependency graph.

---

# 6. Bar

## Responsibility

Represent one canonical daily OHLCV observation.

## Implemented Data Contract

The market domain defines:

```cpp
using SessionDate = std::chrono::year_month_day;
```

`Bar` uses:

- `market::SessionDate` session date
- `std::string` symbol
- `double` open
- `double` high
- `double` low
- `double` close
- `std::uint64_t` volume

The session date represents the exchange-local daily trading session as a calendar date. The MVP does not attach an intraday timestamp to a daily bar.

## Invariants

Construction rejects a bar unless:

- the calendar date is valid
- the symbol is non-empty
- all prices are finite
- all prices are strictly positive
- `high >= open`
- `high >= close`
- `high >= low`
- `low <= open`
- `low <= close`

Volume is unsigned and zero volume is allowed.

The interface is logically immutable after successful construction: state is private, access is read-only, and no setters are exposed.

## Ownership

`Bar` is a value type.

It does not own external resources.

---

# 7. CsvBarReader

## Responsibility

Parse the provider-independent canonical CSV format into validated `Bar` objects.

## Implemented Interface

The reader exposes a stateless class-level operation equivalent to:

```cpp
static std::vector<market::Bar> read(
    const std::filesystem::path& path
);
```

## Input Contract

The CSV header must be exactly:

```text
session_date,symbol,open,high,low,close,volume
```

Rows must preserve the canonical representation established by the Python data pipeline.

## Output

A successfully parsed file produces:

```text
std::vector<Bar>
```

in file order.

## Validation Responsibilities

The reader rejects, with explicit contextual errors:

- files that cannot be opened
- invalid headers
- wrong column counts
- invalid calendar dates
- missing required values
- malformed numeric values
- invalid `Bar` invariants
- duplicate session dates
- non-monotonic session dates
- mixed symbols in one canonical dataset

Errors include file/line context where applicable.

The reader does not sort, repair, fill, interpolate, or otherwise alter ambiguous input.

## Must Not

- download data
- know about `yfinance`
- rename provider-specific columns
- implement strategy logic
- expose future-data semantics

---

# 8. HistoricalDataFeed

## Responsibility

Reveal validated historical bars sequentially as simulated time advances.

## Implemented Interface

The feed owns its historical data by value:

```cpp
explicit HistoricalDataFeed(std::vector<market::Bar> bars);

[[nodiscard]]
std::optional<market::Bar> next();
```

## Owned State

The implemented state is:

- `std::vector<Bar>` historical storage
- a `std::size_t` cursor identifying the next unread bar

The constructor receives the vector by value and moves it into owned storage.

## Public Behavior

`next()` returns the next bar exactly once.

After the final bar, it returns:

```cpp
std::nullopt
```

Repeated calls after exhaustion continue returning `std::nullopt`.

There is no public random-access API and no public method exposing future bars.

## Invariants

- input order is preserved
- each bar is exposed at most once
- no bar is skipped by normal sequential use
- future bars cannot be retrieved through arbitrary indexing
- end-of-data behavior is deterministic

## Must Not

- generate strategy signals
- modify portfolio state
- determine execution
- calculate performance

---

# 9. Event Protocol

The MVP contains four explicit domain events:

```text
MarketEvent
SignalEvent
OrderEvent
FillEvent
```

They represent distinct stages in the causal trading protocol:

```text
market information
        ↓
strategy intent
        ↓
trading instruction
        ↓
execution
```

The event model is explicit, type-safe, and value-oriented.

Backtesting and future live trading should preserve the same domain-event semantics where practical. Infrastructure adapters may differ, but the meaning of the events should not.

---

# 10. Event Representation

The event protocol uses a closed set of value-like event types:

```cpp
using Event = std::variant<
    MarketEvent,
    SignalEvent,
    OrderEvent,
    FillEvent
>;
```

Dispatch uses:

```cpp
std::visit(...)
```

This is a fixed MVP architectural decision unless implementation reveals a concrete correctness or extensibility problem.

## Why This Fits the Architecture

- the event set is small and explicitly known
- events are value-like messages
- dispatch remains explicit and type-safe
- ownership is simple
- no inheritance hierarchy or mandatory heap allocation is required
- deterministic behavior is straightforward to test

Runtime-polymorphic event hierarchies should not be introduced without a concrete requirement for dynamically open event types.

---

# 11. MarketEvent

## Meaning

A completed market observation has become available to downstream simulation components.

For the MVP, that observation is one completed daily `Bar`.

## Producer

During backtesting:

- `BacktestEngine`, using the next `Bar` delivered by `HistoricalDataFeed`

A future live implementation may produce equivalent market events from a live market-data adapter.

## Consumer

During the implemented Phase 3 lifecycle:

- `Strategy`

Portfolio valuation does not consume `MarketEvent` directly. The engine uses the active session close to invoke Portfolio mark-to-market after the complete event chain for that session has finished.

## Payload and Ownership

`MarketEvent` owns one `Bar` by value.

It does not reference `HistoricalDataFeed` storage or depend on feed internals.

This keeps event lifetime and ownership explicit and avoids dangling-reference or mutable-shared-state concerns.

## Timestamp Meaning

`Bar::session_date()` identifies the completed trading session represented by the event.

For the daily MVP, this is a session date rather than a precise exchange or wall-clock timestamp.

---

# 12. SignalEvent

## Meaning

The strategy expresses a desired market exposure based only on currently observable information.

A signal represents trading intent, not an execution instruction.

## Producer

- `Strategy`

## Consumer

During the implemented Phase 3 lifecycle:

```text
SignalEvent
        ↓
PositionSizer
        ↓
target position
        ↓
OrderGenerator
        ↓
optional OrderEvent
```

The Portfolio is inspected read-only by sizing logic but is not mutated by a signal.

## Direction Semantics

The durable protocol uses target-exposure semantics:

```text
Long
Short
Flat
```

Conceptually:

```cpp
enum class SignalDirection {
    Long,
    Short,
    Flat
};
```

`Short` is part of the durable event protocol even though short-selling behavior remains outside the current MVP.

No explicit `Hold` signal is required. The absence of a new `SignalEvent` represents no change in desired exposure.

## Payload

Conceptually:

```text
symbol
direction
signal_date
```

`signal_date` identifies the market session whose observable information caused the signal.

## Must Not Contain

- execution price
- fill price
- hidden Portfolio mutation
- assumed trade result

---

# 13. OrderEvent

## Meaning

A concrete trading instruction has been created from strategy intent, target sizing, and current executed Portfolio state.

An order represents an instruction to attempt execution. It does not imply that execution has already occurred.

## Producer

- `OrderGenerator`

`PositionSizer` determines the target position.

`OrderGenerator` compares that target with the current executed position and produces the required transition order, if any.

## Consumer

During backtesting:

- `ExecutionHandler`

A future live implementation may route equivalent orders through a broker adapter.

## Required MVP Data

Conceptually:

```text
symbol
side
quantity
order_type
submission_date
```

Required sides:

```text
Buy
Sell
```

Required MVP order type:

```text
Market
```

Additional order types should be added only when corresponding execution semantics are implemented.

## Temporal Meaning

`submission_date` identifies the session after whose observable information the order was created.

A market order created from completed session `t` is not eligible for execution at `Open(t)`.

## Invariants

- quantity is a positive unsigned magnitude
- side is explicit
- order type is `Market` in the MVP
- creating an `OrderEvent` does not modify executed Portfolio holdings
- the order may remain pending across the boundary between two sessions

---

# 14. FillEvent

## Meaning

A previously submitted order has been simulated or externally confirmed as executed.

A fill represents an execution fact.

## Producer

During backtesting:

- `ExecutionHandler`.

A future live implementation may produce equivalent fill information from a broker or execution adapter.

## Consumer

- `Portfolio`.

## Payload

The durable core payload is conceptually:

```text
symbol
side
quantity
fill_price
fill_date
fees
```

For the MVP:

```text
fees = 0
```

is valid.

Fees or commissions are execution facts and therefore belong naturally in the fill representation.

## Slippage

Slippage is not a fundamental `FillEvent` field.

It is derived by comparing the actual fill price with a chosen reference price, such as an arrival price, decision price, midpoint, or expected execution price.

Execution-quality metrics such as slippage and implementation shortfall belong to later analytics rather than to the core fill fact.

## Invariant

A fill represents executed quantity.

Portfolio holdings and cash change only as a consequence of fill processing.

---

# 15. Event Queue

## Responsibility

Store domain events that have been produced but not yet processed.

`EventQueue` defines deterministic FIFO dispatch order.

## Implemented Interface

The queue exposes:

```cpp
void push(Event event);

[[nodiscard]]
std::optional<Event> pop();

[[nodiscard]]
bool empty() const noexcept;
```

Its implemented storage is:

```cpp
std::queue<Event>
```

The queue owns pending `Event` values.

Copy construction and copy assignment are disabled so one pending event stream cannot be duplicated accidentally.

Move construction and move assignment are allowed so ownership of the pending stream can be transferred.

## Ordering

The queue is FIFO and preserves insertion order exactly.

An empty `pop()` returns:

```cpp
std::nullopt
```

Empty-queue behavior is expected control flow rather than an exception.

## Invariants

- deterministic FIFO retrieval
- insertion order is preserved
- no implicit parallel dispatch
- each queued event can be removed at most once
- empty state is explicit and deterministic

## Important Principle

`EventQueue` is a **dispatch queue**, not a market-time scheduler.

It must not reorder events according to timestamps.

Future scheduling concerns such as timers, delayed actions, or explicitly future-dated events belong to a separate scheduler abstraction if one becomes necessary.

## Live-Trading Direction

The same queue semantics can remain valid in a future live system even if the underlying transport implementation changes.

A later implementation may require a thread-safe queue, MPSC queue, ring buffer, or other low-latency transport, but such changes should be driven by concrete concurrency and performance requirements rather than introduced during the MVP.

---

# 16. BacktestEngine

## Responsibility

Coordinate the complete deterministic simulation lifecycle.

The engine is the system orchestrator and must not absorb domain logic owned by Strategy, Portfolio, position sizing, order generation, or execution.

## Implemented Phase 3 Interface

The current engine is constructed from:

```cpp
BacktestEngine(
    data::HistoricalDataFeed data_feed,
    std::unique_ptr<strategy::Strategy> strategy,
    portfolio::Portfolio portfolio,
    portfolio::PositionSizer position_sizer
);
```

It exposes:

```cpp
[[nodiscard]]
std::optional<events::Event> step();

[[nodiscard]]
bool finished() const noexcept;

[[nodiscard]]
const portfolio::Portfolio& portfolio() const noexcept;
```

The engine is non-copyable and movable.

A null Strategy is invalid construction.

## Ownership

`BacktestEngine` owns the runtime state required for one simulation:

```text
HistoricalDataFeed
EventQueue
std::unique_ptr<Strategy>
Portfolio
PositionSizer
OrderGenerator
ExecutionHandler
optional active Bar
feed-exhaustion state
```

Strategy uses exclusive polymorphic ownership through `std::unique_ptr<Strategy>`.

Portfolio, PositionSizer, OrderGenerator, ExecutionHandler, HistoricalDataFeed, and EventQueue are owned by the engine.

Portfolio is exposed only through a const reference.

## Active Session

The engine may hold one `Bar` as active session context while several observable events derived from that session are processed.

The active Bar provides:

```text
session date
Open(t)  → execution orchestration
Close(t) → sizing and valuation
```

ExecutionHandler does not receive the complete Bar.

For next-open execution it receives only:

```text
session date
open price
current Portfolio
```

This prevents execution logic from observing the same session's High, Low, or Close.

## Session Lifecycle

For one session `t`:

```text
load Bar(t)
        ↓
attempt previous pending order at Open(t)
        ↓
optional FillEvent(t)
        ↓
queue MarketEvent(t)
        ↓
process queued events one at a time
```

Concrete event dispatch is:

```text
MarketEvent
→ Strategy::on_market()
→ append zero or more SignalEvents

SignalEvent
→ PositionSizer using active Close(t)
→ OrderGenerator
→ append optional OrderEvent

OrderEvent
→ ExecutionHandler::submit()

FillEvent
→ Portfolio::apply_fill()
```

When all events generated from the active session have been processed and `EventQueue` is empty:

```text
Portfolio.mark_to_market(session_date, Close(t))
        ↓
one PortfolioSnapshot(t)
        ↓
clear active session
```

A pending order stored by `ExecutionHandler` may remain after the active session closes because it belongs to a future execution opportunity.

## `step()` Semantics

`step()` processes and returns at most one observable `Event` per call.

A returned event has already been processed internally by the engine before `step()` returns.

Therefore:

```text
returned FillEvent
→ Portfolio already reflects the fill

returned MarketEvent
→ Strategy has already processed it

returned SignalEvent
→ sizing and order generation have already run

returned OrderEvent
→ ExecutionHandler has already accepted it
```

Events generated by processing the current event remain queued for later `step()` calls.

The engine processes all queued work for the active session before requesting another historical Bar.

## Event Dispatch Safety

The closed `Event` variant is dispatched with `std::visit`.

The generic fallback uses a dependent compile-time `static_assert`, so adding a new event alternative without extending engine dispatch fails at compile time.

## Data-Feed Exhaustion

Historical-data exhaustion is observed only when:

```cpp
data_feed_.next()
```

returns:

```cpp
std::nullopt
```

Processing the final valid Bar does not by itself mark the feed exhausted.

If an order remains pending when exhaustion is observed, the engine discards it without creating a synthetic fill.

## Completion

The engine is finished only when:

```text
historical-data exhaustion has been observed
AND
EventQueue is empty
AND
no active session remains
AND
ExecutionHandler has no pending order
```

Repeated calls after completion return no additional events and produce no additional financial state changes.

## `run()` Boundary

A separate `run()` method is not required for the current architecture.

The complete simulation can already be driven with repeated `step()` calls while preserving event-by-event observability.

A future `run()` may be added as a convenience wrapper around the same semantics rather than as a separate execution path.

---

# 17. Engine Responsibilities

The engine owns coordination.

It may:

- advance `HistoricalDataFeed`
- maintain one active historical session
- attempt previous pending execution at the current session open
- create `MarketEvent` from the completed active Bar
- enqueue and dispatch observable events deterministically
- invoke Strategy, PositionSizer, OrderGenerator, ExecutionHandler, and Portfolio at the correct causal points
- mark Portfolio to market at the completed session close
- create exactly one Portfolio snapshot per completed session through Portfolio valuation
- observe feed exhaustion
- discard a terminal pending order when no future execution opportunity exists
- terminate the simulation

The engine should not:

- implement strategy rules
- calculate target-position formulas itself
- calculate order deltas itself
- mutate Portfolio accounting except by dispatching `FillEvent`
- determine execution approval or fill quantity itself
- choose execution rules
- calculate performance formulas

The engine validates composition invariants that exist only when otherwise valid components are combined.

For the single-asset MVP, active market-data symbol and Portfolio symbol must agree.

The engine also preserves these ordering guarantees:

```text
previous pending execution
→ before current completed MarketEvent

all current-session event work
→ before loading next historical Bar

session close valuation
→ after current-session event chain completes
```

---

# 18. Strategy Interface

## Responsibility

Consume observable market information and produce zero or more target-exposure signals.

The implemented runtime-polymorphic contract is conceptually:

```cpp
class Strategy {
public:
    Strategy() = default;
    virtual ~Strategy() = default;

    Strategy(const Strategy&) = delete;
    Strategy& operator=(const Strategy&) = delete;
    Strategy(Strategy&&) = default;
    Strategy& operator=(Strategy&&) = default;

    [[nodiscard]]
    virtual std::vector<events::SignalEvent>
    on_market(const events::MarketEvent& event) = 0;
};
```

Returning an empty vector means no target-exposure change is requested.

Strategy objects are non-copyable and movable so mutable strategy state is not accidentally duplicated.

## Polymorphism

Runtime polymorphism is implemented intentionally because concrete strategies form an extensible set while `BacktestEngine` depends on one stable Strategy interface.

The engine owns the Strategy through:

```cpp
std::unique_ptr<Strategy>
```

## Strategy-Owned State

Examples:

- entered/not-entered state for Buy-and-Hold
- future moving-average rolling state
- future regime/signal state

## Must Not Own

- cash
- executed position quantity
- pending execution state
- execution price selection

Strategy must not mutate Portfolio, create fills, or access future Bars.

---

# 19. BuyAndHoldStrategy

## Responsibility

Generate one initial `Long` target-exposure signal for its configured instrument.

## Configuration and State

The strategy stores:

```text
configured symbol
has_signaled_entry
```

The configured symbol must be non-empty.

## Behavior

```text
first matching MarketEvent
→ SignalEvent(Long)

later matching MarketEvents
→ no new signal

unrelated MarketEvent
→ no signal
→ entry opportunity remains available
```

The emitted signal preserves:

```text
configured symbol
triggering session date
SignalDirection::Long
```

Buy-and-Hold does not determine quantity, order side, execution price, or execution timing.

## Invariants

- at most one `Long` entry signal is emitted for the configured symbol
- unrelated symbols do not consume the entry opportunity
- identical event sequences produce identical behavior
- the strategy never mutates Portfolio or execution state

---

# 20. MovingAverageCrossoverStrategy

## Responsibility

Generate `Long` / `Flat` signals based on the relationship between short and long moving averages.

## Owned State

Potentially:

- short-window rolling data
- long-window rolling data
- current strategy regime
- warm-up state

## Configuration

- short window
- long window

## Invariants

- no signal before required warm-up data are available
- short window must be valid relative to long window
- transitions are deterministic
- duplicate unintended signals are avoided

The exact crossover convention must be documented before implementation.

---

# 21. Moving Average Utility

A minimal reusable moving-average primitive may be introduced if it clearly simplifies strategy implementation.

## Responsibility

Maintain or calculate a rolling arithmetic mean.

## Non-Goal

Do not build a generic technical-analysis indicator framework.

Only implement what the MVP strategy needs.

---

# 22. Portfolio

## Responsibility

Own executed financial state and deterministic end-of-session valuation history.

Portfolio reflects what has actually been executed.

It does not decide what should be traded.

## Implemented State

For the single-instrument MVP:

```text
configured symbol
initial capital
current cash
current signed position quantity
latest mark price
current market value
current equity
chronological PortfolioSnapshot history
```

The configured symbol is non-empty.

Initial capital is finite and strictly positive.

## Main Inputs

Portfolio state changes through two explicit operations:

```text
FillEvent
→ apply_fill()
→ executed cash and position change
```

and:

```text
session date + close mark
→ mark_to_market()
→ market value + equity + PortfolioSnapshot
```

Signals and orders do not mutate Portfolio.

## Main Outputs

Portfolio exposes read-only current state and snapshot history.

It does not produce `OrderEvent`.

Order generation is owned by `OrderGenerator` after `PositionSizer` determines the target position.

---

# 23. Portfolio Responsibilities

Portfolio should:

- own initial capital and current cash
- own executed signed position quantity
- apply valid `FillEvent` accounting
- calculate market value from the current mark
- calculate current equity
- record chronological `PortfolioSnapshot` values
- expose current executed state read-only for sizing and execution checks
- reject fills for another symbol
- protect signed position arithmetic from overflow and underflow
- calculate and validate candidate state before committing a multi-field mutation

Portfolio should not:

- acquire market data
- generate strategy signals
- determine target allocation
- generate orders
- choose execution price
- perform buying-power approval
- calculate final performance statistics

Only `FillEvent` changes executed cash and position.

Mark-to-market changes valuation state and snapshot history, not executed holdings.

---

# 24. Position Representation

The implemented Portfolio position representation is deliberately small and signed:

```cpp
using PositionQuantity = std::int64_t;
```

Semantics:

```text
position_quantity > 0 → long
position_quantity = 0 → flat
position_quantity < 0 → short representation
```

Order and fill quantities remain unsigned magnitudes combined with an explicit side:

```text
OrderSide::Buy  + std::uint64_t quantity
OrderSide::Sell + std::uint64_t quantity
```

The MVP target-generation policy does not create short targets, but the Portfolio representation can encode future short positions without redesign.

Conversions and arithmetic across signed Portfolio quantities and unsigned order/fill quantities must be range-checked before mutation.

A generic multi-position container is intentionally outside the single-instrument MVP.

---

# 25. PositionSizer

## Responsibility

Convert target-exposure intent into a deterministic target position quantity using current executed Portfolio state and an observable reference price.

Conceptually:

```text
SignalEvent
+ current Portfolio state
+ observable reference price
+ configured target allocation
        ↓
target position
```

## Implemented Policy

`PositionSizer` owns a configured target allocation satisfying:

```text
0 < target_allocation <= 1
```

At signal time, reference equity is calculated from current executed state:

```text
reference equity
=
cash
+
current position × reference price
```

For Phase 3, the observable reference price is the close of the completed active session.

For `Long`:

```text
target
=
floor(target_allocation × reference equity / reference price)
```

For `Flat`:

```text
target = 0
```

`Short` remains part of the event protocol but negative target generation is rejected by the long-only MVP policy.

## Boundary

PositionSizer:

- reads Portfolio through a const reference
- does not mutate Portfolio
- does not know the future execution price
- does not perform execution-time buying-power approval
- does not create a fill

The target quantity is desired state, not a guarantee that the complete requested transition remains executable at the next open.

---

# 26. Order Generation

`OrderGenerator` converts the target position into the concrete trade required to move from current executed state toward that target.

The implemented rule is:

```text
delta
=
target position
-
current position
```

Then:

```text
delta > 0
→ Buy abs(delta)

delta < 0
→ Sell abs(delta)

delta = 0
→ no OrderEvent
```

The generated order preserves:

```text
symbol          = originating SignalEvent symbol
order_type      = Market
submission_date = originating SignalEvent date
```

Examples:

```text
current = 0
target  = 10
→ Buy 10
```

```text
current = 9
target  = 0
→ Sell 9
```

```text
current = target
→ no order
```

OrderGenerator is stateless.

It does not mutate Portfolio, choose an execution price, apply buying-power rules, or create fills.

Safe signed arithmetic is required when computing the distance between current and target positions.

---

# 27. ExecutionHandler

## Responsibility

Own the MVP pending-order state and simulate deterministic next-open execution.

## Implemented Interface

Conceptually:

```cpp
[[nodiscard]]
bool has_pending_order() const noexcept;

void submit(events::OrderEvent order);

[[nodiscard]]
std::optional<events::FillEvent> execute_on_open(
    market::SessionDate session_date,
    double open_price,
    const portfolio::Portfolio& portfolio
);

void discard_pending() noexcept;
```

The handler is non-copyable and movable.

## Pending-Order State

The MVP owns at most one pending `OrderEvent`.

Submitting a second order while another remains pending is invalid.

The pending lifecycle is:

```text
submit
→ pending
→ first valid later session open
→ one execution attempt
→ pending state cleared
```

A same-session open is not eligible and preserves the pending order.

An execution date earlier than submission is invalid and preserves pending state.

## Execution Price

Only market orders are supported.

The execution price is:

```text
current eligible session open
```

The execution API receives only session date, open price, and current Portfolio.

It does not receive the complete Bar and therefore cannot inspect the same session's High, Low, or Close.

## Buy-Side Buying Power

For a Buy order:

```text
available cash = max(current cash, 0)
maximum affordable quantity
=
floor(available cash / open price)
```

Approved quantity is:

```text
min(requested quantity, maximum affordable quantity)
```

If approved quantity is positive, one `FillEvent` is produced for exactly the approved quantity.

If approved quantity is zero, no fill is produced.

In both cases, the valid execution attempt consumes the complete pending order.

No residual quantity remains pending.

## Sell-Side Execution

Sell orders execute their complete requested quantity.

The long-only target-generation policy is responsible for avoiding short-producing targets.

ExecutionHandler does not reinterpret strategy intent.

## Fill Semantics

A successful execution produces:

```text
symbol     = pending order symbol
side       = pending order side
quantity   = approved quantity
fill_price = current open price
fill_date  = current session date
fees       = 0
```

ExecutionHandler does not apply the fill to Portfolio.

Portfolio mutation occurs only when `BacktestEngine` later dispatches that `FillEvent`.

## Validation and Exception Safety

Invalid session dates, invalid open prices, symbol mismatch, or execution dates earlier than submission do not consume the pending order.

A valid result is constructed before pending state is cleared.

This preserves pending state if fill construction itself fails.

## Invariants

- at most one order is pending
- only market orders are supported
- same-session execution is impossible
- an eligible later open provides one execution attempt
- Buy approval never exceeds requested quantity
- Buy approval never exceeds cash-only buying power
- Sell approval equals requested quantity
- filled quantity equals approved quantity
- zero-approved Buy produces no fill
- every valid eligible attempt clears the complete pending order
- no residual order quantity is retained
- fees are zero in the MVP
- ExecutionHandler never mutates Portfolio
- ExecutionHandler never observes High, Low, or Close at execution time

---

# 28. End-of-Data Execution Policy

An order created from the final completed market session may remain pending after that session has been valued.

The implemented policy is:

```text
final completed MarketEvent
        ↓
optional SignalEvent
        ↓
optional OrderEvent
        ↓
pending order
        ↓
HistoricalDataFeed::next()
returns std::nullopt
        ↓
feed exhaustion is now observed
        ↓
ExecutionHandler::discard_pending()
        ↓
no FillEvent
```

The engine must not:

- fabricate a future session
- execute at the final close
- replay the final Bar
- invent an execution price

The final pending order is discarded only after feed exhaustion has actually been observed.

A pending order by itself therefore does not mean the simulation is permanently blocked at end-of-data.

---

# 29. Portfolio Valuation

Portfolio valuation is distinct from execution accounting.

Execution uses the actual fill price.

End-of-session valuation uses the active session close.

For the single-instrument MVP:

```text
market value
=
signed position quantity × session close
```

and:

```text
equity
=
cash + market value
```

The engine invokes mark-to-market exactly once after the complete event chain for the active session has been processed and `EventQueue` is empty.

Therefore the ordering is:

```text
optional open FillEvent
→ fill accounting
→ completed MarketEvent
→ optional SignalEvent
→ optional OrderEvent for a future session
→ Close(t) mark-to-market
→ PortfolioSnapshot(t)
```

A newly created pending order does not change current cash or holdings and therefore does not alter the current session snapshot except through already-executed state.

The valuation convention is deterministic and is the basis for later performance calculations.

---

# 30. PortfolioSnapshot

`PortfolioSnapshot` is an implemented value type representing end-of-session Portfolio state.

Its minimum payload is:

```text
session_date
cash
position_quantity
market_value
equity
```

Exactly one snapshot is recorded for each completed historical session when `BacktestEngine` closes the active session.

No artificial pre-session snapshot is created before the first completed market session.

Snapshots are historical values.

Later mutations of the live Portfolio do not modify already recorded snapshots.

Snapshot dates must be strictly chronological.

The snapshot history is the deterministic source for later equity-curve and performance analysis.

---

# 31. Performance

## Responsibility

Calculate performance statistics from completed portfolio/equity history.

## Inputs

- portfolio snapshots and/or equity series.

## Outputs

MVP metrics:

- total return
- annualized return
- annualized volatility
- Sharpe ratio
- maximum drawdown

## Design Principle

Performance should be largely pure/stateless where practical.

Quantitative functions should be easy to test independently with known values.

---

# 32. Return Series

A dedicated return-series calculation may exist either as:

- a helper function
- a performance utility
- a small value transformation

Do not create a large analytics hierarchy.

The exact return convention must be documented and tested.

---

# 33. Benchmark

The benchmark should reuse the same data assumptions and compatible performance conventions.

Where possible, benchmark calculation should remain outside the event engine unless there is a concrete reason to run it as a separate strategy instance.

The simplest correct comparison should be preferred.

---

# 34. Reporting

## Responsibility

Serialize and visualize completed backtest outputs.

## C++ Responsibilities

Likely:

- write result CSV files
- write portfolio/equity history
- write performance summary

## Python Responsibilities

Likely:

- load exported results
- generate plots
- compare strategy and benchmark visually

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

- canonical dataset path
- strategy selection
- initial capital
- strategy parameters
- output directory

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

Performance and reporting depend on completed results but should not become dependencies of trading logic.

Provider-specific Python code is outside the C++ domain dependency graph.

---

# 37. Domain Types

The project should introduce explicit domain types where they materially reduce ambiguity.

Possible examples:

```text
Timestamp
Symbol
Quantity
Price
Cash
```

However, strong wrappers should be added selectively.

Do not create extensive type machinery unless it improves correctness, readability, or unit safety.

---

# 38. Enumerations

The event protocol uses scoped enums.

The current direction is:

```text
SignalDirection
    Long
    Short
    Flat

OrderSide
    Buy
    Sell

OrderType
    Market
```

Use `enum class` rather than unscoped enums.

`Short` is intentionally part of the durable signal protocol even though short-selling behavior is outside the current MVP.

Additional order types or execution modes should not be added until their semantics are actually implemented.

---

# 39. Ownership Model

Prefer simple and explicit ownership.

The implemented Phase 3 ownership model is:

- `Bar` is a value type
- `MarketEvent` owns its `Bar` by value
- `Event` is a value-like closed `std::variant`
- `EventQueue` owns pending `Event` values
- `HistoricalDataFeed` owns historical Bars and its cursor state
- `BacktestEngine` owns `HistoricalDataFeed` and `EventQueue`
- `BacktestEngine` owns Strategy exclusively through `std::unique_ptr<Strategy>`
- `BacktestEngine` owns Portfolio, PositionSizer, OrderGenerator, and ExecutionHandler
- `BacktestEngine` owns one optional active Bar as current-session context
- `ExecutionHandler` owns at most one optional pending OrderEvent
- Strategy owns strategy-specific mutable state
- Portfolio owns executed financial state and valuation history
- callers receive read-only Portfolio access from the engine
- state-owning engine, queue, Strategy, Portfolio, and ExecutionHandler types use ownership policies that prevent accidental duplication where required
- no global mutable state is used

Ownership remains obvious from public APIs.

Pointers are used where runtime polymorphic ownership is required.

References are used for non-owning read-only access when lifetime is controlled by the caller.

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

Exceptions may still be appropriate where they make ownership and control flow clearer.

The project should not force a single error mechanism everywhere.

---

# 41. Testing Architecture

Testing mirrors component boundaries and increases in scope:

```text
Unit Tests
    ↓
Component Integration Tests
    ↓
Engine Integration Tests
    ↓
Manual-Oracle End-to-End Test
```

Through Phase 3, all four layers are implemented.

Unit tests protect individual domain contracts.

Integration tests verify that independently validated components compose without changing their meaning.

The manual-oracle test verifies the complete financial path against values calculated independently from the production implementation.

Deterministic repeated-run tests verify that identical inputs and initial state produce identical event and Portfolio histories.

---

# 42. Unit-Test Targets

Implemented and planned unit-test boundaries include:

## Data

- `Bar` validation
- CSV parsing
- session-date parsing
- invalid-row behavior
- chronological dataset validation
- `HistoricalDataFeed` sequential delivery and exhaustion

## Events

- event construction and invariants
- event alternative preservation
- type-safe dispatch
- EventQueue FIFO behavior and ownership semantics

## Strategy

- Buy-and-Hold configuration
- first matching `Long` signal
- no repeated entry signal
- unrelated-symbol behavior
- independent Strategy state
- deterministic repeated behavior
- future MA warm-up and crossover transitions

## Portfolio

- construction invariants
- Buy and Sell fill accounting
- fees
- signed position arithmetic
- symbol validation
- mark-to-market
- PortfolioSnapshot history
- separation between fills and valuation

## Position Sizing and Order Generation

- target-allocation validation
- reference-equity calculation
- `Long` and `Flat` target quantities
- rejection of `Short` targets under MVP policy
- Buy and Sell order deltas
- zero-delta no-order behavior
- signed arithmetic boundaries
- no Portfolio mutation

## Execution

- pending-order lifecycle
- second-order rejection while pending
- same-session non-execution
- earlier-date rejection
- next-open fill timing
- Buy-side buying-power clipping
- zero-approved no-fill behavior
- Sell requested-quantity execution
- fill fields and fees
- pending-state preservation on validation failure
- explicit terminal discard
- no Portfolio mutation

## Engine

- null Strategy rejection
- single-symbol composition invariant
- market-only compatibility with a silent Strategy
- event ordering
- processed-before-return behavior
- exactly one snapshot per completed session
- final pending-order discard
- exhaustion and stable completion
- deterministic repeated runs

## Performance

Phase 4 will add deterministic known-result tests for each required metric.

---

# 43. Integration Tests

Important implemented integration boundaries include:

```text
CsvBarReader
→ HistoricalDataFeed
```

```text
canonical CSV
→ CsvBarReader
→ HistoricalDataFeed
→ BacktestEngine
→ MarketEvent
→ deterministic observable sequence
```

```text
MarketEvent
→ Strategy
→ SignalEvent
→ PositionSizer
→ OrderGenerator
→ OrderEvent
```

```text
previous OrderEvent
→ next eligible Open
→ ExecutionHandler
→ FillEvent
→ Portfolio
```

```text
complete active-session event chain
→ Close valuation
→ PortfolioSnapshot
```

Phase 3 engine integration validates:

- `MarketEvent → SignalEvent → OrderEvent` causal ordering
- pending orders survive the session boundary
- next-session `FillEvent` precedes that session's completed `MarketEvent`
- Buy clipping propagates into Portfolio accounting
- Portfolio is updated before the corresponding `FillEvent` is returned by `step()`
- no later session loads before current-session event work is complete
- each completed session produces exactly one snapshot
- final-session pending orders are discarded without synthetic execution
- complete event and Portfolio histories are deterministic

The manual-oracle end-to-end test then validates the complete financial trajectory independently.

---

# 44. Manual-Oracle Architecture

The Phase 3 end-to-end oracle is intentionally small and independent from real provider data.

It is implemented directly in the deterministic C++ end-to-end test rather than depending on live data or production-generated expected values.

The canonical scenario uses four sessions and a test-only deterministic Strategy:

```text
Session 1 → Long
Session 3 → Flat
otherwise → no signal
```

With:

```text
initial capital = 1,000
target allocation = 100%
fees = 0
slippage = 0
```

the manually calculated path exercises:

```text
Long signal
→ Buy 10 requested
→ upward gap at next Open
→ Buy clipped to 9
→ Portfolio accounting
→ close valuation
→ Flat signal
→ Sell 9 requested
→ next-open Sell 9 fill
→ return to cash
```

The expected result is defined independently:

```text
10 observable Events
4 PortfolioSnapshots
final cash = 1,135
final position = 0
final equity = 1,135
```

The important principle is:

> Expected values are calculated by hand first and are not generated by the production code under test.

This oracle is the principal Phase 3 system-level correctness reference.

---

# 45. Repository Structure

The repository grows only when real components are implemented.

Through Phase 3, the architecture includes these implemented C++ module boundaries:

```text
include/backtester/
├── market/
│   └── bar.hpp
├── data/
│   ├── csv_bar_reader.hpp
│   └── historical_data_feed.hpp
├── events/
│   ├── event.hpp
│   └── event_queue.hpp
├── strategy/
│   └── strategy.hpp
├── portfolio/
│   ├── portfolio.hpp
│   ├── position_sizer.hpp
│   └── order_generator.hpp
├── execution/
│   └── execution_handler.hpp
└── engine/
    └── backtest_engine.hpp
```

The concrete Buy-and-Hold implementation is colocated in the Strategy module.

Corresponding implementation files live under the matching `src/` domains.

Relevant Phase 3 tests include:

```text
tests/cpp/test_phase_3_engine_integration.cpp
tests/cpp/test_phase_3_end_to_end.cpp
```

alongside the existing component unit tests and Phase 1/2 integration tests.

The Python data workflow remains under the provider-specific Python modules established in Phase 1.

Later-phase Performance, Reporting, and CLI files should be created only when those components are implemented.

Do not create empty architecture merely to match a planned tree.

---

# 46. Header and Source Organization

Create files only when a real component exists.

Implemented public Phase 3 architecture includes:

```text
include/backtester/market/bar.hpp

include/backtester/data/csv_bar_reader.hpp
include/backtester/data/historical_data_feed.hpp

include/backtester/events/event.hpp
include/backtester/events/event_queue.hpp

include/backtester/strategy/strategy.hpp

include/backtester/portfolio/portfolio.hpp
include/backtester/portfolio/position_sizer.hpp
include/backtester/portfolio/order_generator.hpp

include/backtester/execution/execution_handler.hpp

include/backtester/engine/backtest_engine.hpp
```

Concrete Strategy implementations are kept inside the Strategy domain.

Corresponding `.cpp` implementation files are kept under matching `src/` domains when an out-of-line implementation is required.

Potential later modules include:

```text
backtester::performance
reporting / export
CLI application wiring
```

Avoid premature file proliferation.

---

# 47. Namespace

Use a single top-level namespace:

```cpp
namespace backtester {
    ...
}
```

Nested namespaces mirror meaningful implemented domains.

Implemented through Phase 3:

```cpp
backtester::market
backtester::data
backtester::events
backtester::strategy
backtester::portfolio
backtester::execution
backtester::engine
```

A later domain may include:

```cpp
backtester::performance
```

Only introduce new namespaces when they improve clarity.

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

Additional targets should be introduced only when there is a concrete build boundary.

---

# 49. External C++ Dependencies

Keep dependencies minimal.

Current required testing dependency:

- Catch2 v3

The core engine should rely primarily on the C++ standard library.

Avoid adding:

- CSV frameworks
- date libraries
- logging frameworks
- CLI frameworks

unless implementation shows that the standard-library solution would materially reduce quality or waste significant development effort.

---

# 50. Python Architecture

Python is intentionally outside the simulation core.

## Implemented Phase 1 Data Modules

### `acquisition.py`

Owns Yahoo Finance acquisition and acquisition metadata construction.

It calls `yf.download()` with explicit MVP parameters and performs no normalization or file writing.

### `validation.py`

Owns provider-boundary and canonical-data validation.

It rejects invalid data and does not repair, sort, fill, interpolate, or fabricate observations.

### `normalization.py`

Owns:

```text
Yahoo MultiIndex representation
→ selected single-symbol provider data
→ canonical daily-bar representation
```

It uses provider `Open`, `High`, `Low`, `Close`, and `Volume`.

`Adj Close` and corporate-action columns are not mapped into canonical OHLCV.

### `persistence.py`

Owns serialization of:

- raw provider CSV
- acquisition metadata JSON
- canonical CSV

Existing artifacts are not overwritten silently.

### `pipeline.py`

Owns orchestration only:

```text
download
→ preserve raw + metadata
→ validate provider data
→ select symbol
→ normalize
→ validate canonical data
→ persist canonical CSV
```

### `scripts/build_daily_data.py`

Provides the thin CLI entry point for building canonical daily data.

## Later Analysis Responsibilities

```text
read backtest outputs
→ generate plots
→ optional external validation
```

## Testing Boundary

Python tests validate provider-specific acquisition, normalization, validation, persistence, and pipeline behavior.

C++ tests validate the canonical-domain representation and simulation behavior.

---

# 51. Data Directory Policy

```text
data/raw/
```

contains provider-originated data preserved for traceability together with acquisition metadata.

```text
data/canonical/
```

contains normalized provider-independent datasets.

```text
data/fixtures/
```

contains small deterministic canonical fixtures used by C++ tests.

Live-generated raw and canonical datasets should generally not be committed.

Deterministic fixtures required by the test suite should be committed.

---

# 52. Simulation State Ownership Summary

| State | Owner |
|---|---|
| historical dataset / cursor | `HistoricalDataFeed` |
| pending observable events | `EventQueue` |
| active session `Bar` | `BacktestEngine` |
| feed-exhaustion state | `BacktestEngine` |
| Strategy polymorphic lifetime | `BacktestEngine` |
| strategy-specific mutable state | concrete `Strategy` |
| target-allocation configuration | `PositionSizer` |
| order-generation logic | stateless `OrderGenerator` |
| pending execution order | `ExecutionHandler` |
| cash | `Portfolio` |
| signed position quantity | `Portfolio` |
| latest mark / market value / equity | `Portfolio` |
| Portfolio snapshot history | `Portfolio` |
| completed performance results | future performance/reporting layer |

Every mutable state item should have one obvious owner.

The active session and pending execution order are deliberately different kinds of state:

```text
active Bar
→ current session still being processed

pending OrderEvent
→ future execution opportunity
```

---

# 53. Responsibility Summary

| Component | Responsibility | Must Not |
|---|---|---|
| Python acquisition | Acquire provider data and metadata | Normalize or simulate |
| Python validation | Enforce provider/canonical data contracts | Repair ambiguous data |
| Python normalization | Map provider representation to canonical representation | Apply hidden price adjustments |
| Python persistence | Serialize raw, metadata, and canonical artifacts | Silently overwrite evidence |
| Python pipeline | Orchestrate the data workflow | Absorb validation or normalization rules |
| `Bar` | Represent one valid canonical daily observation | Know provider details |
| `CsvBarReader` | Parse canonical data | Know provider details |
| `HistoricalDataFeed` | Reveal Bars sequentially | Generate signals |
| `Strategy` | Generate target-exposure intent | Modify Portfolio or execution state |
| `PositionSizer` | Determine target position from observable state | Assume future execution price |
| `OrderGenerator` | Convert current/target position into optional order | Execute or mutate Portfolio |
| `Portfolio` | Own executed financial state and valuation history | Generate orders or choose execution price |
| `ExecutionHandler` | Own pending order and simulate next-open execution | Generate Strategy intent or mutate Portfolio |
| `BacktestEngine` | Coordinate causal event flow and session progression | Absorb domain calculations |
| `Performance` | Evaluate completed results | Affect trading decisions |
| `Reporting` | Serialize/visualize results | Alter simulation |

The engine owns coordination while each domain component owns its own calculation and state transition rules.

---

# 54. Important MVP Invariants

The following invariants shape implementation and tests.

## Data

- Bars are valid
- Bars are chronologically ordered
- future Bars are inaccessible through normal simulation behavior
- one historical session is active at most once

## Events

- each event has explicit meaning
- events are value-like domain messages
- `MarketEvent` owns its `Bar` by value
- event type is preserved through queueing and dispatch
- `EventQueue` preserves FIFO insertion order
- the queue does not independently reorder events by timestamp
- all event processing is deterministic
- each queued event can be removed at most once
- every delivered historical Bar produces one `MarketEvent`
- engine dispatch is exhaustive for the closed `Event` protocol

## Strategy

- Strategy observes only completed `MarketEvent` information
- Strategy cannot inspect future Bars
- Strategy does not mutate Portfolio
- Strategy expresses desired exposure rather than execution instructions
- Buy-and-Hold emits at most one entry `Long` signal for its configured symbol

## Sizing and Orders

- target sizing uses only observable state and reference price
- sizing does not assume the future execution price
- `Long` and `Flat` produce non-negative targets under MVP policy
- `Short` target generation is rejected by the long-only MVP policy
- order quantity and side are explicit
- zero target delta produces no order
- creating an order does not change executed holdings
- a decision generated from completed session `t` cannot execute at `Open(t)`

## Execution

- at most one order is pending
- pending execution state is owned by `ExecutionHandler`
- next-open rule is enforced
- same-session execution is impossible
- execution observes only session date, open price, and current Portfolio
- Buy approval uses execution-time cash and open price
- Buy approval cannot exceed requested quantity or cash-only buying power
- Sell approval equals requested quantity
- filled quantity equals approved quantity
- a valid execution attempt consumes the complete pending order
- clipped quantity is not retried automatically
- zero-approved Buy creates no fill
- no synthetic execution occurs after data exhaustion

## Portfolio

- only `FillEvent` changes executed cash and holdings
- cash changes consistently with fills and fees
- position quantity is signed
- valuation is separate from execution accounting
- equity equals cash plus marked position value
- Portfolio is marked once at each completed session close
- exactly one snapshot is recorded per completed session
- snapshot dates are chronological
- engine-owned Portfolio is externally read-only

## Engine

- pending current-session event work is processed before loading a later Bar
- a prior pending order is resolved before the current session's completed `MarketEvent`
- returned events have already been processed internally
- feed exhaustion is recognized only after `HistoricalDataFeed::next()` returns `std::nullopt`
- completion requires observed exhaustion, empty queue, no active session, and no pending order
- repeated calls after completion do not change observable state
- identical inputs and initial state produce identical event and Portfolio histories

## Performance

- metrics are derived from completed Portfolio history
- formula conventions are explicit
- deterministic known-result tests will protect Phase 4 quantitative calculations

---

# 55. Post-MVP Event-Infrastructure Direction

The current domain protocol should remain compatible with later live-trading infrastructure without requiring that infrastructure during the MVP.

The expected long-term direction is:

```text
HistoricalDataFeed        LiveMarketDataAdapter
        │                         │
        └──────────┬──────────────┘
                   ↓
              MarketEvent
                   ↓
               EventQueue
                   ↓
                 Engine
                   ↓
               Strategy
                   ↓
              SignalEvent
                   ↓
        Portfolio / PositionSizer
                   ↓
              OrderEvent
                   ↓
        ┌──────────┴──────────┐
        ↓                     ↓
ExecutionHandler         Broker Adapter
        ↓                     ↓
        └──────────┬──────────┘
                   ↓
                FillEvent
                   ↓
                Portfolio
```

Potential future infrastructure includes:

- `EventEnvelope` for transport metadata such as source, event time, receive time, sequence number, and correlation identifiers
- a scheduler for future-timed events and timers
- thread-safe or low-latency queue implementations when concurrency or measured performance requirements justify them

These are non-MVP architectural directions, not current implementation requirements.

---

# 56. Architecture Decisions Deferred Until Implementation

Phase 3 resolved several choices that were previously deferred:

```text
Strategy polymorphism
→ runtime-polymorphic Strategy interface
→ exclusive ownership through std::unique_ptr<Strategy>

PositionSizer shape
→ dedicated PositionSizer component

Order generation
→ dedicated stateless OrderGenerator

Portfolio snapshot timing
→ exactly once per completed session after the event chain
→ valuation uses the current session close

next-open execution input boundary
→ session date + open price + const Portfolio
```

These are no longer open architectural questions.

The following choices should still not be frozen prematurely.

## Performance Internal Shape

Phase 4 should use the smallest structure that cleanly supports the required return series, metrics, benchmark comparison, and deterministic known-result tests.

Do not design a generic analytics framework before those requirements are implemented.

## Reporting and CLI Shape

Output/export and application wiring should remain minimal until Phase 5 makes their exact needs concrete.

## Error Propagation Style

Use the simplest consistent mechanism for each boundary.

The project does not require one universal error mechanism if exceptions or another explicit approach provide clearer ownership and control flow.

Remaining deferred choices are implementation decisions, not missing project planning.

---

# 57. Architectural Decisions Already Fixed

The following decisions are fixed for the MVP unless a correctness issue is found:

- C++23 simulation core
- Python for acquisition, normalization, and later visualization
- one instrument
- daily OHLCV
- provider-independent canonical dataset
- canonical CSV schema: `session_date,symbol,open,high,low,close,volume`
- `session_date` is an exchange-local daily session date represented as `YYYY-MM-DD`, not an intraday timestamp
- canonical OHLC uses provider `Open`, `High`, `Low`, and `Close`
- `Adj Close` is not used as canonical close
- no manual second split adjustment or dividend adjustment is applied in normalization
- canonical volume must be non-negative and integer-valued
- raw provider output is preserved separately from canonical data
- live/provider artifacts are not used as permanent deterministic fixtures
- event-driven sequential engine
- four explicit domain events: `MarketEvent`, `SignalEvent`, `OrderEvent`, and `FillEvent`
- event values are represented as a closed `std::variant`
- event dispatch uses type-safe visitation with exhaustive compile-time fallback
- `MarketEvent` owns its `Bar` by value
- signal direction uses `Long`, `Short`, and `Flat` target-exposure semantics
- Strategy uses runtime polymorphism
- `BacktestEngine` owns Strategy through `std::unique_ptr<Strategy>`
- Buy-and-Hold emits one initial `Long` target and then remains silent
- signed Portfolio position representation uses `std::int64_t`
- order and fill quantities are unsigned magnitudes with explicit side
- `PositionSizer` is a dedicated component
- target allocation is configuration
- sizing uses current executed Portfolio state and an observable close reference
- `OrderGenerator` is a dedicated stateless component
- target minus current position determines the order transition
- `FillEvent` represents execution facts and includes fees
- slippage is derived execution analysis rather than a fundamental fill field
- `EventQueue` is deterministic FIFO dispatch infrastructure
- scheduling and dispatch queueing are separate responsibilities
- `BacktestEngine` owns `HistoricalDataFeed`, `EventQueue`, Portfolio, PositionSizer, OrderGenerator, and ExecutionHandler
- `BacktestEngine` owns one optional active Bar for current-session context
- `BacktestEngine` is non-copyable and movable
- `BacktestEngine::step()` is the incremental observable event boundary
- returned events are processed internally before being returned
- current-session event work completes before the next historical Bar is loaded
- single-symbol market-data and Portfolio composition must agree
- market orders only
- at most one pending order
- pending order state belongs to `ExecutionHandler`
- next-available-open execution
- same-session execution is impossible
- Buy execution is clipped to execution-time cash-only buying power when necessary
- Sell execution uses requested quantity
- filled quantity equals approved quantity
- no residual order remains after the single eligible execution attempt
- fees are zero in the execution MVP
- no slippage or liquidity model in the MVP
- only `FillEvent` changes executed cash and holdings
- Portfolio valuation uses the completed session close
- exactly one PortfolioSnapshot is recorded per completed session
- final pending orders are discarded only after feed exhaustion is observed
- engine completion requires observed feed exhaustion, empty EventQueue, no active session, and no pending execution order
- no concurrency in the MVP
- strategy is separated from execution
- Portfolio is separated from Strategy
- performance is separated from trading logic
- deterministic identical inputs produce identical observable events and Portfolio histories

---

# 58. Phase-to-Architecture Mapping

## Phase 1

Implemented:

```text
Yahoo acquisition workflow
raw-data preservation + metadata
provider validation
provider-specific normalization
canonical validation
canonical CSV persistence
Bar
CsvBarReader
HistoricalDataFeed
CsvBarReader → HistoricalDataFeed integration
```

## Phase 2

Implemented:

```text
MarketEvent
SignalEvent
OrderEvent
FillEvent
Event = std::variant<...>
EventQueue
minimal BacktestEngine
incremental step() boundary
canonical CSV → CsvBarReader → HistoricalDataFeed → BacktestEngine integration
deterministic repeated-run validation
```

Phase 2 established the deterministic event protocol and market-only engine foundation.

## Phase 3

Implemented:

```text
frozen temporal and financial contracts
Strategy interface
BuyAndHoldStrategy
Portfolio
PortfolioSnapshot
signed PositionQuantity
PositionSizer
OrderGenerator
ExecutionHandler
single pending market order
next-available-open execution
execution-time Buy clipping
fill-driven accounting
daily close mark-to-market
integrated BacktestEngine lifecycle
processed-before-return step() semantics
active-session state
terminal pending-order discard
manual financial oracle
end-to-end deterministic validation
```

Phase 3 turns the event foundation into a functioning deterministic trading and Portfolio simulation.

## Phase 4

Build:

```text
MovingAverage
MovingAverageCrossoverStrategy
return series
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

# 59. Architecture Update Rule

Update this document when implementation changes a durable technical decision.

Examples:

- component ownership changes
- an interface changes materially
- event semantics change
- the temporal processing order changes
- a new invariant is introduced
- a deferred architectural choice is resolved

Do not update it for trivial refactors.

When code and this document disagree, resolve the discrepancy quickly.

The architecture document should describe the system that actually exists, not an abandoned design.

---

# 60. Phase 1 Implementation Outcome

Phase 1 established and validated the complete historical-data boundary required by the simulation.

The implemented path is:

```text
Yahoo Finance
        ↓
Python acquisition
        ↓
raw CSV + metadata
        ↓
provider validation
        ↓
provider-specific normalization
        ↓
canonical validation
        ↓
canonical CSV
        ↓
CsvBarReader
        ↓
std::vector<Bar>
        ↓
HistoricalDataFeed
        ↓
sequential Bar delivery
```

The implementation was validated at multiple levels:

- deterministic Python unit and pipeline tests
- deterministic C++ unit tests
- a C++ integration test for `CsvBarReader → HistoricalDataFeed`
- a live Yahoo Finance smoke test
- a real producer-to-consumer smoke test where a Python-generated canonical CSV was successfully consumed by the actual C++ `CsvBarReader`
- Debug and Release C++ test execution

No additional production glue component was required between `CsvBarReader` and `HistoricalDataFeed`.

This trusted data foundation now underpins the implemented Phase 2 event-driven engine.

---

# 61. Phase 2 Implementation Outcome

Phase 2 established and validated the deterministic event-driven engine foundation.

The implemented path is:

```text
canonical CSV
        ↓
CsvBarReader
        ↓
HistoricalDataFeed
        ↓
BacktestEngine
        ↓
MarketEvent
        ↓
EventQueue
        ↓
BacktestEngine::step()
        ↓
observable Event
```

The implementation was validated at multiple levels:

- explicit construction and invariant tests for all four domain event types
- `Event` alternative preservation and type-safe visitation tests
- deterministic FIFO `EventQueue` tests
- copy/move contract checks for state-owning queue and engine types
- `BacktestEngine` tests for sequential market-data access, event production, exhaustion, and stable completion
- a canonical CSV → engine integration test using real production components
- repeated-run validation proving identical observable output for identical canonical input
- Debug and Release C++ test execution

The completed Phase 2 architecture provides:

- four explicit domain event types
- a closed `std::variant` event representation
- value-based event ownership
- deterministic FIFO event transport
- exactly one `MarketEvent` per delivered `Bar`
- queue-first engine progression
- no arbitrary future-data access through the simulation path
- explicit observed feed exhaustion
- deterministic engine completion

Strategy, Portfolio, order-generation behavior, and execution remain intentionally outside Phase 2.

---

# 62. Phase 3 Implementation Outcome

Phase 3 established and validated the complete deterministic trading and Portfolio lifecycle.

The implemented causal path is:

```text
previous pending OrderEvent
        ↓
current session Open
        ↓
ExecutionHandler
        ↓
optional FillEvent
        ↓
Portfolio.apply_fill()

completed MarketEvent
        ↓
Strategy
        ↓
optional SignalEvent
        ↓
PositionSizer
        ↓
target position
        ↓
OrderGenerator
        ↓
optional OrderEvent
        ↓
ExecutionHandler pending state

current session Close
        ↓
Portfolio.mark_to_market()
        ↓
PortfolioSnapshot
```

The implementation was validated at multiple levels:

- Strategy contract and Buy-and-Hold behavior
- signed Portfolio accounting and valuation
- target-position sizing from observable state
- deterministic order generation
- next-open ExecutionHandler behavior
- execution-time Buy-side cash clipping
- complete `BacktestEngine` integration
- exact event ordering across session boundaries
- exactly one Portfolio snapshot per completed session
- final-session pending-order discard without synthetic execution
- stable completion after observed feed exhaustion
- deterministic repeated-run event and Portfolio histories
- an independent four-session manual financial oracle
- Debug and Release C++ test execution

The canonical end-to-end oracle validates:

```text
initial cash = 1,000
Long signal
→ Buy 10 requested
→ Buy 9 filled at next Open after upward-gap clipping
→ invested Portfolio valuation
→ Flat signal
→ Sell 9 requested
→ Sell 9 filled at next Open
→ final cash = 1,135
→ final position = 0
→ final equity = 1,135
```

The exact system-level result is:

```text
10 observable Events
4 PortfolioSnapshots
147 / 147 tests passed in Debug
147 / 147 tests passed in Release
```

No production-code change was required to make the manual oracle pass.

The Phase 3 architecture therefore has deterministic evidence that its temporal model, event ordering, execution path, accounting, valuation, and termination semantics compose correctly.

---

# 63. Final Architectural Principle

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

- a clear responsibility
- explicit inputs and outputs
- obvious state ownership
- deterministic behavior
- testable invariants

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
